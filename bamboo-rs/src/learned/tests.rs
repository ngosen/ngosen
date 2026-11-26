// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

use super::*;
use crate::engine::Learning;
use std::path::PathBuf;
use std::sync::Arc;
use timsen::{Dictionary, Table, ToneStyle};

fn table() -> Arc<Table> {
    Arc::new(Table::from_bytes(include_bytes!("../engine/tiny.timsen").to_vec()).unwrap())
}

/// A directory of its own, removed when the test ends.
struct TempDir(PathBuf);

impl TempDir {
    fn new(name: &str) -> TempDir {
        let dir = std::env::temp_dir().join(format!("ngosen-{name}-{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        fs::create_dir_all(&dir).unwrap();
        TempDir(dir)
    }
}

impl Drop for TempDir {
    fn drop(&mut self) {
        let _ = fs::set_permissions(&self.0, std::os::unix::fs::PermissionsExt::from_mode(0o700));
        let _ = fs::remove_dir_all(&self.0);
    }
}

fn store(dir: &Path) -> DiacriticsStore {
    let path = dir.join("learned");
    DiacriticsStore {
        dictionary: Arc::new(Mutex::new(Dictionary::open(table(), &path))),
        path,
        learning: Arc::new(Learning::new(true)),
        saving: Mutex::default(),
    }
}

fn learn(store: &DiacriticsStore) {
    lock(&store.dictionary).learn(&[Some("ngó"), Some("sen")]);
}

fn restore(store: &DiacriticsStore) -> Vec<String> {
    let personal = lock(&store.dictionary).snapshot();
    personal.restore(&["ngo", "sen"], ToneStyle::New)
}

fn lines(path: &Path) -> usize {
    fs::read_to_string(path).unwrap().lines().count()
}

fn names(dir: &Path) -> Vec<String> {
    let mut names: Vec<String> = (fs::read_dir(dir).unwrap())
        .map(|e| e.unwrap().file_name().to_string_lossy().into_owned())
        .collect();
    names.sort();
    names
}

fn sync(job: &SyncJob) -> io::Result<()> {
    job.sync()
}

#[test]
fn forgetting_empties_the_file_and_the_guesses() {
    let dir = TempDir::new("forget");
    let s = store(&dir.0);
    let table_alone = restore(&s);
    learn(&s);
    assert_ne!(restore(&s), table_alone, "learning changes the guess");
    assert_eq!(s.save_now(sync), Ok(Saved::Written));
    assert!(lines(&s.path) > 1);
    assert_eq!(s.forget(sync), Ok(()));
    assert_eq!(restore(&s), table_alone);
    assert_eq!(lines(&s.path), 1, "only the header is left");
    assert_eq!(s.save_now(sync), Ok(Saved::Unchanged));
}

#[test]
fn forgetting_waits_for_a_save_running() {
    let dir = TempDir::new("forget-saving");
    let s = store(&dir.0);
    learn(&s);
    assert!(crate::ffi::save_diacritics(&s, false));
    assert_eq!(s.forget(sync), Ok(()));
    assert_eq!(lines(&s.path), 1);
    assert!(!lock(&s.dictionary).is_dirty());
}

#[test]
fn forgetting_an_unreadable_file_lets_saves_work_again() {
    let dir = TempDir::new("forget-broken");
    fs::write(dir.0.join("learned"), "not a dictionary\n").unwrap();
    fs::write(dir.0.join("learned.broken.1"), "set aside before\n").unwrap();
    let s = store(&dir.0);
    learn(&s);
    assert!(s.save_now(sync).is_err(), "an unreadable file is a failure");
    assert_eq!(s.forget(sync), Ok(()));
    assert_eq!(names(&dir.0), ["learned"]);
    learn(&s);
    assert_eq!(s.save_now(sync), Ok(Saved::Written));
    assert_eq!(restore(&store(&dir.0)), restore(&s), "the file reads back");
}

#[test]
fn a_save_into_a_read_only_directory_fails_and_keeps_the_file() {
    // SAFETY: geteuid has no preconditions.
    if unsafe { libc::geteuid() } == 0 {
        return; // root writes anyway
    }
    let dir = TempDir::new("save-read-only");
    let s = store(&dir.0);
    learn(&s);
    assert_eq!(s.save_now(sync), Ok(Saved::Written));
    let before = fs::read(&s.path).unwrap();
    lock(&s.dictionary).learn(&[Some("gõ"), Some("phím")]);
    fs::set_permissions(&dir.0, std::os::unix::fs::PermissionsExt::from_mode(0o500)).unwrap();
    assert!(s.save_now(sync).is_err());
    assert!(s.forget(sync).is_err());
    assert_eq!(fs::read(&s.path).unwrap(), before);
    fs::set_permissions(&dir.0, std::os::unix::fs::PermissionsExt::from_mode(0o700)).unwrap();
    assert_eq!(
        s.save_now(sync),
        Ok(Saved::Written),
        "the save is still owed"
    );
}

#[test]
fn a_failed_sync_is_reported_and_the_next_save_retries() {
    let dir = TempDir::new("save-sync");
    let s = store(&dir.0);
    learn(&s);
    let failed = s.save_now(|_| Err(io::Error::other("disk gone")));
    assert!(failed.is_err_and(|e| e.contains("disk gone")));
    assert_eq!(s.save_now(sync), Ok(Saved::Written));
}

#[test]
fn removing_the_files_of_a_dictionary_not_open() {
    let dir = TempDir::new("remove");
    for name in [
        "learned",
        ".learned.tmp",
        "learned.broken",
        "learned.broken.2",
        "learned.brokenx",
        "learned.broken.x",
        "other",
    ] {
        fs::write(dir.0.join(name), "").unwrap();
    }
    assert!(remove_files(&dir.0.join("learned")).is_ok());
    assert_eq!(
        names(&dir.0),
        ["learned.broken.x", "learned.brokenx", "other"]
    );
    assert!(
        remove_files(&dir.0.join("learned")).is_ok(),
        "nothing left to remove"
    );
}
