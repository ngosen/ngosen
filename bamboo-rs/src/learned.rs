// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

//! Saving and forgetting what Đoán dấu learned while the settings window waits, so it can tell the
//! user whether it worked.

use std::fs::{self, File};
use std::io::{self, ErrorKind};
use std::path::Path;
use std::sync::{Mutex, MutexGuard};

use timsen::SyncJob;

use crate::handles::DiacriticsStore;

#[derive(Debug, PartialEq, Eq)]
pub enum Saved {
    Written,
    Unchanged,
}

#[cfg(test)]
mod tests;

fn lock<T>(m: &Mutex<T>) -> MutexGuard<'_, T> {
    m.lock().unwrap_or_else(|e| e.into_inner())
}

impl DiacriticsStore {
    /// Writes what was learned and returns once it is on the disk, a save already running first.
    /// `sync` syncs the directory, as `SyncJob::sync` does.
    pub fn save_now(&self, sync: impl FnOnce(&SyncJob) -> io::Result<()>) -> Result<Saved, String> {
        let mut saving = lock(&self.saving);
        if let Some(running) = saving.take() {
            let _ = running.join();
        }
        self.write(sync)
    }

    /// Forgets everything learned, in memory and in the file, and removes the unreadable files
    /// set aside beside it. Runs open in a field learn nothing and stop guessing with it.
    pub fn forget(&self, sync: impl FnOnce(&SyncJob) -> io::Result<()>) -> Result<(), String> {
        let mut saving = lock(&self.saving);
        if let Some(running) = saving.take() {
            let _ = running.join();
        }
        let set_aside = {
            let mut dictionary = lock(&self.dictionary);
            // An unreadable file stops every save, so it is moved away to save the empty one.
            let set_aside = match dictionary.problem() {
                Some(_) => dictionary.start_empty().map(|_| ()),
                None => Ok(()),
            };
            dictionary.clear();
            self.learning.forgot();
            set_aside
        };
        set_aside.map_err(|e| format!("could not move the unreadable file away: {e}"))?;
        self.write(sync)?;
        remove_set_aside(&self.path)
            .map_err(|e| format!("could not remove an unreadable copy: {e}"))
    }

    fn write(&self, sync: impl FnOnce(&SyncJob) -> io::Result<()>) -> Result<Saved, String> {
        let job = {
            let mut dictionary = lock(&self.dictionary);
            if let Some(problem) = dictionary.problem() {
                return Err(format!("the file could not be read: {problem}"));
            }
            match dictionary.begin_save() {
                Some(job) => job,
                None => return Ok(Saved::Unchanged),
            }
        };
        let written = job.write();
        let job = match lock(&self.dictionary).finish_save(job, written) {
            Ok(Some(job)) => job,
            Ok(None) => return Err("the dictionary was replaced while saving".to_string()),
            Err(e) => return Err(format!("could not write: {e}")),
        };
        let synced = sync(&job);
        (lock(&self.dictionary).finish_sync(job, synced))
            .map_err(|e| format!("could not sync the directory: {e}"))?;
        Ok(Saved::Written)
    }
}

/// Removes the files `start_empty` set aside beside `path` (`<name>.broken`, `<name>.broken.1`,
/// ...) and syncs the directory. Also for when no Đoán dấu is open, with the file itself.
pub fn remove_set_aside(path: &Path) -> io::Result<()> {
    let (Some(dir), Some(name)) = (path.parent(), path.file_name()) else {
        return Ok(());
    };
    let dir = if dir.as_os_str().is_empty() {
        Path::new(".")
    } else {
        dir
    };
    let prefix = format!("{}.broken", name.to_string_lossy());
    let entries = match fs::read_dir(dir) {
        Err(e) if e.kind() == ErrorKind::NotFound => return Ok(()),
        entries => entries?,
    };
    let mut removed = false;
    for entry in entries {
        let entry = entry?;
        let file = entry.file_name();
        let Some(rest) = file.to_str().and_then(|f| f.strip_prefix(&prefix)) else {
            continue;
        };
        let numbered = (rest.strip_prefix('.'))
            .is_some_and(|n| !n.is_empty() && n.bytes().all(|b| b.is_ascii_digit()));
        if rest.is_empty() || numbered {
            match fs::remove_file(entry.path()) {
                Err(e) if e.kind() != ErrorKind::NotFound => return Err(e),
                _ => removed = true,
            }
        }
    }
    if removed {
        File::open(dir)?.sync_all()?;
    }
    Ok(())
}

/// Forgets a file no Đoán dấu has open: the file, the save left half-way beside it, and the copies
/// set aside.
pub fn remove_files(path: &Path) -> io::Result<()> {
    let mut temporary = std::ffi::OsString::from(".");
    temporary.push(path.file_name().unwrap_or_default());
    temporary.push(".tmp");
    for file in [path.to_path_buf(), path.with_file_name(temporary)] {
        match fs::remove_file(&file) {
            Err(e) if e.kind() != ErrorKind::NotFound => return Err(e),
            _ => {}
        }
    }
    remove_set_aside(path)?;
    let dir = path.parent().filter(|d| !d.as_os_str().is_empty());
    match File::open(dir.unwrap_or(Path::new("."))) {
        Err(e) if e.kind() == ErrorKind::NotFound => Ok(()),
        dir => dir?.sync_all(),
    }
}
