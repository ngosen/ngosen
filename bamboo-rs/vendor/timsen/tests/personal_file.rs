//! The personal file is never lost: a file that does not load stays byte for byte, a save replaces
//! it whole, and a save begun before a clear never brings the old counts back.

use std::fs;
use std::path::{Path, PathBuf};
use std::sync::Arc;

use timsen::{Dictionary, Table, ToneStyle};

const HEADER: &str = "timsen-personal 1\n";

fn table() -> Arc<Table> {
    let path = concat!(env!("CARGO_MANIFEST_DIR"), "/tests/fixtures/tiny.timsen");
    Arc::new(Table::open(path).unwrap())
}

/// A directory of its own for each test, removed when it ends.
struct Dir(PathBuf);

impl Dir {
    fn new(name: &str) -> Dir {
        let dir = std::env::temp_dir().join(format!("timsen-{name}-{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        fs::create_dir(&dir).unwrap();
        Dir(dir)
    }

    fn file(&self) -> PathBuf {
        self.0.join("personal.txt")
    }

    fn names(&self) -> Vec<String> {
        let mut names: Vec<String> = fs::read_dir(&self.0)
            .unwrap()
            .map(|e| e.unwrap().file_name().into_string().unwrap())
            .collect();
        names.sort();
        names
    }
}

impl Drop for Dir {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.0);
    }
}

/// A save as an input method runs it: only `begin_save`, `finish_save` and `finish_sync` need the
/// dictionary, so writing and syncing can happen with its lock released.
fn save(d: &mut Dictionary) -> bool {
    let job = d.begin_save().expect("a save is owed");
    let written = job.write();
    let Some(sync) = d.finish_save(job, written).unwrap() else {
        return false;
    };
    let synced = sync.sync();
    d.finish_sync(sync, synced).unwrap();
    true
}

fn learn(d: &mut Dictionary, text: &str) {
    let run: Vec<Option<&str>> = text.split(' ').map(Some).collect();
    d.learn(&run);
}

fn restore(d: &Dictionary, run: &[&str]) -> Vec<String> {
    let mut typing = d.typing();
    for s in run {
        typing.push(s);
    }
    typing.finish(ToneStyle::New)
}

fn assert_left_alone(text: &[u8]) {
    let dir = Dir::new(&format!("alone-{}", text.len()));
    fs::write(dir.file(), text).unwrap();
    let mut d = Dictionary::open(table(), dir.file());
    assert!(d.problem().is_some(), "{text:?} should not load");
    learn(&mut d, "ngó sen");
    d.clear();
    assert!(d.begin_save().is_none(), "saving must stop");
    assert!(d.reload_file().is_err());
    assert_eq!(fs::read(dir.file()).unwrap(), text);
    assert_eq!(dir.names(), ["personal.txt"]);
}

#[test]
fn a_file_with_an_unknown_header_is_left_byte_for_byte() {
    assert_left_alone(b"timsen-personal 2\n1\t1\tng\xc3\xb3\n");
}

#[test]
fn a_file_with_a_bad_line_is_left_byte_for_byte() {
    assert_left_alone(b"timsen-personal 1\n1\t1\tng\xc3\xb3\n2\t1\tsen\n");
}

#[test]
fn a_file_that_is_not_text_is_left_byte_for_byte() {
    assert_left_alone(b"timsen-personal 1\n1\t1\t\xff\xfe\n");
}

#[test]
fn a_word_the_table_refuses_stays_in_the_file() {
    let dir = Dir::new("refused");
    let a: Vec<char> = "aàáảãạăằắẳẵặâầấẩẫậ".chars().collect();
    let mut lines: Vec<String> = (a.iter())
        .flat_map(|x| a.iter().map(move |y| format!("1\t1\t{x}{y}\n")))
        .collect();
    lines.sort();
    let text = format!("{HEADER}{}", lines.concat());
    fs::write(dir.file(), &text).unwrap();
    let mut d = Dictionary::open(table(), dir.file());
    assert!(d.problem().is_none());
    d.reload(text.as_bytes()).unwrap();
    assert!(save(&mut d));
    assert_eq!(fs::read_to_string(dir.file()).unwrap(), text);
}

