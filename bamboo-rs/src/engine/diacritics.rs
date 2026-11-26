// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

//! Đoán dấu for one field: the run of words since it started, guessed by the personal dictionary
//! every field shares, and the changes the front end applies to what the field shows.

use std::sync::{Arc, Mutex, MutexGuard};

use timsen::{Change, Dictionary, FIX_MARGIN, Reach, Run, ToneStyle};

use crate::text::{has_any_vietnamese_rune, without_diacritics};

/// Confidence a raw reading (`test`) needs to replace the interpreted one (`tét`).
const RAW_CONFIDENCE: f64 = 0.99;

/// A change and where it sits in the field: `after` committed words lie between it and the
/// cursor. The front end checks `change.old` there before it replaces anything, and applies the
/// changes before it forwards the key that produced them.
#[derive(Debug)]
pub struct FieldChange {
    pub change: Change,
    pub after: usize,
}

pub struct Diacritics {
    dictionary: Arc<Mutex<Dictionary>>,
    run: Option<Run<'static>>,
    /// The run's words as committed: one typed with marks is known to be right and is learned.
    typed: Vec<String>,
    /// Changes not pulled yet, each with the count of words committed when its part ended.
    changes: Vec<(Change, usize)>,
    /// Words committed since the field last moved its cursor; changes are placed from it.
    words: usize,
    /// The run ended and was learned, and is kept only so the fix of its last word can still be
    /// undone, until the next word.
    over: bool,
    /// How many of the run's words are learned already, all of them once it is over.
    learned: usize,
    /// Words of the run committed again as typed after their fix was undone: learned as typed.
    undone: Vec<usize>,
    /// The word an undo took back, as typed: committed again unchanged, it stays so.
    reopened: Option<String>,
    /// What the last commit put after its last word, until anything else is committed or a key
    /// ends the run: only then can that word's fix be undone.
    tail: Option<String>,
    fixing: bool,
}

/// A poisoned lock still holds a usable dictionary: a panic elsewhere must not stop typing.
fn lock(dictionary: &Mutex<Dictionary>) -> MutexGuard<'_, Dictionary> {
    dictionary.lock().unwrap_or_else(|e| e.into_inner())
}

/// The words of committed text: letters only, as a run splits at digits and symbols.
pub(crate) fn words_of(text: &str) -> Vec<&str> {
    text.split(|c: char| !c.is_alphabetic())
        .filter(|w| !w.is_empty())
        .collect()
}

impl Diacritics {
    pub fn new(dictionary: Arc<Mutex<Dictionary>>) -> Diacritics {
        Diacritics {
            dictionary,
            run: None,
            typed: Vec::new(),
            changes: Vec::new(),
            words: 0,
            over: false,
            learned: 0,
            undone: Vec::new(),
            reopened: None,
            tail: None,
            fixing: true,
        }
    }

    /// Whether runs started from now on fix syllables the table does not know.
    pub fn set_fixing(&mut self, on: bool) {
        self.fixing = on;
    }

    fn margin(&self) -> Option<f64> {
        self.fixing.then_some(FIX_MARGIN)
    }

    /// Whether the last commit ended in a word that was fixed: an undo would take it back.
    pub fn can_undo(&self) -> bool {
        self.tail.is_some()
            && (self.run.as_ref())
                .is_some_and(|run| !run.is_empty() && run.last_fix() == Some(run.len() - 1))
    }

    /// Takes the last word committed out of the run when it was fixed, for the engine to reopen
    /// as typed. Returns the text the field shows from that word on, which the commit ended, and
    /// the word as typed. Committed again unchanged, it stays as typed and is learned so.
    pub fn undo_fix(&mut self) -> Option<(String, String)> {
        // Until the front end applies the changes, the field does not show the fix.
        if !self.can_undo() || !self.changes.is_empty() {
            return None;
        }
        let tail = self.tail.take()?;
        let run = self.run.as_mut()?;
        let shown = run.shown().last()?.clone() + &tail;
        run.pop();
        let typed = self.typed.pop()?;
        self.words -= 1;
        self.over = false;
        self.learned = self.learned.min(run.len());
        self.reopened = Some(typed.clone());
        Some((shown, typed))
    }

