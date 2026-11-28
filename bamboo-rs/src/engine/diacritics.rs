// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

//! Đoán dấu for one field: the run of words since it started, guessed by the personal dictionary
//! every field shares, and the changes the front end applies to what the field shows.

use std::collections::HashMap;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::{Arc, Mutex, MutexGuard};

use timsen::{Change, Dictionary, FIX_MARGIN, Nearby, Reach, Run, ToneStyle};

use crate::text::{has_any_vietnamese_rune, without_diacritics};

/// Confidence a raw reading (`test`) needs to replace the interpreted one (`tét`).
const RAW_CONFIDENCE: f64 = 0.99;

/// How often a word without marks that nothing knows is typed and left alone before it is learned
/// as meant.
const MEANT_AFTER: u8 = 3;

/// Whether a word could be meant as typed: plain letters, neither one alone nor a long string.
fn could_be_meant(word: &str) -> bool {
    (2..=10).contains(&word.len()) && word.bytes().all(|b| b.is_ascii_alphabetic())
}

/// A change and where it sits in the field: `after` committed words lie between it and the
/// cursor. The front end checks `change.old` there before it replaces anything, and applies the
/// changes before it forwards the key that produced them.
#[cfg(test)]
#[derive(Debug)]
pub struct FieldChange {
    pub change: Change,
    pub after: usize,
}

/// The text the field shows from the first word changed to the cursor, and the text to show
/// there instead.
pub type Replacement = (String, String);

/// Gives, for the keys of a word, the words that keys one slip away would have typed, each with
/// the cost of that slip in timsen's units.
pub type WordsNear<'a> = &'a dyn Fn(&str) -> Vec<(String, f64)>;

/// Whether the fields learn, shared by all of them. `epoch` moves on every change and every
/// forget, so a run that was open across one learns nothing, even when learning is back on by its
/// end. `forgets` moves on every forget only: a run open across one is dropped, since it still
/// guesses with what was forgotten.
pub struct Learning {
    on: AtomicBool,
    epoch: AtomicU64,
    forgets: AtomicU64,
    /// How often each word that could be meant was left alone. Kept in memory only, so a word
    /// typed once or twice never reaches the file.
    left_alone: Mutex<HashMap<String, u8>>,
}

impl Learning {
    pub fn new(on: bool) -> Learning {
        Learning {
            on: AtomicBool::new(on),
            epoch: AtomicU64::new(0),
            forgets: AtomicU64::new(0),
            left_alone: Mutex::default(),
        }
    }

    fn left_alone(&self) -> MutexGuard<'_, HashMap<String, u8>> {
        self.left_alone.lock().unwrap_or_else(|e| e.into_inner())
    }

    /// Counts a word typed and left alone; true once that happened often enough to mean it.
    fn means(&self, word: &str) -> bool {
        let word = word.to_lowercase();
        let mut counts = self.left_alone();
        let count = counts.entry(word.clone()).or_default();
        *count += 1;
        let meant = *count >= MEANT_AFTER;
        if meant {
            counts.remove(&word);
        }
        meant
    }

    /// Takes back the last count of a word that was reopened: it counts when it ends again.
    fn uncount(&self, word: &str) {
        let word = word.to_lowercase();
        let mut counts = self.left_alone();
        if let Some(count) = counts.get_mut(&word) {
            *count -= 1;
            if *count == 0 {
                counts.remove(&word);
            }
        }
    }

    /// Everything learned was just forgotten.
    pub fn forgot(&self) {
        self.left_alone().clear();
        self.forgets.fetch_add(1, Ordering::SeqCst);
        self.epoch.fetch_add(1, Ordering::SeqCst);
    }

    fn forgets(&self) -> u64 {
        self.forgets.load(Ordering::SeqCst)
    }

    pub fn set(&self, on: bool) {
        if self.on.swap(on, Ordering::SeqCst) != on {
            self.left_alone().clear();
            self.epoch.fetch_add(1, Ordering::SeqCst);
        }
    }

    fn epoch(&self) -> u64 {
        self.epoch.load(Ordering::SeqCst)
    }

    fn allows(&self, epoch: u64) -> bool {
        self.on.load(Ordering::SeqCst) && self.epoch() == epoch
    }
}