#[test]
fn a_clear_racing_a_save_leaves_an_empty_dictionary_on_disk() {
    let dir = Dir::new("race");
    let mut d = Dictionary::open(table(), dir.file());
    learn(&mut d, "ngó sen");
    assert!(save(&mut d));
    learn(&mut d, "gỗ quý");
    let job = d.begin_save().unwrap();
    let written = job.write();
    d.clear();
    assert!(
        d.finish_save(job, written).unwrap().is_none(),
        "the save began before the clear"
    );
    assert!(d.is_dirty());
    assert!(save(&mut d));
    assert_eq!(fs::read_to_string(dir.file()).unwrap(), HEADER);
    assert_eq!(dir.names(), ["personal.txt"]);
}

#[test]
fn two_fields_learning_then_reloading_keep_both() {
    let dir = Dir::new("two-fields");
    let mut d = Dictionary::open(table(), dir.file());
    let (first, second) = (d.typing(), d.typing());
    // Words the table lacks, so only learning can restore them.
    let (runs, want) = (
        [["ngo", "sen"], ["zed", "sen"]],
        [["ngó", "sen"], ["zèd", "sen"]],
    );
    for (run, want) in runs.iter().zip(&want) {
        assert_ne!(restore(&d, run), *want);
    }
    learn(&mut d, "ngó sen");
    assert!(save(&mut d));
    learn(&mut d, "zèd sen");
    drop((first, second));
    d.reload_file().unwrap();
    for (run, want) in runs.iter().zip(&want) {
        assert_eq!(restore(&d, run), *want);
    }
    assert!(save(&mut d));
    let again = Dictionary::open(table(), dir.file());
    for (run, want) in runs.iter().zip(&want) {
        assert_eq!(restore(&again, run), *want);
    }
    // Each run counted once: replaying a saved run again would double its counts.
    let mut once = Dictionary::new(table());
    learn(&mut once, "ngó sen");
    learn(&mut once, "zèd sen");
    let mut want_file = Vec::new();
    once.save(&mut want_file).unwrap();
    assert_eq!(fs::read(dir.file()).unwrap(), want_file);
}

#[test]
fn one_save_runs_at_a_time() {
    let dir = Dir::new("one-save");
    let mut d = Dictionary::open(table(), dir.file());
    assert!(d.begin_save().is_none(), "nothing to save yet");
    learn(&mut d, "ngó sen");
    let job = d.begin_save().unwrap();
    assert!(d.begin_save().is_none());
    learn(&mut d, "gỗ quý");
    let written = job.write();
    let sync = d.finish_save(job, written).unwrap().unwrap();
    assert!(d.begin_save().is_none(), "the save runs until it is synced");
    let synced = sync.sync();
    d.finish_sync(sync, synced).unwrap();
    assert!(d.is_dirty(), "learned during the save");
    assert!(save(&mut d));
    assert!(!d.is_dirty());
}

#[test]
fn a_failed_write_keeps_the_file_and_the_debt() {
    let dir = Dir::new("failed");
    let mut d = Dictionary::open(table(), dir.file());
    learn(&mut d, "ngó sen");
    assert!(save(&mut d));
    let before = fs::read(dir.file()).unwrap();
    learn(&mut d, "gỗ quý");
    let job = d.begin_save().unwrap();
    let failed = Err(std::io::Error::other("disk full"));
    assert!(d.finish_save(job, failed).is_err());
    assert_eq!(fs::read(dir.file()).unwrap(), before);
    assert!(d.is_dirty());
    assert!(save(&mut d));
    assert_eq!(
        restore(&Dictionary::open(table(), dir.file()), &["go", "quy"]),
        ["gỗ", "quý"]
    );
}

