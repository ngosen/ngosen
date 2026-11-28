//! What an input method changes in the field when a word ends: the run as it is shown, against the
//! run as it is guessed.

use crate::{Fix, MARKS_MARGIN, Nearby, ToneStyle, Typing, tone};

/// One change to the field: syllables `first..first + old.len()` of the run read `old` and become
/// `new`, as long. The input method checks `old` against the field before it deletes anything, and
/// leaves the field alone when they differ.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Change {
    pub first: usize,
    pub old: Vec<String>,
    pub new: Vec<String>,
}

/// How far back a change may reach.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Reach {
    /// Any syllable of the run: the field reports its text, so earlier words can be checked.
    Run,
    /// Only the word that just ended, for a field that does not report its text in time.
    LastWord,
}

/// The most syllables a run should hold before the input method goes on in a new one; see
/// [`Run::is_full`]. Typed text rarely gets near it, and a key stays fast well past it.
pub const MAX_RUN: usize = 200;

/// Syllables a run started by [`Run::next`] takes from the one before, as context only.
const CONTEXT: usize = 2;

/// Syllables at the end of a run whose reading is still weighed on every key. A word read two ways
/// is decided for good when it leaves them, so a key costs the same however long the run.
const WINDOW: usize = 4;

/// A syllable the input method read two ways, and the reading kept once it left the window.
struct Reading {
    interpreted: String,
    raw: String,
    raw_kept: bool,
}

impl Reading {
    fn kept(&self) -> &String {
        if self.raw_kept {
            &self.raw
        } else {
            &self.interpreted
        }
    }
}

/// The fix in the case it was typed in: lowercase, capitals, or a capital first. Any other mix has
/// no place for the capitals once a letter moves, so the word is left as typed.
fn fix_case(typed: &str, plain: &str) -> Option<String> {
    let mut chars = typed.chars();
    let first = chars.next()?.is_uppercase();
    let rest: Vec<bool> = chars.map(char::is_uppercase).collect();
    match (first, rest.iter().all(|&u| u), rest.iter().any(|&u| u)) {
        (_, _, false) => Some(tone::apply_case(typed, plain)),
        (true, true, _) => Some(plain.to_uppercase()),
        _ => None,
    }
}

/// How a syllable went into the typing, so it can go in again after an earlier one changes.
enum Pushed {
    /// Guessed from this text: as typed, or as fixed.
    Guessed(String),
    Either(String, String),
    /// Kept as typed, never guessed or fixed: a fix the user undid.
    Kept(String),
}

/// A syllable left as typed that the next word may still fix, with the words near its keys, and
/// whether those differ from it only in marks ([`Run::push_marks`]).
struct Waiting {
    at: usize,
    nearby: Vec<(String, f64)>,
    marks: bool,
}

/// A run on its way into a field. Each syllable is shown as typed until a [`Change`] says otherwise;
/// later syllables can change earlier ones, several at once.
pub struct Run<'t> {
    typing: Typing<'t>,
    /// Syllables from the run before, at the start of `typing`, that only serve as context.
    context: Vec<String>,
    shown: Vec<String>,
    readings: Vec<Option<Reading>>,
    pushed: Vec<Pushed>,
    /// The syllable as typed, for one that was fixed.
    fixed: Vec<Option<String>>,
    raw_confidence: f64,
    fix_margin: Option<f64>,
    fix_again: bool,
    waiting: Option<Waiting>,
}

impl<'t> Run<'t> {
    /// A run guessed by `typing`, which should be empty. A raw reading replaces the interpreted one
    /// only as [`crate::Choices::takes_raw`] allows with `raw_confidence`.
    pub fn new(typing: Typing<'t>, raw_confidence: f64) -> Run<'t> {
        let mut run = Run {
            typing,
            context: Vec::new(),
            shown: Vec::new(),
            readings: Vec::new(),
            pushed: Vec::new(),
            fixed: Vec::new(),
            raw_confidence,
            fix_margin: None,
            fix_again: false,
            waiting: None,
        };
        while !run.typing.is_empty() {
            run.typing.pop();
        }
        run
    }

    /// How far a fix must win by ([`crate::Typing::fix`]), or `None`, the default, to fix nothing.
    /// [`FIX_MARGIN`] is the margin measured to be safe.
    pub fn fixing(mut self, margin: Option<f64>) -> Run<'t> {
        self.fix_margin = margin;
        self
    }