pub struct Diacritics {
    dictionary: Arc<Mutex<Dictionary>>,
    learning: Arc<Learning>,
    /// The learning epoch when the run started.
    run_epoch: u64,
    /// The forget count when the run started.
    run_forgets: u64,
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
    /// Words of the run a macro put there: the user did not type them, so they are never learned.
    from_macro: Vec<usize>,
    /// Words of the run typed without marks that nothing knew: one left alone often enough is
    /// learned as meant.
    plain: Vec<usize>,
    /// Whether this field counts those words.
    counts_plain: bool,
    /// The last word of the run that just ended, when it was counted as left alone: Backspace may
    /// still reopen it.
    counted_last: Option<String>,
    /// The word an undo took back, as typed, and whether a macro put it there: committed again
    /// unchanged, it stays so.
    reopened: Option<(String, bool)>,
    /// What the last commit put after its last word, until anything else is committed or a key
    /// ends the run: only then can that word's fix be undone.
    tail: Option<String>,
    /// The last word committed made the word before it fixed.
    later_fix: bool,
    /// How far a fix must score above the rest before it is made; None makes none.
    fix_margin: Option<f64>,
    reach: Reach,
    style: ToneStyle,
    /// What the field shows of the words committed since the cursor moved, each with the text
    /// committed after it, from word `field_start` on: a change is applied as the text from its
    /// first word to the cursor.
    field: Vec<(String, String)>,
    field_start: usize,
}

/// A poisoned lock still holds a usable dictionary: a panic elsewhere must not stop typing.
fn lock(dictionary: &Mutex<Dictionary>) -> MutexGuard<'_, Dictionary> {
    dictionary.lock().unwrap_or_else(|e| e.into_inner())
}

/// Restores each stretch of words that only spaces separate as one run, as a sentence reads, and
/// keeps everything between the words as it is.
pub(crate) fn restore_runs(text: &str, restore: impl Fn(&[&str]) -> Vec<String>) -> String {
    let mut out = String::new();
    let mut words = Vec::new();
    let mut gaps = Vec::new();
    let mut rest = text;
    while !rest.is_empty() {
        let (word, after) = rest.split_at(
            rest.find(|c: char| !c.is_alphabetic())
                .unwrap_or(rest.len()),
        );
        let (gap, next) = after.split_at(after.find(char::is_alphabetic).unwrap_or(after.len()));
        if word.is_empty() {
            out.push_str(gap);
        } else {
            words.push(word);
            gaps.push(gap);
        }
        if !words.is_empty() && (!gap.chars().all(char::is_whitespace) || next.is_empty()) {
            let restored = restore(&words);
            let restored = if restored.len() == words.len() {
                restored
            } else {
                words.iter().map(|w| (*w).to_string()).collect()
            };
            for (word, gap) in restored.iter().zip(&gaps) {
                out.push_str(word);
                out.push_str(gap);
            }
            words.clear();
            gaps.clear();
        }
        rest = next;
    }
    out
}

/// The words of committed text: letters only, as a run splits at digits and symbols.
pub(crate) fn words_of(text: &str) -> Vec<&str> {
    text.split(|c: char| !c.is_alphabetic())
        .filter(|w| !w.is_empty())
        .collect()
}

impl Diacritics {
    pub fn new(dictionary: Arc<Mutex<Dictionary>>, learning: Arc<Learning>) -> Diacritics {
        let (run_epoch, run_forgets) = (learning.epoch(), learning.forgets());
        Diacritics {
            dictionary,
            learning,
            run_epoch,
            run_forgets,
            run: None,
            typed: Vec::new(),
            changes: Vec::new(),
            words: 0,
            over: false,
            learned: 0,
            undone: Vec::new(),
            from_macro: Vec::new(),
            plain: Vec::new(),
            counts_plain: false,
            counted_last: None,
            reopened: None,
            tail: None,
            later_fix: false,
            fix_margin: Some(FIX_MARGIN),
            reach: Reach::Run,
            style: ToneStyle::New,
            field: Vec::new(),
            field_start: 0,
        }
    }