#[test]
fn a_file_broken_since_it_opened_stops_saving_on_reload() {
    let dir = Dir::new("broken-later");
    let mut d = Dictionary::open(table(), dir.file());
    learn(&mut d, "ngó sen");
    assert!(save(&mut d));
    fs::write(dir.file(), "something else\n").unwrap();
    learn(&mut d, "gỗ quý");
    assert!(d.reload_file().is_err());
    assert_eq!(restore(&d, &["ngo", "sen"]), ["ngó", "sen"]);
    assert!(d.begin_save().is_none());
    assert_eq!(fs::read_to_string(dir.file()).unwrap(), "something else\n");
}

#[test]
fn starting_empty_sets_the_file_aside_and_saves_again() {
    let dir = Dir::new("aside");
    for (n, aside) in ["personal.txt.broken", "personal.txt.broken.1"]
        .iter()
        .enumerate()
    {
        let text = format!("not a dictionary {n}\n");
        fs::write(dir.file(), &text).unwrap();
        let mut d = Dictionary::open(table(), dir.file());
        let moved = d.start_empty().unwrap().unwrap();
        assert_eq!(moved, dir.0.join(aside));
        assert_eq!(fs::read_to_string(&moved).unwrap(), text);
        assert!(d.problem().is_none());
        learn(&mut d, "ngó sen");
        assert!(save(&mut d));
        assert_eq!(
            restore(&Dictionary::open(table(), dir.file()), &["ngo", "sen"]),
            ["ngó", "sen"]
        );
    }
}

#[test]
fn a_dictionary_that_loaded_has_nothing_to_set_aside() {
    let dir = Dir::new("nothing-aside");
    let mut d = Dictionary::open(table(), dir.file());
    assert!(d.start_empty().unwrap().is_none());
    assert_eq!(dir.names(), Vec::<String>::new());
}

#[cfg(unix)]
#[test]
fn a_saved_file_is_readable_by_the_user_only() {
    use std::os::unix::fs::PermissionsExt;
    let dir = Dir::new("mode");
    fs::write(dir.file(), HEADER).unwrap();
    fs::set_permissions(dir.file(), fs::Permissions::from_mode(0o644)).unwrap();
    let mut d = Dictionary::open(table(), dir.file());
    learn(&mut d, "ngó sen");
    assert!(save(&mut d));
    let mode = fs::metadata(dir.file()).unwrap().permissions().mode();
    assert_eq!(mode & 0o777, 0o600);
}

#[test]
fn a_file_that_cannot_be_read_stops_saving() {
    let dir = Dir::new("unreadable");
    fs::create_dir(dir.file()).unwrap();
    let mut d = Dictionary::open(table(), dir.file());
    assert!(d.problem().is_some());
    learn(&mut d, "ngó sen");
    assert!(d.begin_save().is_none());
    assert!(Path::new(&dir.file()).is_dir());
    // A directory cannot be hard-linked, so it is renamed aside.
    let moved = d.start_empty().unwrap().unwrap();
    assert!(moved.is_dir());
    assert!(save(&mut d));
}

#[cfg(unix)]
#[test]
fn a_rename_not_synced_still_owes_a_save() {
    use std::os::unix::fs::PermissionsExt;
    let dir = Dir::new("unsynced");
    let mut d = Dictionary::open(table(), dir.file());
    learn(&mut d, "ngó sen");
    // Without read permission the directory cannot be opened to sync it, but renames still work.
    fs::set_permissions(&dir.0, fs::Permissions::from_mode(0o300)).unwrap();
    if fs::File::open(&dir.0).is_ok() {
        fs::set_permissions(&dir.0, fs::Permissions::from_mode(0o700)).unwrap();
        return; // Running as root: permissions do not stop it.
    }
    let job = d.begin_save().unwrap();
    let written = job.write();
    let sync = d.finish_save(job, written).unwrap().expect("renamed");
    let synced = sync.sync();
    fs::set_permissions(&dir.0, fs::Permissions::from_mode(0o700)).unwrap();
    assert!(d.finish_sync(sync, synced).is_err());
    assert!(d.is_dirty(), "the save may not survive a crash");
    d.reload_file().unwrap();
    assert!(
        d.is_dirty(),
        "reading the file back does not make it durable"
    );
    assert!(save(&mut d));
}