    /// Whether a syllable [`Run::push_nearby`] left as typed is decided once more when the word
    /// after it ends, with that word as context too. Such a fix changes the word before the last,
    /// so it is for a field whose earlier words can change ([`Reach::Run`]). Off by default. What
    /// counts is the setting when the word after ends, so an input method may turn it on for a
    /// field that only then shows its text.
    pub fn fixing_again(mut self, again: bool) -> Run<'t> {
        self.fix_again = again;
        self
    }

    /// A word that ended, shown as typed. A syllable nothing knows is fixed now, from the words
    /// before it, when a fix wins by the margin; a later [`Run::change`] shows it.
    pub fn push(&mut self, syllable: &str) {
        self.push_fixing(syllable, |typing, last| typing.fix(last, false));
    }

    /// Whether [`Run::push_nearby`] could fix `syllable`: fixing is on and nothing knows the
    /// syllable. Finding the words near its keys costs the input method time, so it asks first.
    pub fn may_fix(&self, syllable: &str) -> bool {
        self.fix_margin.is_some() && !self.typing.knows(syllable)
    }

    /// [`Run::push`] for a word whose keys the input method also read with a slip in them: `nearby`
    /// are the words that keys near the ones typed would have shown. A syllable nothing knows is
    /// fixed to one of those ([`Typing::fix_nearby`]) and to nothing else.
    pub fn push_nearby(&mut self, syllable: &str, nearby: &[Nearby<'_>]) {
        self.push_fixing(syllable, |typing, last| {
            typing.fix_nearby(last, false, nearby)
        });
        let at = self.shown.len() - 1;
        if self.fixed[at].is_none() && !nearby.is_empty() {
            let nearby = (nearby.iter())
                .map(|near| (near.word.to_string(), near.cost))
                .collect();
            self.waiting = Some(Waiting {
                at,
                nearby,
                marks: false,
            });
        }
    }

    /// A word that may be known, with `raw` its second reading as for [`Run::push_either`], and
    /// `nearby` the words that keys near the ones typed would have shown and that differ from it
    /// only in marks. It is shown as typed; when the word after it ends, it is fixed to one of
    /// those that wins over keeping it by [`MARKS_MARGIN`] ([`Typing::fix_marks`]). That needs
    /// fixing on and [`Run::fixing_again`]. A syllable nothing knows is fixed as [`Run::push`]
    /// fixes it.
    pub fn push_marks(&mut self, syllable: &str, raw: Option<&str>, nearby: &[Nearby<'_>]) {
        match raw {
            Some(raw) => self.push_either(syllable, raw),
            None => self.push(syllable),
        }
        let at = self.shown.len() - 1;
        if self.fixed[at].is_none() && !nearby.is_empty() {
            let nearby = (nearby.iter())
                .map(|near| (near.word.to_string(), near.cost))
                .collect();
            self.waiting = Some(Waiting {
                at,
                nearby,
                marks: true,
            });
        }
    }

    /// Decides the syllable before the one just pushed once more, when it waits for that.
    fn decide_waiting(&mut self) {
        let Some(Waiting { at, nearby, marks }) = self.waiting.take() else {
            return;
        };
        if !self.fix_again {
            return;
        }
        let typed = match self.pushed.get(at) {
            Some(Pushed::Guessed(typed)) => typed,
            Some(Pushed::Either(interpreted, _)) if marks => interpreted,
            _ => return,
        };
        if at + 2 != self.shown.len() {
            return;
        }
        let typed = typed.clone();
        let nearby: Vec<Nearby> = (nearby.iter())
            .map(|(word, cost)| Nearby { word, cost: *cost })
            .collect();
        let i = self.context.len() + at;
        let fix = (self.fix_margin)
            .and_then(|m| match marks {
                true => {
                    (self.typing.fix_marks(i, false, &nearby)).filter(|f| f.margin >= MARKS_MARGIN)
                }
                false => (self.typing.fix_nearby(i, false, &nearby)).filter(|f| f.margin >= m),
            })
            .and_then(|f| fix_case(&typed, &f.word));
        if let Some(fix) = fix {
            self.fixed[at] = Some(typed);
            self.pushed[at] = Pushed::Guessed(fix);
            self.readings[at] = None;
            self.retype_from(at);
        }
    }

    /// Puts syllables `i..` into the typing again, after one of them changed how it goes in.
    fn retype_from(&mut self, i: usize) {
        while self.typing.len() > self.context.len() + i {
            self.typing.pop();
        }
        for pushed in &self.pushed[i..] {
            match pushed {
                Pushed::Guessed(text) => self.typing.push(text),
                Pushed::Either(interpreted, raw) => self.typing.push_either(interpreted, raw),
                Pushed::Kept(text) => self.typing.push_context(text),
            }
        }
    }

    fn push_fixing(&mut self, syllable: &str, fix: impl Fn(&Typing<'t>, usize) -> Option<Fix>) {
        self.typing.push(syllable);
        let mut fixed = None;
        let mut given = syllable.to_string();
        let last = self.typing.len() - 1;
        let fix = (self.fix_margin)
            .and_then(|m| fix(&self.typing, last).filter(|f| f.margin >= m))
            .and_then(|f| fix_case(syllable, &f.word));
        if let Some(fix) = fix {
            given = fix;
            self.typing.pop();
            self.typing.push(&given);
            fixed = Some(syllable.to_string());
        }
        self.shown.push(syllable.to_string());
        self.readings.push(None);
        self.pushed.push(Pushed::Guessed(given));
        self.fixed.push(fixed);
        self.decide_waiting();
        self.decide_leaving();
    }

    /// A word that stays as typed: neither guessed nor fixed, only context for the words around
    /// it. For a word the user took back from a fix and committed again unchanged.
    pub fn push_as_typed(&mut self, syllable: &str) {
        self.typing.push_context(syllable);
        self.shown.push(syllable.to_string());
        self.readings.push(None);
        self.pushed.push(Pushed::Kept(syllable.to_string()));
        self.fixed.push(None);
        self.decide_waiting();
        self.decide_leaving();
    }

    /// A word the input method read two ways, shown as `interpreted`.
    pub fn push_either(&mut self, interpreted: &str, raw: &str) {
        self.typing.push_either(interpreted, raw);
        self.shown.push(interpreted.to_string());
        self.readings.push(Some(Reading {
            interpreted: interpreted.to_string(),
            raw: raw.to_string(),
            raw_kept: false,
        }));
        self.pushed
            .push(Pushed::Either(interpreted.to_string(), raw.to_string()));
        self.fixed.push(None);
        self.decide_waiting();
        self.decide_leaving();
    }

    /// Where the latest fix still in the run is, counted from its start like [`Run::shown`], or
    /// `None`. [`Run::undo_fix`] undoes that one, so an input method that undoes only the fix just
    /// made checks first that it is the last syllable.
    pub fn last_fix(&self) -> Option<usize> {
        self.fixed.iter().rposition(Option::is_some)
    }

    /// Undoes the latest fix still in the run: the syllable goes back to what was typed and stays
    /// so, also after the run ended. Returns whether there was one; [`Run::change`] then shows it.
    /// Learning the syllable as typed afterwards keeps it from being fixed in later runs.
    pub fn undo_fix(&mut self) -> bool {
        let Some(i) = self.last_fix() else {
            return false;
        };
        let typed = self.fixed[i].take().unwrap_or_default();
        self.pushed[i] = Pushed::Kept(typed);
        self.retype_from(i);
        true
    }

    /// Decides the syllable that the last push moved out of the window, if it was read two ways.
    /// Backspace brings it back in, where it is weighed again until it leaves once more.
    fn decide_leaving(&mut self) {
        let n = self.shown.len();
        let Some(j) = n.checked_sub(WINDOW + 1) else {
            return;
        };
        let Some(reading) = &self.readings[j] else {
            return;
        };
        // Both readings are kept as given, so the tone style does not change them.
        let choices = self.typing.choices(WINDOW + 1, false, ToneStyle::New);
        let raw_kept = choices[0].takes_raw(&reading.raw, self.raw_confidence);
        if let Some(reading) = &mut self.readings[j] {
            reading.raw_kept = raw_kept;
        }
    }

    /// Removes the last syllable, as backspace into it does; the keys typed next make the syllable
    /// that replaces it.
    pub fn pop(&mut self) {
        if self.shown.is_empty() {
            return;
        }
        self.typing.pop();
        self.shown.pop();
        self.readings.pop();
        self.pushed.pop();
        self.fixed.pop();
    }

    pub fn len(&self) -> usize {
        self.shown.len()
    }

    pub fn is_empty(&self) -> bool {
        self.shown.is_empty()
    }

    /// Whether the run holds [`MAX_RUN`] syllables. The input method then ends it (learning what it
    /// learns at a run's end) and goes on with [`Run::next`] before the next word.
    pub fn is_full(&self) -> bool {
        self.shown.len() >= MAX_RUN
    }

    /// The run that goes on after this one, guessed by `typing`, which should be empty: take it from
    /// the dictionary again, so the new run sees what was learned meanwhile. This run's last two
    /// syllables, as shown, are its context only, so they are neither shown again, changed, nor
    /// learned twice.
    pub fn next(&self, mut typing: Typing<'t>) -> Run<'t> {
        let all: Vec<&String> = self.context.iter().chain(&self.shown).collect();
        let context: Vec<String> = all[all.len().saturating_sub(CONTEXT)..]
            .iter()
            .map(|w| w.to_string())
            .collect();
        while !typing.is_empty() {
            typing.pop();
        }
        for word in &context {
            typing.push_context(word);
        }
        Run {
            typing,
            context,
            shown: Vec::new(),
            readings: Vec::new(),
            pushed: Vec::new(),
            fixed: Vec::new(),
            raw_confidence: self.raw_confidence,
            fix_margin: self.fix_margin,
            fix_again: self.fix_again,
            waiting: None,
        }
    }

    /// The run as the field shows it.
    pub fn shown(&self) -> &[String] {
        &self.shown
    }

    /// The change that brings the field to the best guess, or `None` when it already shows it.
    /// `ended` says whether the run is over, as for [`Typing::finish`].
    pub fn change(&self, ended: bool, reach: Reach, style: ToneStyle) -> Option<Change> {
        let n = self.shown.len();
        let from = match reach {
            Reach::Run => 0,
            Reach::LastWord => n.saturating_sub(1),
        };
        let wanted = self.wanted(from, ended, style);
        let differs = |i: &usize| wanted[*i - from] != self.shown[*i];
        let first = (from..n).find(differs)?;
        let last = (from..n).rev().find(differs)?;
        Some(Change {
            first,
            old: self.shown[first..=last].to_vec(),
            new: wanted[first - from..=last - from].to_vec(),
        })
    }

    /// Records that the field now shows `change`. A change taken before the run moved on (a pop, a
    /// push, another change) no longer matches what is shown: it is refused, and nothing changes.
    #[must_use = "a refused change was not recorded"]
    pub fn apply(&mut self, change: &Change) -> bool {
        let end = change.first.checked_add(change.old.len());
        let matches = end
            .and_then(|end| self.shown.get(change.first..end))
            .is_some_and(|shown| shown == change.old.as_slice());
        if !matches || change.new.len() != change.old.len() {
            return false;
        }
        // Matching text cannot tell a word kept as typed from the one a stale change saw there.
        let alters_kept = (change.new.iter().enumerate()).any(
            |(k, new)| matches!(&self.pushed[change.first + k], Pushed::Kept(kept) if kept != new),
        );
        if alters_kept {
            return false;
        }
        let end = change.first + change.new.len();
        self.shown[change.first..end].clone_from_slice(&change.new);
        true
    }

    /// The best spelling of syllables `from..`. A syllable with two readings in the window keeps the
    /// interpreted one unless the raw one wins with the confidence asked for; one before the window
    /// keeps the reading decided when it left.
    fn wanted(&self, from: usize, ended: bool, style: ToneStyle) -> Vec<String> {
        let n = self.shown.len();
        let guess = if ended {
            self.typing.finish(style)
        } else {
            self.typing.guess(style)
        };
        let mut wanted = guess[self.context.len() + from..].to_vec();
        let live = n.saturating_sub(WINDOW).max(from);
        for i in from..live {
            if let Some(reading) = &self.readings[i] {
                wanted[i - from] = reading.kept().clone();
            }
        }
        let Some(earliest) = (live..n).find(|&i| self.readings[i].is_some()) else {
            return wanted;
        };
        let choices = self.typing.choices(n - earliest, ended, style);
        for i in earliest..n {
            if let Some(reading) = &self.readings[i] {
                let takes = choices[i - earliest].takes_raw(&reading.raw, self.raw_confidence);
                wanted[i - from] = if takes {
                    &reading.raw
                } else {
                    &reading.interpreted
                }
                .clone();
            }
        }
        wanted
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::FIX_MARGIN;
    use crate::Table;

    fn table() -> Table {
        let path = concat!(env!("CARGO_MANIFEST_DIR"), "/tests/fixtures/tiny.timsen");
        Table::open(path).unwrap()
    }

    fn strings(words: &[&str]) -> Vec<String> {
        words.iter().map(|w| w.to_string()).collect()
    }

    /// Pushes each word and applies the change at each word end, as an input method would.
    fn type_words(run: &mut Run, words: &[&str], reach: Reach) {
        for w in words {
            run.push(w);
            if let Some(c) = run.change(false, reach, ToneStyle::New) {
                assert!(run.apply(&c));
            }
        }
    }

    #[test]
    fn a_later_word_changes_two_syllables_in_one_batch() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99);
        type_words(&mut run, &["nguoi", "ta", "co", "go"], Reach::Run);
        assert_eq!(run.shown()[3], "gõ");
        run.push("quy");
        let change = run.change(false, Reach::Run, ToneStyle::New).unwrap();
        assert_eq!(change.first, 3);
        assert_eq!(change.old, strings(&["gõ", "quy"]));
        assert_eq!(change.new, strings(&["gỗ", "quý"]));
        assert!(run.apply(&change));
        assert_eq!(run.change(false, Reach::Run, ToneStyle::New), None);
    }

    #[test]
    fn a_field_that_does_not_report_its_text_changes_only_the_last_word() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99);
        type_words(&mut run, &["nguoi", "ta", "co", "go"], Reach::LastWord);
        run.push("quy");
        let change = run.change(false, Reach::LastWord, ToneStyle::New).unwrap();
        assert_eq!(change.first, 4);
        assert_eq!(change.old, strings(&["quy"]));
        assert_eq!(change.new, strings(&["quý"]));
    }

    #[test]
    fn backspace_into_a_word_then_more_keys_replaces_it() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99);
        type_words(&mut run, &["nguoi", "ta", "co", "go", "quy"], Reach::Run);
        assert_eq!(run.shown()[3], "gỗ");
        run.pop();
        run.pop();
        type_words(&mut run, &["go", "cua"], Reach::Run);
        assert_eq!(run.shown()[3], "gõ");
        assert_eq!(run.len(), 5);
    }

    #[test]
    fn a_raw_reading_needs_the_confidence_asked_for() {
        let t = table();
        for (confidence, want) in [(0.0, "file"), (1.1, "fìle")] {
            let mut run = Run::new(Typing::new(&t), confidence);
            run.push("mở");
            run.push_either("fìle", "file");
            run.push("code");
            if let Some(c) = run.change(true, Reach::Run, ToneStyle::New) {
                assert!(run.apply(&c));
            }
            assert_eq!(run.shown()[1], want, "confidence {confidence}");
        }
    }

    #[test]
    fn a_raw_reading_shown_goes_back_when_the_context_turns() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.0);
        run.push("mở");
        run.push_either("tét", "test");
        assert!(run.apply(&Change {
            first: 1,
            old: strings(&["tét"]),
            new: strings(&["test"]),
        }));
        let change = run.change(true, Reach::Run, ToneStyle::New).unwrap();
        assert_eq!(change.old, strings(&["test"]));
        assert_eq!(change.new, strings(&["tét"]));
    }

    #[test]
    fn a_change_that_no_longer_matches_is_refused() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99);
        type_words(&mut run, &["nguoi", "ta", "co"], Reach::Run);
        run.push("go");
        let change = run.change(false, Reach::Run, ToneStyle::New).unwrap();
        run.pop();
        assert!(!run.apply(&change), "the syllable it replaces is gone");
        run.push("phim");
        assert!(!run.apply(&change), "another syllable stands there now");
        let shown = run.shown().to_vec();
        let wrong = Change {
            first: 0,
            old: shown[..1].to_vec(),
            new: strings(&["a", "b"]),
        };
        assert!(!run.apply(&wrong), "new must be as long as old");
        assert_eq!(run.shown(), shown);
    }

    /// What a run sees after `context` kept as shown, for the syllables after it.
    fn after(t: &Table, context: &[&str], run: &[&str]) -> Vec<String> {
        let mut typing = Typing::new(t);
        for w in context {
            // One reading given twice is kept as given, never guessed again.
            typing.push_either(w, w);
        }
        for w in run {
            typing.push(w);
        }
        typing.guess(ToneStyle::New)[context.len()..].to_vec()
    }

    #[test]
    fn the_next_run_reads_the_last_two_syllables_as_context_only() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99);
        type_words(&mut run, &["nguoi", "ta", "co", "go"], Reach::Run);
        let context: Vec<String> = run.shown()[2..].to_vec();
        let mut next = run.next(Typing::new(&t));
        assert!(next.is_empty());
        assert_eq!(next.context, context);
        next.pop();
        assert_eq!(next.context, context, "backspace never reaches the context");
        type_words(&mut next, &["quy", "nha"], Reach::Run);
        let context: Vec<&str> = context.iter().map(String::as_str).collect();
        assert_eq!(next.shown(), after(&t, &context, &["quy", "nha"]));
        assert_eq!(next.len(), 2, "the context is not shown again");
    }

    #[test]
    fn the_context_is_kept_as_shown_and_not_guessed_again() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99);
        // No change was applied, so `co go` stands unmarked in the field.
        for w in ["nguoi", "ta", "co", "go"] {
            run.push(w);
        }
        let next = run.next(Typing::new(&t));
        let mut guessed = Typing::new(&t);
        guessed.push("co");
        guessed.push("go");
        assert_ne!(guessed.guess(ToneStyle::New), ["co", "go"]);
        assert_eq!(next.typing.guess(ToneStyle::New), ["co", "go"]);
    }

    #[test]
    fn a_word_read_two_ways_is_decided_when_it_leaves_the_window() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.0);
        run.push("mở");
        run.push_either("fìle", "file");
        assert_eq!(run.wanted(1, false, ToneStyle::New)[0], "file");
        for _ in 0..WINDOW {
            run.push("ta");
        }
        // Out of the window, its reading no longer depends on the confidence or what comes next.
        run.raw_confidence = 1.1;
        assert_eq!(run.wanted(1, true, ToneStyle::New)[0], "file");
        run.pop();
        assert_eq!(
            run.wanted(1, true, ToneStyle::New)[0],
            "fìle",
            "back in, weighed again"
        );
    }

    #[test]
    fn a_short_run_passes_on_context_from_the_one_before() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99);
        type_words(&mut run, &["nguoi", "ta", "co"], Reach::Run);
        let mut next = run.next(Typing::new(&t));
        type_words(&mut next, &["go"], Reach::Run);
        let last = run.shown()[2].clone();
        let mut third = next.next(Typing::new(&t));
        type_words(&mut third, &["quy"], Reach::Run);
        let gone = next.shown()[0].clone();
        assert_eq!(third.context, [last.clone(), gone.clone()]);
        assert_eq!(third.shown(), after(&t, &[&last, &gone], &["quy"]));
    }

    #[test]
    fn a_run_is_full_at_the_cap() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99);
        for _ in 0..MAX_RUN - 1 {
            run.push("ta");
        }
        assert!(!run.is_full());
        run.push("ta");
        assert!(run.is_full());
        assert!(!run.next(Typing::new(&t)).is_full());
    }

    #[test]
    fn an_empty_run_has_nothing_to_change() {
        let t = table();
        let run = Run::new(Typing::new(&t), 0.99);
        assert!(run.is_empty());
        assert_eq!(run.change(true, Reach::LastWord, ToneStyle::New), None);
    }

    #[test]
    fn a_syllable_nothing_knows_is_fixed_when_its_word_ends() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
        type_words(&mut run, &["toi", "go"], Reach::Run);
        run.push("Phmi");
        let change = run.change(false, Reach::LastWord, ToneStyle::New).unwrap();
        assert_eq!(
            (change.old, change.new),
            (strings(&["Phmi"]), strings(&["Phím"]))
        );
    }

    #[test]
    fn a_fix_that_does_not_win_by_the_margin_is_not_made() {
        let t = table();
        // On the fixture, phmi after "ban" wins by 3.87 when its word ends.
        let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
        type_words(&mut run, &["ban", "phmi"], Reach::Run);
        assert_eq!(run.shown()[1], "phmi");
        let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(3.8));
        type_words(&mut run, &["ban", "phmi"], Reach::Run);
        assert_eq!(run.shown()[1], "phím");
    }

    #[test]
    fn a_fix_keeps_a_word_typed_in_capitals_in_capitals() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
        type_words(&mut run, &["toi", "go", "PHM"], Reach::Run);
        assert_eq!(run.shown()[2], "PHÍM", "the added letter is a capital too");
    }

    #[test]
    fn a_word_in_mixed_case_is_left_as_typed() {
        let t = table();
        for typed in ["phM", "phMi", "pHmi"] {
            let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
            type_words(&mut run, &["toi", "go", typed], Reach::Run);
            assert_eq!(run.shown()[2], typed);
        }
        let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
        type_words(&mut run, &["toi", "go", "Phm"], Reach::Run);
        assert_eq!(run.shown()[2], "Phím");
    }

    #[test]
    fn a_word_with_a_letter_beyond_plain_latin_is_never_fixed() {
        let t = table();
        for typed in ["phi\u{300}m", "phi\u{1ab0}m", "phïm", "phmí\u{301}"] {
            let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
            type_words(&mut run, &["toi", "go", typed], Reach::Run);
            assert_eq!(run.shown()[2], typed);
        }
    }

    #[test]
    fn a_letter_that_lowercases_to_plain_latin_is_not_plain() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
        // U+212A, the Kelvin sign, lowercases to an ASCII k.
        type_words(&mut run, &["chuc", "anh", "\u{212a}he"], Reach::Run);
        assert_eq!(run.shown()[2], "\u{212a}he");
    }

    #[test]
    fn a_word_of_any_length_is_pushed_without_running_out_of_memory() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
        let long = "x".repeat(10_000);
        type_words(&mut run, &["toi", &long], Reach::Run);
        assert_eq!(run.shown()[1], long);
    }

    #[test]
    fn fixing_is_off_by_default() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99);
        type_words(&mut run, &["toi", "go", "phmi"], Reach::Run);
        assert_eq!(run.shown()[2], "phmi");
        let next = run.next(Typing::new(&t));
        assert_eq!(next.fix_margin, None, "the next run keeps the setting");
    }

    #[test]
    fn undoing_a_fix_brings_back_what_was_typed_for_good() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
        type_words(&mut run, &["toi", "go", "phmi", "rat"], Reach::Run);
        assert_eq!(run.shown()[2], "phím");
        assert!(run.undo_fix());
        let change = run.change(false, Reach::Run, ToneStyle::New).unwrap();
        assert_eq!((change.first, &change.new), (2, &strings(&["phmi"])));
        assert!(run.apply(&change));
        type_words(&mut run, &["nhanh"], Reach::Run);
        assert_eq!(run.shown(), strings(&["tôi", "gõ", "phmi", "rất", "nhanh"]));
        assert!(!run.undo_fix(), "nothing left to undo");
    }

    #[test]
    fn the_latest_fix_is_found_until_it_is_undone() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
        type_words(&mut run, &["toi", "go"], Reach::Run);
        assert_eq!(run.last_fix(), None);
        type_words(&mut run, &["phmi", "rat"], Reach::Run);
        assert_eq!(run.last_fix(), Some(2), "the fix stays found after a word");
        type_words(&mut run, &["toi", "go", "phmi"], Reach::Run);
        assert_eq!(run.last_fix(), Some(6));
        assert!(run.undo_fix());
        assert_eq!(run.last_fix(), Some(2), "the earlier fix is next");
        for _ in 0..4 {
            run.pop();
        }
        assert_eq!(run.last_fix(), Some(2));
        run.pop();
        assert_eq!(run.last_fix(), None, "popped with its word");
    }

    #[test]
    fn a_fix_is_undone_after_its_run_ended() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
        type_words(&mut run, &["toi", "go", "phmi"], Reach::Run);
        if let Some(c) = run.change(true, Reach::Run, ToneStyle::New) {
            assert!(run.apply(&c));
        }
        assert_eq!(run.shown()[2], "phím");
        assert!(run.undo_fix());
        let change = run.change(true, Reach::Run, ToneStyle::New).unwrap();
        assert_eq!((change.first, &change.new), (2, &strings(&["phmi"])));
        assert!(run.apply(&change));
        assert_eq!(run.shown(), strings(&["tôi", "gõ", "phmi"]));
    }

    #[test]
    fn an_undone_fix_learned_as_typed_is_not_fixed_again() {
        let mut d = crate::Dictionary::new(std::sync::Arc::new(table()));
        // As an input method learns a run: guessed words unknown, the undone one as typed.
        d.learn(&[None, None, Some("phmi")]);
        let mut run = Run::new(d.typing(), 0.99).fixing(Some(FIX_MARGIN));
        type_words(&mut run, &["toi", "go", "phmi"], Reach::Run);
        assert_eq!(run.shown()[2], "phmi");
        assert_eq!(run.last_fix(), None);
    }

    #[test]
    fn a_word_pushed_as_typed_stays_so_and_is_context() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
        type_words(&mut run, &["toi", "go"], Reach::Run);
        run.push_as_typed("phmi");
        assert_eq!(run.last_fix(), None, "not fixed");
        type_words(&mut run, &["rat", "nhanh"], Reach::Run);
        if let Some(c) = run.change(true, Reach::Run, ToneStyle::New) {
            assert!(run.apply(&c));
        }
        assert_eq!(run.shown(), strings(&["tôi", "gõ", "phmi", "rất", "nhanh"]));
        run.pop();
        run.pop();
        run.pop();
        assert_eq!(run.len(), 2);
    }

    #[test]
    fn a_change_taken_before_a_word_was_kept_does_not_alter_it() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
        type_words(&mut run, &["toi", "go"], Reach::Run);
        run.push("phmi");
        let fix = run.change(false, Reach::LastWord, ToneStyle::New).unwrap();
        assert_eq!(fix.new, strings(&["phím"]));
        run.pop();
        run.push_as_typed("phmi");
        assert!(!run.apply(&fix), "pushed as typed");
        run.pop();
        run.push("phmi");
        assert!(run.apply(&fix));
        assert!(run.undo_fix());
        let back = run.change(false, Reach::LastWord, ToneStyle::New).unwrap();
        assert!(run.apply(&back));
        assert!(!run.apply(&fix), "its fix undone");
        assert_eq!(run.shown(), strings(&["tôi", "gõ", "phmi"]));
    }

    #[test]
    fn a_word_in_the_personal_dictionary_is_never_fixed() {
        let mut d = crate::Dictionary::new(std::sync::Arc::new(table()));
        d.learn(&[Some("phmí")]);
        let mut run = Run::new(d.typing(), 0.99).fixing(Some(FIX_MARGIN));
        type_words(&mut run, &["toi", "go", "phmi"], Reach::Run);
        assert_eq!(
            run.shown()[2],
            "phmí",
            "the learned word, not fixed to phím"
        );
    }

    /// Pushes a word with the words near its keys and applies the change, as an input method would.
    fn type_nearby(run: &mut Run, word: &str, nearby: &[(&str, f64)]) {
        let nearby: Vec<Nearby> = (nearby.iter())
            .map(|&(word, cost)| Nearby { word, cost })
            .collect();
        run.push_nearby(word, &nearby);
        if let Some(c) = run.change(false, Reach::Run, ToneStyle::New) {
            assert!(run.apply(&c));
        }
    }

    #[test]
    fn a_word_near_the_keys_typed_replaces_a_syllable_nothing_knows() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
        type_words(&mut run, &["tôi", "gõ"], Reach::Run);
        assert!(run.may_fix("Phsifm"));
        type_nearby(&mut run, "Phsifm", &[("phím", 1.0), ("nhanh", 2.5)]);
        assert_eq!(run.shown()[2], "Phím");
        assert_eq!(run.last_fix(), Some(2));
        assert!(run.undo_fix());
        let back = run.change(false, Reach::Run, ToneStyle::New).unwrap();
        assert_eq!(back.new, strings(&["Phsifm"]));
    }

    #[test]
    fn a_nearby_word_that_costs_too_much_is_not_taken() {
        let t = table();
        // On the fixture, phím after "tôi gõ" wins by 17.85 before its cost, 2 for each unit.
        for (cost, shown) in [(6.5, "phím"), (7.0, "phsifm")] {
            let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
            type_words(&mut run, &["tôi", "gõ"], Reach::Run);
            type_nearby(&mut run, "phsifm", &[("phím", cost)]);
            assert_eq!(run.shown()[2], shown, "cost {cost}");
        }
    }

    #[test]
    fn a_syllable_given_nearby_words_is_fixed_to_none_but_those() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
        type_words(&mut run, &["tôi", "gõ"], Reach::Run);
        // One edit from phim, which a plain push fixes it to.
        type_nearby(&mut run, "phmi", &[]);
        type_nearby(
            &mut run,
            "phmi",
            &[("xyzzy", 1.0), ("phmí", 1.0), ("phmi", 1.0)],
        );
        assert_eq!(run.shown()[2..], strings(&["phmi", "phmi"]));
        assert_eq!(run.last_fix(), None);
    }

    #[test]
    fn nearby_words_that_come_to_one_spelling_do_not_compete() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
        type_words(&mut run, &["tôi", "gõ"], Reach::Run);
        type_nearby(
            &mut run,
            "phsifm",
            &[("phim", 1.0), ("phím", 1.0), ("PHÍM", 3.0)],
        );
        assert_eq!(run.shown()[2], "phím");
    }

    #[test]
    fn a_word_something_knows_takes_no_nearby_word() {
        let mut d = crate::Dictionary::new(std::sync::Arc::new(table()));
        d.learn(&[Some("phsím")]);
        let mut run = Run::new(d.typing(), 0.99).fixing(Some(FIX_MARGIN));
        type_words(&mut run, &["tôi", "gõ"], Reach::Run);
        for known in ["nhanh", "Gỗ", "go", "phsím"] {
            assert!(!run.may_fix(known), "{known}");
            type_nearby(&mut run, known, &[("phím", 1.0)]);
            assert_eq!(run.last_fix(), None, "{known}");
        }
        assert!(run.may_fix("phsifm"));
        let off = Run::new(d.typing(), 0.99);
        assert!(!off.may_fix("phsifm"), "fixing is off");
    }

    // On the fixture, người for ngwoif at the start of a run wins by 2 at its end and by 16.67
    // once "ta" follows.
    const SHORT_OF_THE_MARGIN: [(&str, f64); 2] = [("người", 1.0), ("tôi", 2.0)];

    #[test]
    fn a_syllable_short_of_the_margin_is_fixed_once_the_next_word_ends() {
        let t = table();
        for (again, shown) in [(true, "Người"), (false, "Ngwoif")] {
            let run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
            let mut run = run.fixing_again(again);
            type_nearby(&mut run, "Ngwoif", &SHORT_OF_THE_MARGIN);
            assert_eq!(run.shown(), strings(&["Ngwoif"]), "not at its end");
            run.push("ta");
            let change = run.change(false, Reach::Run, ToneStyle::New);
            assert_eq!(change.is_some(), again);
            if let Some(change) = change {
                assert_eq!((change.first, &change.new), (0, &strings(&["Người"])));
                assert!(run.apply(&change));
            }
            assert_eq!(run.shown(), strings(&[shown, "ta"]));
            assert_eq!(run.last_fix(), again.then_some(0));
        }
    }

    #[test]
    fn the_setting_counts_when_the_next_word_ends() {
        let t = table();
        for (at_its_end, at_the_next) in [(false, true), (true, false)] {
            let run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
            let mut run = run.fixing_again(at_its_end);
            type_nearby(&mut run, "ngwoif", &SHORT_OF_THE_MARGIN);
            let mut run = run.fixing_again(at_the_next);
            run.push("ta");
            assert_eq!(run.last_fix(), at_the_next.then_some(0));
        }
    }

    #[test]
    fn a_fix_made_after_the_next_word_is_undone_like_any_other() {
        let t = table();
        let run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
        let mut run = run.fixing_again(true);
        type_nearby(&mut run, "ngwoif", &SHORT_OF_THE_MARGIN);
        type_words(&mut run, &["ta", "co"], Reach::Run);
        assert_eq!(run.shown(), strings(&["người", "ta", "có"]));
        assert!(run.undo_fix());
        let back = run.change(false, Reach::Run, ToneStyle::New).unwrap();
        assert_eq!((back.first, &back.new), (0, &strings(&["ngwoif"])));
        assert!(run.apply(&back));
        type_words(&mut run, &["go"], Reach::Run);
        assert_eq!(run.shown()[0], "ngwoif", "kept as typed for good");
    }

    #[test]
    fn only_the_word_right_after_decides_a_waiting_syllable() {
        let t = table();
        let fixing = |t| Run::new(Typing::new(t), 0.99).fixing(Some(FIX_MARGIN));
        // The syllable was taken back before the next word: what replaces it does not wait.
        let mut run = fixing(&t).fixing_again(true);
        type_nearby(&mut run, "ngwoif", &SHORT_OF_THE_MARGIN);
        run.pop();
        type_words(&mut run, &["xyzzy", "ta"], Reach::Run);
        assert_eq!(run.shown(), strings(&["xyzzy", "ta"]));
        // The word after did not decide it, so the one after that does not either.
        let mut run = fixing(&t).fixing_again(true);
        type_nearby(&mut run, "ngwoif", &SHORT_OF_THE_MARGIN);
        type_words(&mut run, &["xyzzy", "ta"], Reach::Run);
        assert_eq!(run.shown()[0], "ngwoif");
        // Taking the word after back and typing another does not ask again.
        let mut run = fixing(&t).fixing_again(true);
        type_nearby(&mut run, "ngwoif", &SHORT_OF_THE_MARGIN);
        run.push("xyzzy");
        run.pop();
        type_words(&mut run, &["ta"], Reach::Run);
        assert_eq!(run.shown(), strings(&["ngwoif", "ta"]));
    }

    #[test]
    fn a_waiting_syllable_is_decided_whichever_way_the_next_word_goes_in() {
        let t = table();
        for way in 0..3 {
            let run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
            let mut run = run.fixing_again(true);
            type_nearby(&mut run, "ngwoif", &SHORT_OF_THE_MARGIN);
            match way {
                0 => run.push_either("ta", "ta"),
                1 => run.push_as_typed("ta"),
                _ => run.push_nearby("ta", &[]),
            }
            assert_eq!(run.last_fix(), Some(0), "way {way}");
        }
    }

    /// Pushes a word with the words that differ from it only in marks and applies the change.
    fn type_marks(run: &mut Run, word: &str, raw: Option<&str>, nearby: &[(&str, f64)]) {
        let nearby: Vec<Nearby> = (nearby.iter())
            .map(|&(word, cost)| Nearby { word, cost })
            .collect();
        run.push_marks(word, raw, &nearby);
        if let Some(c) = run.change(false, Reach::Run, ToneStyle::New) {
            assert!(run.apply(&c));
        }
    }

    // On the fixture, gõ for gỗ between "tôi" and "phím" wins by 21.63 before its cost.
    #[test]
    fn a_known_word_is_fixed_in_its_marks_once_the_next_word_ends() {
        let t = table();
        for (again, shown) in [(true, "Gõ"), (false, "Gỗ")] {
            let run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
            let mut run = run.fixing_again(again);
            type_words(&mut run, &["tôi"], Reach::Run);
            type_marks(&mut run, "Gỗ", None, &[("gõ", 1.0)]);
            assert_eq!(run.shown()[1], "Gỗ", "not at its end");
            type_words(&mut run, &["phím"], Reach::Run);
            assert_eq!(run.shown(), strings(&["tôi", shown, "phím"]));
            assert_eq!(run.last_fix(), again.then_some(1));
        }
    }

    #[test]
    fn a_marks_fix_must_win_by_the_marks_margin() {
        let t = table();
        let fixing = |t| {
            let run = Run::new(Typing::new(t), 0.99).fixing(Some(FIX_MARGIN));
            run.fixing_again(true)
        };
        for (cost, shown) in [(5.0, "gõ"), (6.0, "gỗ")] {
            let mut run = fixing(&t);
            type_words(&mut run, &["tôi"], Reach::Run);
            type_marks(&mut run, "gỗ", None, &[("gõ", cost)]);
            type_words(&mut run, &["phím"], Reach::Run);
            assert_eq!(run.shown()[1], shown, "cost {cost}");
        }
        // bàn wins by 7.66 here: enough for a word nothing knows, not for one the table has.
        let mut run = fixing(&t);
        type_words(&mut run, &["cái"], Reach::Run);
        type_marks(&mut run, "bạn", None, &[("bàn", 1.0)]);
        type_words(&mut run, &["này"], Reach::Run);
        assert_eq!(run.shown(), strings(&["cái", "bạn", "này"]));
    }

    #[test]
    fn a_word_read_two_ways_is_fixed_in_its_marks_too() {
        let t = table();
        let run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
        let mut run = run.fixing_again(true);
        type_words(&mut run, &["tôi"], Reach::Run);
        type_marks(&mut run, "gỗ", Some("gox"), &[("gõ", 1.0)]);
        type_words(&mut run, &["phím", "rất", "nhanh", "và", "êm"], Reach::Run);
        assert_eq!(run.shown()[1], "gõ");
        assert!(run.undo_fix());
        let back = run.change(false, Reach::Run, ToneStyle::New).unwrap();
        assert_eq!((back.first, &back.new), (1, &strings(&["gỗ"])));
    }

    #[test]
    fn a_raw_reading_that_fits_is_not_fixed_in_its_marks() {
        let t = table();
        let run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
        let mut run = run.fixing_again(true);
        type_words(&mut run, &["tôi"], Reach::Run);
        type_marks(&mut run, "gỗ", Some("gõ"), &[("gõ", 1.0)]);
        type_words(&mut run, &["phím"], Reach::Run);
        assert_eq!(run.shown()[1], "gõ");
        assert!(!run.undo_fix(), "read so, not fixed");
    }

    #[test]
    fn no_marks_fix_without_fixing_on() {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99).fixing_again(true);
        type_words(&mut run, &["tôi"], Reach::Run);
        type_marks(&mut run, "gỗ", None, &[("gõ", 1.0)]);
        type_words(&mut run, &["phím"], Reach::Run);
        assert_eq!(run.shown(), strings(&["tôi", "gỗ", "phím"]));
    }

    #[test]
    fn keeping_a_known_word_is_scored_as_that_word() {
        // gỗ fits "bàn … lim": kept as itself it beats gõ by 10.23, as an unknown word by 2.51.
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99);
        type_words(&mut run, &["bàn", "gỗ", "lim"], Reach::Run);
        let near = [Nearby {
            word: "gõ",
            cost: 1.0,
        }];
        let fix = run.typing.fix_marks(1, false, &near).unwrap();
        assert!(fix.margin < -5.0, "{}", fix.margin);
    }
}