    /// The margin runs started from now on fix syllables the table does not know by, or None
    /// for no fixing.
    pub fn set_fixing(&mut self, margin: Option<f64>) {
        self.fix_margin = margin;
    }

    /// Whether words without marks that nothing knows count here toward being learned as meant.
    pub fn set_counting_plain(&mut self, on: bool) {
        self.counts_plain = on;
    }

    /// How far back a change may reach.
    pub fn set_reach(&mut self, reach: Reach) {
        self.reach = reach;
    }

    /// The tone placement the changes follow, as the engine's own words do.
    pub fn set_style(&mut self, style: ToneStyle) {
        self.style = style;
    }

    /// Text typed without marks with the marks the dictionary gives it, learning nothing.
    pub fn restore(&self, text: &str, style: ToneStyle) -> String {
        let personal = lock(&self.dictionary).snapshot();
        restore_runs(text, |words| personal.restore(words, style))
    }

    fn margin(&self) -> Option<f64> {
        self.fix_margin
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
        // Only the key that ended the word may follow it, since the word reopens as letters alone
        // and anything else after it (a digit) would be lost.
        if self.tail.as_ref().is_some_and(|t| t.chars().count() > 1) {
            return None;
        }
        let tail = self.tail.take()?;
        let run = self.run.as_mut()?;
        let shown = run.shown().last()?.clone() + &tail;
        run.pop();
        let typed = self.typed.pop()?;
        let from_macro = self.from_macro.last() == Some(&self.typed.len());
        if from_macro {
            self.from_macro.pop();
        }
        self.plain.retain(|&k| k < self.typed.len());
        self.words -= 1;
        self.field.pop();
        self.over = false;
        self.learned = self.learned.min(run.len());
        self.later_fix = false;
        self.reopened = Some((typed.clone(), from_macro));
        Some((shown, typed))
    }

    /// Whether the last commit ended in a word that made the word before it fixed: an undo would
    /// take that fix back.
    pub fn can_undo_earlier(&self) -> bool {
        self.later_fix
            && self.tail.is_some()
            && self.reach == Reach::Run
            && (self.run.as_ref()).is_some_and(|run| run.last_fix() == run.len().checked_sub(2))
    }

    /// The text the field shows from the word `undo_earlier_fix` would bring back to the cursor.
    pub fn earlier_fix_shown(&self) -> Option<String> {
        // Until the front end applies the changes, the field does not show the fix.
        if !self.can_undo_earlier() || !self.changes.is_empty() {
            return None;
        }
        let from = self.words.checked_sub(2)?.checked_sub(self.field_start)?;
        Some(self.field_text(from))
    }

    /// Takes back the fix the last word committed made to the word before it, which stays as
    /// typed from then on and is learned so. Returns the text the field shows from that word to
    /// the cursor, and the text to show there instead.
    pub fn undo_earlier_fix(&mut self) -> Option<Replacement> {
        self.earlier_fix_shown()?;
        self.later_fix = false;
        let run = self.run.as_mut()?;
        let at = run.len() - 2;
        run.undo_fix();
        // Only this word, which is what `earlier_fix_shown` had checked: what the run now reads
        // otherwise around it changes with the next word, as any guess does.
        let change = Change {
            first: at,
            old: vec![run.shown()[at].clone()],
            new: vec![self.typed[at].clone()],
        };
        if !run.apply(&change) {
            return None;
        }
        self.changes.push((change, self.words - 1));
        self.learn_as_typed(at);
        self.pull_field_change()
    }

    /// Learns word `at` of the run as typed: with the rest of the run when that is still to come,
    /// alone when the words around it are learned already.
    fn learn_as_typed(&mut self, at: usize) {
        if at >= self.learned {
            self.undone.push(at);
            return;
        }
        if self.from_macro.contains(&at) {
            return;
        }
        let mut run = vec![None; self.typed.len()];
        run[at] = Some(self.typed[at].as_str());
        // Checked under the lock a forget takes, as in `learn`.
        let mut dictionary = lock(&self.dictionary);
        if self.learning.allows(self.run_epoch) {
            dictionary.learn(&run);
        }
    }

