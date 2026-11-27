//! Keeping the dictionary in a file without ever losing it: a file that does not load is never
//! written over, and a save replaces the file whole or not at all.

use std::fs::{self, File, OpenOptions};
use std::io::{self, BufReader, BufWriter, ErrorKind};
use std::path::{Path, PathBuf};
use std::sync::Arc;

use super::Dictionary;
use crate::{Error, Personal, Table};

/// The file a dictionary is kept in, and what it still owes that file.
pub(super) struct Disk {
    path: PathBuf,
    /// Why the file did not load. While set, nothing is written to `path`.
    broken: Option<Error>,
    saving: bool,
    /// The file is behind the counts.
    dirty: bool,
    /// Runs learned since the last save reached the file, learned again on top of a reloaded file.
    pending: Vec<Vec<Option<String>>>,
}

impl Disk {
    pub(super) fn learned(&mut self, run: &[Option<&str>]) {
        self.pending
            .push(run.iter().map(|w| w.map(str::to_string)).collect());
        self.dirty = true;
    }

    /// The counts were replaced: what was learned is gone, and the file must follow.
    pub(super) fn replaced(&mut self) {
        self.pending.clear();
        self.dirty = true;
    }
}

/// A save taken from the dictionary's counts at one moment. [`SaveJob::write`] needs no access to
/// the dictionary, so an input method can run it after releasing the lock it keeps the dictionary
/// behind, then hand the result to [`Dictionary::finish_save`].
///
/// A whole save, with the lock held only where the dictionary is passed:
///
/// ```no_run
/// # use std::sync::Mutex;
/// # fn save(dictionary: &Mutex<timsen::Dictionary>) -> std::io::Result<()> {
/// let Some(job) = dictionary.lock().unwrap().begin_save() else { return Ok(()) };
/// let written = job.write();
/// let Some(sync) = dictionary.lock().unwrap().finish_save(job, written)? else { return Ok(()) };
/// let synced = sync.sync();
/// dictionary.lock().unwrap().finish_sync(sync, synced)
/// # }
/// ```
pub struct SaveJob {
    snapshot: Arc<Personal<'static>>,
    path: PathBuf,
    generation: u64,
    /// How many pending runs the snapshot holds.
    pending: usize,
}

impl SaveJob {
    /// Writes the counts beside the file, readable by the user only, and syncs them to the disk.
    /// The file itself changes only in [`Dictionary::finish_save`].
    pub fn write(&self) -> io::Result<()> {
        let tmp = temporary(&self.path);
        match fs::remove_file(&tmp) {
            Err(e) if e.kind() != ErrorKind::NotFound => return Err(e),
            _ => {}
        }
        let mut options = OpenOptions::new();
        options.write(true).create_new(true);
        #[cfg(unix)]
        std::os::unix::fs::OpenOptionsExt::mode(&mut options, 0o600);
        let mut out = BufWriter::new(options.open(&tmp)?);
        self.snapshot.save(&mut out)?;
        out.into_inner()
            .map_err(io::IntoInnerError::into_error)?
            .sync_all()
    }
}

/// A save renamed into place, still to be made to survive a crash. [`SyncJob::sync`] needs no
/// access to the dictionary; a directory sync can take tens of milliseconds on a busy disk, so an
/// input method runs it with the lock released, then hands the result to
/// [`Dictionary::finish_sync`].
#[must_use = "the save runs until finish_sync takes it"]
pub struct SyncJob {
    path: PathBuf,
}

impl SyncJob {
    /// Syncs the directory the file was renamed in.
    pub fn sync(&self) -> io::Result<()> {
        sync_directory(&self.path)
    }
}

/// Where a save is written before it replaces the file: the same directory, so the rename that
/// replaces it cannot cross file systems.
fn temporary(path: &Path) -> PathBuf {
    let mut name = std::ffi::OsString::from(".");
    name.push(path.file_name().unwrap_or_default());
    name.push(".tmp");
    path.with_file_name(name)
}

/// Makes a rename inside `path`'s directory survive a crash.
fn sync_directory(path: &Path) -> io::Result<()> {
    #[cfg(unix)]
    {
        let dir = match path.parent() {
            Some(dir) if !dir.as_os_str().is_empty() => dir,
            _ => Path::new("."),
        };
        File::open(dir)?.sync_all()?;
    }
    let _ = path;
    Ok(())
}

/// Moves `path` to the first free name beside it (`<name>.broken`, `<name>.broken.1`, ...), never
/// replacing a file already there. `None` when there is no file at `path`.
fn set_aside(path: &Path) -> io::Result<Option<PathBuf>> {
    for n in 0u32.. {
        let mut name = path.file_name().unwrap_or_default().to_os_string();
        name.push(if n == 0 {
            ".broken".into()
        } else {
            format!(".broken.{n}")
        });
        let to = path.with_file_name(name);
        // A hard link is never made over an existing name, even when another process races for it.
        match fs::hard_link(path, &to) {
            Ok(()) => return fs::remove_file(path).map(|()| Some(to)),
            Err(e) if e.kind() == ErrorKind::AlreadyExists => continue,
            Err(e) if e.kind() == ErrorKind::NotFound => return Ok(None),
            // A directory, or a file system without hard links.
            Err(_) => {}
        }
        match fs::symlink_metadata(&to) {
            Ok(_) => continue,
            Err(e) if e.kind() != ErrorKind::NotFound => return Err(e),
            Err(_) => {}
        }
        return match fs::rename(path, &to) {
            Ok(()) => Ok(Some(to)),
            Err(e) if e.kind() == ErrorKind::NotFound => Ok(None),
            Err(e) => Err(e),
        };
    }
    unreachable!("names run out after u32::MAX files set aside")
}