    /// What the engine committed at a word end, and its keys. Anything but letters and spaces
    /// ends the run where it stands, so a macro of two sentences makes two runs.
    pub fn committed(&mut self, text: &str, keys: &str) {
        // The keys end with the one that ended the word, and read one word only.
        let keys = keys.trim_end_matches(|c: char| !c.is_alphanumeric());
        let keys = (words_of(text).len() == 1).then_some(keys);
        let reopened = self.reopened.take();
        let last = text.rfind(char::is_alphabetic);
        self.tail = last.map(|i| text[i..].chars().skip(1).collect());
        let mut open = false;
        for piece in text.split_inclusive(|c: char| !c.is_alphabetic()) {
            let word = piece.trim_end_matches(|c: char| !c.is_alphabetic());
            if !word.is_empty() {
                let as_typed = !open && reopened.as_deref() == Some(word);
                self.push(word, keys, as_typed);
                open = true;
            }
            if piece.ends_with(|c: char| !c.is_alphabetic() && c != ' ') {
                self.change(true);
                self.end_run();
                open = false;
            }
        }
        if open {
            self.change(false);
        }
    }

    /// A key the engine forwards (Enter, Tab, a symbol after a space): the run ends where it is,
    /// and the key, such as Home, may move the cursor off the last word, so its fix stays.
    pub fn ended_at_key(&mut self) {
        self.change(true);
        self.end_run();
        self.reopened = None;
        self.tail = None;
    }

    /// A reset: the cursor moved or the field lost focus, so the field no longer holds the run
    /// where its changes would go. The run ends without changing anything.
    pub fn reset(&mut self) {
        self.end_run();
        self.forget_run();
        self.changes.clear();
        self.words = 0;
    }

    fn change(&mut self, ended: bool) {
        let Some(run) = self.run.as_mut().filter(|_| !self.over) else {
            return;
        };
        if let Some(change) = run.change(ended, Reach::Run, ToneStyle::New)
            && run.apply(&change)
        {
            let end = self.words - (run.len() - change.first - change.old.len());
            self.changes.push((change, end));
        }
    }

    fn push(&mut self, word: &str, keys: Option<&str>, as_typed: bool) {
        if self.over {
            self.forget_run();
        }
        if self.run.as_ref().is_some_and(Run::is_full) {
            // The full run ends here: its last change before the words of the next run arrive.
            self.change(true);
            self.learn();
            let typing = lock(&self.dictionary).typing();
            let margin = self.margin();
            self.run = self.run.take().map(|full| full.next(typing).fixing(margin));
            self.typed.clear();
            self.undone.clear();
            self.learned = 0;
        }
        let (dictionary, margin) = (&self.dictionary, self.margin());
        let run = self.run.get_or_insert_with(|| {
            let typing = lock(dictionary).typing();
            Run::new(typing, RAW_CONFIDENCE).fixing(margin)
        });
        // Keys that differ from the text and are plain letters are the word's second reading.
        match keys.filter(|k| *k != word && k.chars().all(|c| c.is_ascii_alphabetic())) {
            _ if as_typed => {
                self.undone.push(run.len());
                run.push_as_typed(word);
            }
            Some(keys) if has_any_vietnamese_rune(word) => run.push_either(word, keys),
            _ => run.push(word),
        }
        self.typed.push(word.to_string());
        self.words += 1;
    }

    /// Learns the words of the run not learned yet that were typed with marks, or committed again
    /// as typed after an undo, and are still shown so; a guessed word, or one whose raw reading
    /// replaced it, is not known to be right.
    fn learn(&mut self) {
        let shown = self.run.as_ref().map_or(&[][..], Run::shown);
        let run: Vec<Option<&str>> = (self.typed.iter().zip(shown).enumerate())
            .map(|(k, (w, s))| {
                let right = without_diacritics(w) != *w || self.undone.contains(&k);
                (k >= self.learned && right && w == s).then_some(w.as_str())
            })
            .collect();
        if run.iter().any(Option::is_some) {
            lock(&self.dictionary).learn(&run);
        }
        self.learned = shown.len();
    }

    fn end_run(&mut self) {
        if self.over {
            return;
        }
        self.learn();
        self.over = self.run.is_some();
    }

    fn forget_run(&mut self) {
        self.run = None;
        self.over = false;
        self.typed.clear();
        self.undone.clear();
        self.learned = 0;
        self.reopened = None;
        self.tail = None;
    }

    /// A field that turned private: the run ends, and nothing it held is learned.
    pub fn drop_run(&mut self) {
        self.forget_run();
        self.changes.clear();
        self.words = 0;
    }

    /// The changes made since the last call, oldest first, for the front end to apply.
    pub fn pull_changes(&mut self) -> Vec<FieldChange> {
        let words = self.words;
        (self.changes.drain(..))
            .map(|(change, end)| FieldChange {
                change,
                after: words - end,
            })
            .collect()
    }

    #[cfg(test)]
    pub fn run_len(&self) -> usize {
        self.run.as_ref().filter(|_| !self.over).map_or(0, Run::len)
    }
}