    /// What the engine committed at a word end, and its keys; `typed` is false for a macro's text.
    /// Anything but letters and spaces ends the run where it stands, so a macro of two sentences
    /// makes two runs. `near` is asked about a typed word nothing knows.
    pub fn committed(&mut self, text: &str, keys: &str, typed: bool, near: WordsNear) {
        self.take_in(text, keys, typed.then_some(near), false);
    }

    /// A chat abbreviation's text: its words are context for the others, never guessed and never
    /// learned.
    pub fn committed_as_written(&mut self, text: &str) {
        self.take_in(text, "", None, true);
    }

    fn take_in(&mut self, text: &str, keys: &str, near: Option<WordsNear>, written: bool) {
        // The keys end with the one that ended the word, and read one word only.
        let keys = keys.trim_end_matches(|c: char| !c.is_alphanumeric());
        let keys = (words_of(text).len() == 1).then_some(keys);
        self.drop_forgotten_run();
        self.later_fix = false;
        let reopened = self.reopened.take();
        let last = text.rfind(char::is_alphabetic);
        let tail = last.map(|i| text[i..].chars().skip(1).collect());
        let mut open = false;
        for piece in text.split_inclusive(|c: char| !c.is_alphabetic()) {
            let word = piece.trim_end_matches(|c: char| !c.is_alphabetic());
            let after = piece[word.len()..].to_string();
            if !word.is_empty() {
                let taken_back = !open && reopened.as_ref().is_some_and(|(w, _)| w == word);
                self.push(word, keys, written || taken_back, near);
                if near.is_none() || taken_back && reopened.as_ref().is_some_and(|(_, m)| *m) {
                    self.from_macro.push(self.typed.len() - 1);
                }
                self.field.push((word.to_string(), after));
                open = true;
            } else if let Some((_, last)) = self.field.last_mut() {
                last.push_str(&after);
            }
            if piece.ends_with(|c: char| !c.is_alphabetic() && c != ' ') {
                self.change(true);
                self.end_run(true);
                open = false;
            }
        }
        if open {
            self.change(false);
        }
        // Only now: the run this commit's words started has forgotten the tail of the one before.
        self.tail = tail;
    }

    /// A key the engine forwards (Enter, Tab, a symbol after a space): the run ends where it is,
    /// and the key, such as Home, may move the cursor off the last word, so its fix stays.
    /// `typed_on` is false for a key that may be about to remove that word.
    pub fn ended_at_key(&mut self, typed_on: bool) {
        self.drop_forgotten_run();
        self.change(true);
        self.end_run(typed_on);
        self.reopened = None;
        self.tail = None;
        self.later_fix = false;
    }

    /// A key the engine never sees typed on from the last word, as Enter after a space does: the
    /// run ends there with that word left alone.
    pub fn typed_on(&mut self) {
        self.end_run(true);
    }

    /// The front end took `word` back from the field to type on, so it was not left alone.
    pub fn taken_back(&mut self, word: &str) {
        if self.counted_last.as_deref() == Some(word) {
            self.erased_back();
        }
    }

    /// Backspace came before any other word: it may be erasing the word that was just counted as
    /// left alone, and an app that shows none of its text cannot tell.
    pub fn erased_back(&mut self) {
        if let Some(word) = self.counted_last.take() {
            self.learning.uncount(&word);
        }
    }

    /// A reset: the cursor moved or the field lost focus, so the field no longer holds the run
    /// where its changes would go. The run ends without changing anything.
    pub fn reset(&mut self) {
        self.end_run(false);
        self.forget_run();
        self.changes.clear();
        self.words = 0;
        self.field.clear();
        self.field_start = 0;
    }