/// The file's counts, or `None` when there is no file yet.
fn read(table: &Arc<Table>, path: &Path) -> Result<Option<Personal<'static>>, Error> {
    match File::open(path) {
        Err(e) if e.kind() == ErrorKind::NotFound => Ok(None),
        Err(e) => Err(Error::Io(e)),
        Ok(file) => Personal::load_shared(Arc::clone(table), BufReader::new(file)).map(Some),
    }
}

impl Dictionary {
    /// The dictionary kept in `path`; empty when there is no file yet. A file that does not load
    /// leaves the dictionary empty and stops saving, so the file is never written over:
    /// [`Dictionary::problem`] says why, and [`Dictionary::start_empty`] sets it aside.
    pub fn open(table: Arc<Table>, path: impl Into<PathBuf>) -> Dictionary {
        let path = path.into();
        let mut d = Dictionary::new(table);
        let broken = match read(&d.table, &path) {
            Ok(loaded) => {
                if let Some(p) = loaded {
                    d.current = Arc::new(p);
                }
                None
            }
            Err(e) => Some(e),
        };
        d.disk = Some(Disk {
            path,
            broken,
            saving: false,
            dirty: false,
            pending: Vec::new(),
        });
        d
    }

    /// Why the file did not load, while saving is stopped for it.
    pub fn problem(&self) -> Option<&Error> {
        self.disk.as_ref()?.broken.as_ref()
    }

    /// Whether the file is behind what was learned or cleared.
    pub fn is_dirty(&self) -> bool {
        self.disk.as_ref().is_some_and(|d| d.dirty)
    }

    /// Starts over from an empty dictionary after the file did not load, moving the file to a new
    /// name beside it (`<name>.broken`, then `<name>.broken.1`, ...) so nothing is lost. Returns that
    /// name, or `None` when there was nothing to set aside.
    pub fn start_empty(&mut self) -> io::Result<Option<PathBuf>> {
        let Some(disk) = self.disk.as_mut().filter(|d| d.broken.is_some()) else {
            return Ok(None);
        };
        let moved = set_aside(&disk.path)?;
        sync_directory(&disk.path)?;
        disk.broken = None;
        self.clear();
        Ok(moved)
    }

    /// Reads the file again and learns on top of it the runs not yet saved to it, so what other
    /// fields learned is kept. A file that does not load leaves the counts as they were and stops
    /// saving, as [`Dictionary::open`] does.
    pub fn reload_file(&mut self) -> Result<(), &Error> {
        let Some(disk) = self.disk.as_mut() else {
            return Ok(());
        };
        match read(&self.table, &disk.path) {
            Err(e) => {
                disk.broken = Some(e);
                return Err(disk.broken.as_ref().expect("just set"));
            }
            Ok(loaded) => {
                let mut p = loaded.unwrap_or_else(|| Personal::shared(Arc::clone(&self.table)));
                for run in &disk.pending {
                    let run: Vec<Option<&str>> = run.iter().map(Option::as_deref).collect();
                    p.learn(&run);
                }
                self.current = Arc::new(p);
                disk.broken = None;
                // Never lowered here: a save that may not have reached the disk is still owed.
                disk.dirty |= !disk.pending.is_empty();
            }
        }
        self.generation += 1;
        Ok(())
    }

    /// Takes a save of the counts as they are now. `None` when there is nothing to save, no file,
    /// saving is stopped, or a save is still running: one save runs at a time.
    pub fn begin_save(&mut self) -> Option<SaveJob> {
        let disk = self.disk.as_mut()?;
        if !disk.dirty || disk.saving || disk.broken.is_some() {
            return None;
        }
        disk.saving = true;
        Some(SaveJob {
            snapshot: Arc::clone(&self.current),
            path: disk.path.clone(),
            generation: self.generation,
            pending: disk.pending.len(),
        })
    }

    /// Puts a written save in place of the file, unless a clear or reload came after the save
    /// began: then the save is dropped and the file is still owed one. The rename happens here,
    /// under the caller's lock, so a clear or reload can never come between the check and it.
    ///
    /// Returns the sync still owed when the file was replaced; the save runs, and
    /// [`Dictionary::begin_save`] gives nothing, until [`Dictionary::finish_sync`] takes it.
    pub fn finish_save(
        &mut self,
        job: SaveJob,
        written: io::Result<()>,
    ) -> io::Result<Option<SyncJob>> {
        let tmp = temporary(&job.path);
        let current = self.disk.as_mut().filter(|d| d.path == job.path);
        let Some(disk) = current.filter(|d| d.saving) else {
            let _ = fs::remove_file(&tmp);
            return Ok(None);
        };
        if job.generation != self.generation || disk.broken.is_some() {
            disk.saving = false;
            let _ = fs::remove_file(&tmp);
            return Ok(None);
        }
        if let Err(e) = written.and_then(|()| fs::rename(&tmp, &job.path)) {
            disk.saving = false;
            let _ = fs::remove_file(&tmp);
            return Err(e);
        }
        disk.pending.drain(..job.pending);
        disk.dirty = !disk.pending.is_empty();
        Ok(Some(SyncJob { path: job.path }))
    }

    /// Ends a save with the result of [`SyncJob::sync`]. A failed sync leaves the file owed
    /// another save, since the rename may not survive a crash.
    pub fn finish_sync(&mut self, job: SyncJob, synced: io::Result<()>) -> io::Result<()> {
        let Some(disk) = self.disk.as_mut().filter(|d| d.path == job.path) else {
            return synced;
        };
        disk.saving = false;
        if synced.is_err() {
            disk.dirty = true;
        }
        synced
    }
}
