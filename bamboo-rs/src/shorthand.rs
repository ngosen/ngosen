// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

//! The chat abbreviations every field shares, and which of them the user keeps as typed.

use std::collections::BTreeMap;
use std::fs;
use std::io::{self, ErrorKind, Write};
use std::os::unix::fs::{DirBuilderExt, OpenOptionsExt};
use std::path::{Path, PathBuf};
use std::sync::{Mutex, MutexGuard};

use crate::macro_table::MacroTable;

/// Taken back this many times in a row, an abbreviation stays as typed from then on.
const KEEP_AFTER: u8 = 2;

#[derive(Default)]
pub struct Shorthand {
    table: MacroTable,
    kept: Mutex<Kept>,
}

#[derive(Default)]
struct Kept {
    /// The file the counts are kept in, once the front end names one.
    path: Option<PathBuf>,
    learning: bool,
    /// How many times in a row each abbreviation was taken back.
    taken_back: BTreeMap<String, u8>,
}

impl Shorthand {
    pub fn new(table: MacroTable) -> Self {
        Self {
            table,
            kept: Mutex::default(),
        }
    }

    fn kept(&self) -> MutexGuard<'_, Kept> {
        self.kept.lock().unwrap_or_else(|e| e.into_inner())
    }

    /// What `word` stands for, unless the user keeps it as typed.
    pub fn get(&self, word: &str) -> Option<&str> {
        let text = self.table.get(word)?;
        let count = self.kept().taken_back.get(&word.to_lowercase()).copied();
        (count.unwrap_or(0) < KEEP_AFTER).then_some(text)
    }

    /// Whether taking an abbreviation back is counted. Those already kept stay so.
    pub fn set_learning(&self, on: bool) {
        self.kept().learning = on;
    }

    /// Reads the counts in `path` and writes them there from now on.
    pub fn keep_in(&self, path: &Path) {
        let read = fs::read_to_string(path).unwrap_or_default();
        let mut kept = self.kept();
        kept.taken_back = read
            .lines()
            .filter_map(|line| {
                let (key, count) = line.split_once('\t')?;
                let count = count.parse::<u8>().ok()?.min(KEEP_AFTER);
                // The list may have lost the key since the file was written.
                (count > 0 && self.table.get(key).is_some()).then(|| (key.to_lowercase(), count))
            })
            .collect();
        kept.path = Some(path.to_path_buf());
    }

    /// The user took the expansion of `word` back with Backspace.
    pub fn taken_back(&self, word: &str) {
        self.count(word, |n| (n + 1).min(KEEP_AFTER));
    }

    /// The user typed on after the expansion of `word`.
    pub fn accepted(&self, word: &str) {
        // Another field may have kept it since this one expanded it.
        self.count(word, |n| if n < KEEP_AFTER { 0 } else { n });
    }

    fn count(&self, word: &str, next: impl Fn(u8) -> u8) {
        let mut kept = self.kept();
        if !kept.learning || self.table.get(word).is_none() {
            return;
        }
        let key = word.to_lowercase();
        let before = kept.taken_back.get(&key).copied().unwrap_or(0);
        let after = next(before);
        if after == before {
            return;
        }
        if after == 0 {
            kept.taken_back.remove(&key);
        } else {
            kept.taken_back.insert(key, after);
        }
        // A count that could not be written still holds until fcitx5 quits.
        let _ = kept.save();
    }

    /// Every abbreviation with its text and whether the user keeps it as typed, by key.
    pub fn list(&self) -> Vec<(String, String, bool)> {
        let kept = self.kept();
        let mut list: Vec<_> = self
            .table
            .entries()
            .map(|(key, text)| {
                let count = kept.taken_back.get(key).copied().unwrap_or(0);
                (key.to_string(), text.to_string(), count >= KEEP_AFTER)
            })
            .collect();
        list.sort();
        list
    }

    /// Expands `word` again, whatever the learning switch says: the user asked for this one.
    pub fn expand_again(&self, word: &str) -> io::Result<()> {
        let mut kept = self.kept();
        let key = word.to_lowercase();
        let Some(count) = kept.taken_back.remove(&key) else {
            return Ok(());
        };
        let saved = kept.save();
        // Or the file brings the key back as kept after a restart, with nothing left to retry.
        if saved.is_err() {
            kept.taken_back.insert(key, count);
        }
        saved
    }

    /// Forgets every count, in memory and in the file.
    pub fn forget(&self) -> io::Result<()> {
        let mut kept = self.kept();
        kept.taken_back.clear();
        match &kept.path {
            Some(path) => remove_file(path),
            None => Ok(()),
        }
    }
}

impl Kept {
    /// Written on each change: a count changes only when the user takes an expansion back.
    fn save(&self) -> io::Result<()> {
        let Some(path) = &self.path else {
            return Ok(());
        };
        if self.taken_back.is_empty() {
            return remove_file(path);
        }
        if let Some(dir) = path.parent() {
            fs::DirBuilder::new()
                .recursive(true)
                .mode(0o700)
                .create(dir)?;
        }
        let unfinished = unfinished(path);
        let mut file = fs::OpenOptions::new()
            .write(true)
            .create(true)
            .truncate(true)
            .mode(0o600)
            .open(&unfinished)?;
        for (key, count) in &self.taken_back {
            writeln!(file, "{key}\t{count}")?;
        }
        drop(file);
        fs::rename(&unfinished, path)
    }
}

/// Where a save writes before it takes the place of `path`.
fn unfinished(path: &Path) -> PathBuf {
    let mut name = path.as_os_str().to_owned();
    name.push(".tmp");
    PathBuf::from(name)
}