    fn change(&mut self, ended: bool) {
        let Some(run) = self.run.as_mut().filter(|_| !self.over) else {
            return;
        };
        if let Some(change) = run.change(ended, self.reach, self.style)
            && run.apply(&change)
        {
            let end = self.words - (run.len() - change.first - change.old.len());
            self.changes.push((change, end));
        }
    }

    fn push(&mut self, word: &str, keys: Option<&str>, as_typed: bool, near: Option<WordsNear>) {
        if self.over {
            self.forget_run();
        }
        if self.run.as_ref().is_some_and(Run::is_full) {
            // The full run ends here: its last change before the words of the next run arrive.
            self.change(true);
            self.learn(true);
            let typing = lock(&self.dictionary).typing();
            let margin = self.margin();
            self.run = self.run.take().map(|full| full.next(typing).fixing(margin));
            self.run_epoch = self.learning.epoch();
            self.run_forgets = self.learning.forgets();
            self.typed.clear();
            self.undone.clear();
            self.from_macro.clear();
            self.plain.clear();
            self.learned = 0;
        }
        if self.run.is_none() {
            self.run_epoch = self.learning.epoch();
            self.run_forgets = self.learning.forgets();
        }
        let (dictionary, margin) = (&self.dictionary, self.margin());
        let run = (self.run.take())
            .unwrap_or_else(|| Run::new(lock(dictionary).typing(), RAW_CONFIDENCE).fixing(margin));
        // A fix made after the next word changes the word before the last, and a field can start
        // to show its text during the run.
        let run = self.run.insert(run.fixing_again(self.reach == Reach::Run));
        let letters = keys.filter(|k| k.chars().all(|c| c.is_ascii_alphabetic()));
        // Keys that differ from the text and are plain letters are the word's second reading.
        let raw = letters.filter(|k| *k != word && has_any_vietnamese_rune(word));
        // A word neither reading of which is known was mistyped, or is new.
        let slipped = run.may_fix(word) && raw.is_none_or(|keys| run.may_fix(keys));
        let typed_plain = near.is_some() && !as_typed && could_be_meant(word);
        if typed_plain && self.counts_plain && run.may_fix(word) {
            self.plain.push(run.len());
        }
        let (before, fixed) = (run.len().checked_sub(1), run.last_fix());
        match (raw, letters.filter(|k| !k.is_empty()).zip(near)) {
            _ if as_typed => {
                self.undone.push(run.len());
                run.push_as_typed(word);
            }
            (_, Some((keys, near))) if slipped => {
                let words = near(keys);
                let nearby: Vec<Nearby> = (words.iter())
                    .map(|(word, cost)| Nearby { word, cost: *cost })
                    .collect();
                run.push_nearby(word, &nearby);
            }
            (Some(keys), _) => run.push_either(word, keys),
            _ => run.push(word),
        }
        self.later_fix = before.is_some() && run.last_fix() == before && fixed != before;
        self.typed.push(word.to_string());
        self.words += 1;
        self.counted_last = None;
    }

    /// Learns the words of the run not learned yet that were typed with marks, committed again as
    /// typed after an undo, or typed without marks often enough to be meant, and are still shown
    /// so; a guessed word, or one whose raw reading replaced it, is not known to be right. Nothing
    /// when learning is off, or was off at any time since the run started. The last word counts as
    /// left alone only when the user `typed_on`: a run that just stopped may lose it to Backspace.
    fn learn(&mut self, typed_on: bool) {
        let shown = self.run.as_ref().map_or(&[][..], Run::shown);
        let left = shown.len().saturating_sub(usize::from(!typed_on));
        let allowed = self.learning.allows(self.run_epoch);
        let counted: Vec<usize> = (self.plain.iter().copied())
            .filter(|&k| allowed && k >= self.learned && k < left && self.typed[k] == shown[k])
            .collect();
        self.counted_last =
            (counted.last().filter(|&&k| k + 1 == shown.len())).map(|&k| self.typed[k].clone());
        let meant: Vec<usize> = (counted.into_iter())
            .filter(|&k| self.learning.means(&self.typed[k]))
            .collect();
        let run: Vec<Option<&str>> = (self.typed.iter().zip(shown).enumerate())
            .map(|(k, (w, s))| {
                let known = without_diacritics(w) != *w || self.undone.contains(&k);
                let right = (known || meant.contains(&k)) && !self.from_macro.contains(&k);
                (k >= self.learned && right && w == s).then_some(w.as_str())
            })
            .collect();
        if run.iter().any(Option::is_some) {
            // Checked under the lock a forget takes, so none comes between the check and the learning.
            let mut dictionary = lock(&self.dictionary);
            if self.learning.allows(self.run_epoch) {
                dictionary.learn(&run);
            }
        }
        self.learned = shown.len();
    }

    fn end_run(&mut self, typed_on: bool) {
        if self.over {
            return;
        }
        self.learn(typed_on);
        self.over = self.run.is_some();
    }

    fn forget_run(&mut self) {
        self.run = None;
        self.over = false;
        self.typed.clear();
        self.undone.clear();
        self.from_macro.clear();
        self.plain.clear();
        self.learned = 0;
        self.reopened = None;
        self.tail = None;
        self.later_fix = false;
    }

    fn drop_forgotten_run(&mut self) {
        if self.run.is_some() && self.run_forgets != self.learning.forgets() {
            self.drop_run();
        }
    }

    /// A field that turned private: the run ends, and nothing it held is learned.
    pub fn drop_run(&mut self) {
        self.forget_run();
        self.changes.clear();
        self.words = 0;
        self.field.clear();
        self.field_start = 0;
    }

    /// The changes made since the last call, oldest first, for the front end to apply.
    #[cfg(test)]
    pub fn pull_changes(&mut self) -> Vec<FieldChange> {
        (self.pull().0.into_iter())
            .map(|(change, after)| FieldChange { change, after })
            .collect()
    }

    /// `pull_changes` as one replacement: the text the field shows from the first word changed to
    /// the end of the last commit, and the text to show there instead.
    pub fn pull_field_change(&mut self) -> Option<Replacement> {
        self.pull().1
    }

    fn pull(&mut self) -> (Vec<(Change, usize)>, Option<Replacement>) {
        let words = self.words;
        let changes = std::mem::take(&mut self.changes);
        let first = (changes.iter())
            .map(|(change, end)| end.saturating_sub(change.old.len()))
            .min();
        let from = first.and_then(|first| first.checked_sub(self.field_start));
        let old = from.map(|from| self.field_text(from));
        if from.is_none() && first.is_some()
            || !changes.iter().all(|(change, end)| self.show(change, *end))
        {
            // Never expected: the field would no longer hold what the run shows.
            self.reset();
            return (Vec::new(), None);
        }
        let replacement = old.zip(from.map(|from| self.field_text(from)));
        let keep = self.run.as_ref().map_or(0, Run::len);
        let gone = self.field.len().saturating_sub(keep);
        self.field.drain(..gone);
        self.field_start += gone;
        let changes = (changes.into_iter())
            .map(|(change, end)| (change, words - end))
            .collect();
        (changes, replacement)
    }

    fn field_text(&self, from: usize) -> String {
        (self.field.iter().skip(from))
            .flat_map(|(word, after)| [word.as_str(), after.as_str()])
            .collect()
    }

    /// Records in `field` a change ending at word `end`: false when the field does not hold its
    /// old words there.
    fn show(&mut self, change: &Change, end: usize) -> bool {
        let Some(first) = (end.checked_sub(change.old.len()))
            .and_then(|first| first.checked_sub(self.field_start))
        else {
            return false;
        };
        let Some(shown) = self.field.get_mut(first..first + change.old.len()) else {
            return false;
        };
        if !shown.iter().map(|(w, _)| w).eq(&change.old) || change.new.len() != change.old.len() {
            return false;
        }
        for ((word, _), new) in shown.iter_mut().zip(&change.new) {
            *word = new.clone();
        }
        true
    }

    #[cfg(test)]
    pub fn run_len(&self) -> usize {
        self.run.as_ref().filter(|_| !self.over).map_or(0, Run::len)
    }
}