/// Removes the file the counts are kept in and a save of it left half-way; what is not there is
/// already forgotten.
pub fn remove_file(path: &Path) -> io::Result<()> {
    for each in [path, &unfinished(path)] {
        match fs::remove_file(each) {
            Err(e) if e.kind() != ErrorKind::NotFound => return Err(e),
            _ => {}
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A directory of its own, removed when the test ends.
    struct TempDir(PathBuf);

    impl TempDir {
        fn new(name: &str) -> TempDir {
            let dir = std::env::temp_dir().join(format!("ngosen-{name}-{}", std::process::id()));
            let _ = fs::remove_dir_all(&dir);
            TempDir(dir)
        }
    }

    impl Drop for TempDir {
        fn drop(&mut self) {
            let _ = fs::remove_dir_all(&self.0);
        }
    }

    fn shorthand(learning: bool) -> Shorthand {
        let pairs = [("ko", "không"), ("dc", "được")];
        let s = Shorthand::new(MacroTable::new(
            pairs.iter().map(|&(k, v)| (k.to_string(), v.to_string())),
        ));
        s.set_learning(learning);
        s
    }

    #[test]
    fn taken_back_twice_in_a_row_keeps_the_key() {
        let s = shorthand(true);
        s.taken_back("ko");
        assert_eq!(s.get("ko"), Some("không"));
        s.taken_back("Ko");
        assert_eq!(s.get("ko"), None);
        assert_eq!(s.get("Ko"), None);
        assert_eq!(s.get("dc"), Some("được"));
    }

    #[test]
    fn an_expansion_left_in_place_starts_the_count_again() {
        let s = shorthand(true);
        s.taken_back("ko");
        s.accepted("ko");
        s.taken_back("ko");
        assert_eq!(s.get("ko"), Some("không"));
    }

    #[test]
    fn a_kept_key_stays_kept_when_another_field_accepts_it() {
        let s = shorthand(true);
        s.taken_back("ko");
        s.taken_back("ko");
        s.accepted("ko");
        assert_eq!(s.get("ko"), None);
    }

    #[test]
    fn nothing_is_counted_with_learning_off() {
        let s = shorthand(false);
        s.taken_back("ko");
        s.taken_back("ko");
        assert_eq!(s.get("ko"), Some("không"));
    }

    #[test]
    fn counts_come_back_from_the_file_and_forget_removes_it() {
        let dir = TempDir::new("kept-counts");
        let path = dir.0.join("kept");
        let s = shorthand(true);
        s.keep_in(&path);
        s.taken_back("ko");
        s.taken_back("ko");
        s.taken_back("dc");
        assert_eq!(fs::read_to_string(&path).unwrap(), "dc\t1\nko\t2\n");

        let again = shorthand(true);
        again.keep_in(&path);
        assert_eq!(again.get("ko"), None);
        again.taken_back("dc");
        assert_eq!(again.get("dc"), None);

        let unfinished = dir.0.join("kept.tmp");
        fs::write(&unfinished, "ko\t2\n").unwrap();
        again.forget().unwrap();
        assert!(!path.exists());
        assert!(
            !unfinished.exists(),
            "a save left half-way is forgotten too"
        );
        assert_eq!(again.get("ko"), Some("không"));
        again.forget().unwrap();
    }

    #[test]
    fn the_list_marks_the_kept_keys_and_one_can_expand_again() {
        let dir = TempDir::new("kept-list");
        let path = dir.0.join("kept");
        let s = shorthand(true);
        s.keep_in(&path);
        s.taken_back("ko");
        s.taken_back("ko");
        s.taken_back("dc");
        let row = |key: &str, text: &str, kept| (key.to_string(), text.to_string(), kept);
        assert_eq!(
            s.list(),
            [row("dc", "được", false), row("ko", "không", true)]
        );

        s.set_learning(false);
        s.expand_again("Ko").unwrap();
        assert_eq!(s.get("ko"), Some("không"));
        assert_eq!(fs::read_to_string(&path).unwrap(), "dc\t1\n");
        s.expand_again("ko").unwrap();
    }

    #[test]
    fn a_key_that_could_not_be_expanded_again_stays_kept() {
        let dir = TempDir::new("kept-retry");
        let path = dir.0.join("kept");
        let s = shorthand(true);
        s.keep_in(&path);
        s.taken_back("ko");
        s.taken_back("ko");
        s.taken_back("dc");
        fs::create_dir(unfinished(&path)).unwrap();
        assert!(s.expand_again("ko").is_err());
        assert_eq!(s.get("ko"), None);

        fs::remove_dir(unfinished(&path)).unwrap();
        s.expand_again("ko").unwrap();
        assert_eq!(s.get("ko"), Some("không"));
        assert_eq!(fs::read_to_string(&path).unwrap(), "dc\t1\n");
    }

    #[test]
    fn a_file_with_other_lines_gives_only_the_keys_of_the_list() {
        let dir = TempDir::new("kept-lines");
        let path = dir.0.join("kept");
        fs::create_dir_all(&dir.0).unwrap();
        fs::write(&path, "ko\t9\nxyz\t2\ndc\t0\nnoise\nko\n").unwrap();
        let s = shorthand(true);
        s.keep_in(&path);
        assert_eq!(s.get("ko"), None);
        assert_eq!(s.get("dc"), Some("được"));
        s.accepted("dc");
        s.taken_back("dc");
        assert_eq!(fs::read_to_string(&path).unwrap(), "dc\t1\nko\t2\n");
    }

    #[test]
    fn the_file_is_for_its_owner_only() {
        use std::os::unix::fs::PermissionsExt;
        let dir = TempDir::new("kept-mode");
        let path = dir.0.join("kept");
        let s = shorthand(true);
        s.keep_in(&path);
        s.taken_back("ko");
        assert_eq!(
            fs::metadata(&path).unwrap().permissions().mode() & 0o777,
            0o600
        );
    }
}
