//! The word-end contract for any keys: a change says exactly what the field shows in the part it
//! replaces, keeps every syllable's letters and capitalization, and leaves the rest alone.

use std::sync::Arc;

use proptest::prelude::*;
use proptest::strategy::ValueTree;
use timsen::{Change, Dictionary, FIX_MARGIN, MAX_RUN, Reach, Run, Table, ToneStyle, Typing};

fn table() -> Table {
    Table::open(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/tests/fixtures/tiny.timsen"
    ))
    .unwrap()
}

#[derive(Clone, Debug)]
enum Op {
    Push(String),
    Either(String, String),
    Pop,
    End(Reach, bool),
    /// Applies a change taken earlier, which the run may have moved past.
    Stale,
    /// Goes on in a new run, as at the cap.
    Next,
    /// Undoes the latest fix, as the key for it does.
    Undo,
    /// A word that must stay as typed, as one brought back by undoing its fix.
    Typed(String),
}

fn cased(word: String, how: u8) -> String {
    match how {
        0 => word.to_uppercase(),
        1 => {
            let mut c = word.chars();
            c.next()
                .map(|f| f.to_uppercase().chain(c).collect())
                .unwrap_or_default()
        }
        // A capital last, as a slip of the shift key leaves it.
        2 => {
            let mut c: Vec<char> = word.chars().collect();
            if let Some(l) = c.pop() {
                c.extend(l.to_uppercase());
            }
            c.into_iter().collect()
        }
        _ => word,
    }
}

fn syllable() -> impl Strategy<Value = String> {
    let words = vec![
        "nguoi", "ta", "co", "go", "quy", "cua", "nha", "phim", "toi", "da", "ban", "thich",
    ];
    let plain = prop_oneof![
        4 => prop::sample::select(words).prop_map(str::to_string),
        1 => "[a-z]{1,4}",
        1 => Just("gõ".to_string()),
        // Typos the fixture table can fix: a letter swapped, doubled or left out.
        1 => prop::sample::select(vec!["phmi", "phm", "quyy", "tooi", "rta", "mnah"])
            .prop_map(str::to_string),
    ];
    (plain, 0..6u8).prop_map(|(w, how)| cased(w, how))
}

fn op() -> impl Strategy<Value = Op> {
    let either = prop::sample::select(vec![("tét", "test"), ("fìle", "file"), ("sẽ", "sex")]);
    let reach = prop_oneof![Just(Reach::Run), Just(Reach::LastWord)];
    prop_oneof![
        6 => syllable().prop_map(Op::Push),
        1 => either.prop_map(|(a, b)| Op::Either(a.to_string(), b.to_string())),
        1 => Just(Op::Pop),
        4 => (reach, any::<bool>()).prop_map(|(r, e)| Op::End(r, e)),
        1 => Just(Op::Stale),
        1 => Just(Op::Next),
        1 => Just(Op::Undo),
        1 => syllable().prop_map(Op::Typed),
    ]
}

/// Uppercase flags letter by letter, and the letter count: what a restored word must keep.
fn shape(word: &str) -> Vec<bool> {
    word.chars().map(char::is_uppercase).collect()
}

/// Whether `typed` is a syllable the run could fix: unknown, with a known form one edit away.
fn fixable(t: &Table, typed: &str) -> bool {
    let mut typing = Typing::new(t);
    typing.push(typed);
    typing.fix(0, true).is_some()
}

fn check(
    t: &Table,
    field: &[String],
    typed: &[Op],
    c: &Change,
    reach: Reach,
) -> Result<(), TestCaseError> {
    let n = field.len();
    prop_assert!(!c.old.is_empty());
    prop_assert_eq!(c.old.len(), c.new.len());
    prop_assert!(c.first + c.old.len() <= n);
    prop_assert_eq!(&field[c.first..c.first + c.old.len()], c.old.as_slice());
    prop_assert!(c.old.first() != c.new.first() && c.old.last() != c.new.last());
    if reach == Reach::LastWord {
        prop_assert_eq!(c.first, n - 1);
    }
    for (i, new) in (c.first..).zip(&c.new) {
        match &typed[i] {
            // A fixed syllable may gain or lose a letter, but only one the run could fix.
            Op::Push(w) if shape(new) != shape(w) => {
                prop_assert!(fixable(t, w), "{} for {}", new, w);
                prop_assert!(new.chars().count().abs_diff(w.chars().count()) <= 1);
                prop_assert_eq!(shape(new).first().copied(), shape(w).first().copied());
                // Only lowercase, capitals or a capital first: any other mix stays as typed.
                let caps: Vec<bool> = w.chars().skip(1).map(char::is_uppercase).collect();
                prop_assert!(
                    !caps.contains(&true) || !caps.contains(&false),
                    "{} for {}",
                    new,
                    w
                );
                if w.chars().count() > 1 && w.chars().all(char::is_uppercase) {
                    prop_assert!(!new.chars().any(char::is_lowercase), "{} for {}", new, w);
                }
            }
            Op::Push(_) => {}
            Op::Either(a, b) => prop_assert!(new == a || new == b),
            Op::Typed(w) => prop_assert_eq!(new, w),
            _ => unreachable!("only pushes stay in the run"),
        }
    }
    Ok(())
}

proptest! {
    #[test]
    fn a_change_replaces_exactly_what_it_reports(ops in prop::collection::vec(op(), 1..40)) {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
        let mut field: Vec<String> = Vec::new();
        let mut typed: Vec<Op> = Vec::new();
        let mut taken: Vec<Change> = Vec::new();
        for op in &ops {
            match op {
                Op::Push(s) => {
                    run.push(s);
                    field.push(s.clone());
                    typed.push(op.clone());
                }
                Op::Either(a, b) => {
                    run.push_either(a, b);
                    field.push(a.clone());
                    typed.push(op.clone());
                }
                Op::Typed(s) => {
                    run.push_as_typed(s);
                    field.push(s.clone());
                    typed.push(op.clone());
                }
                Op::Pop => {
                    run.pop();
                    field.pop();
                    typed.pop();
                }
                Op::End(reach, ended) => {
                    if let Some(c) = run.change(*ended, *reach, ToneStyle::New) {
                        check(&t, &field, &typed, &c, *reach)?;
                        taken.push(c.clone());
                        let before = field.clone();
                        field[c.first..c.first + c.new.len()].clone_from_slice(&c.new);
                        assert!(run.apply(&c));
                        // Outside the change, nothing moved.
                        prop_assert_eq!(&field[..c.first], &before[..c.first]);
                        let end = c.first + c.new.len();
                        prop_assert_eq!(&field[end..], &before[end..]);
                        prop_assert_eq!(run.change(*ended, *reach, ToneStyle::New), None);
                    }
                }
                Op::Undo => {
                    // An undone fix keeps the word as typed, as a push as typed does.
                    if let Some(i) = run.last_fix() {
                        prop_assert!(run.undo_fix());
                        let Op::Push(w) = &typed[i] else { unreachable!("only a push is fixed") };
                        typed[i] = Op::Typed(w.clone());
                    }
                }
                Op::Next => {
                    run = run.next(Typing::new(&t));
                    field.clear();
                    typed.clear();
                }
                Op::Stale => {
                    let Some(c) = taken.first() else { continue };
                    let end = c.first + c.old.len();
                    let kept = |(k, new): (usize, &String)| {
                        matches!(&typed[c.first + k], Op::Typed(w) if w != new)
                    };
                    let fits = field.get(c.first..end) == Some(c.old.as_slice())
                        && !c.new.iter().enumerate().any(kept);
                    prop_assert_eq!(run.apply(c), fits);
                    if fits {
                        field[c.first..end].clone_from_slice(&c.new);
                    }
                }
            }
            prop_assert_eq!(run.shown(), field.as_slice());
            prop_assert_eq!(run.len(), typed.len());
        }
    }

    #[test]
    fn text_past_the_cap_goes_on_with_two_syllables_of_context(
        words in prop::collection::vec(syllable(), MAX_RUN + 1..2 * MAX_RUN + 20),
    ) {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99);
        let (mut context, mut start, mut runs) = (Vec::<String>::new(), 0, 0);
        for (i, w) in words.iter().enumerate() {
            if run.is_full() {
                prop_assert_eq!(run.len(), MAX_RUN);
                check_run(&t, &context, &words[start..i], run.shown())?;
                context = run.shown()[MAX_RUN - 2..].to_vec();
                (run, start, runs) = (run.next(Typing::new(&t)), i, runs + 1);
            }
            run.push(w);
            if let Some(c) = run.change(false, Reach::Run, ToneStyle::New) {
                prop_assert!(run.apply(&c));
            }
        }
        check_run(&t, &context, &words[start..], run.shown())?;
        prop_assert!(runs >= 1);
    }

    #[test]
    fn a_whole_run_ends_as_the_table_restores_it(words in prop::collection::vec(syllable(), 1..12)) {
        let t = table();
        let mut run = Run::new(Typing::new(&t), 0.99);
        for w in &words {
            run.push(w);
            if let Some(c) = run.change(false, Reach::Run, ToneStyle::New) {
                assert!(run.apply(&c));
            }
        }
        if let Some(c) = run.change(true, Reach::Run, ToneStyle::New) {
            assert!(run.apply(&c));
        }
        let words: Vec<&str> = words.iter().map(String::as_str).collect();
        prop_assert_eq!(run.shown().to_vec(), t.restore(&words, ToneStyle::New));
    }
}

/// A run after `context` shows what a run typed after it, kept as shown, guesses.
fn check_run(
    t: &Table,
    context: &[String],
    typed: &[String],
    shown: &[String],
) -> Result<(), TestCaseError> {
    let mut typing = Typing::new(t);
    for w in context {
        // One reading given twice is kept as given, never guessed again.
        typing.push_either(w, w);
    }
    for w in typed {
        typing.push(w);
    }
    prop_assert_eq!(&typing.guess(ToneStyle::New)[context.len()..], shown);
    Ok(())
}

#[test]
fn several_syllables_change_at_once_somewhere() {
    // The property above must meet a change of more than one syllable, or it proves little about
    // batches: count them over the same kind of runs.
    let t = table();
    let mut runner = proptest::test_runner::TestRunner::deterministic();
    let mut wide = 0;
    for _ in 0..200 {
        let words = prop::collection::vec(syllable(), 2..10)
            .new_tree(&mut runner)
            .unwrap()
            .current();
        let mut run = Run::new(Typing::new(&t), 0.99);
        for w in &words {
            run.push(w);
            if let Some(c) = run.change(false, Reach::Run, ToneStyle::New) {
                wide += usize::from(c.new.len() > 1);
                assert!(run.apply(&c));
            }
        }
    }
    assert!(wide > 0, "no change covered two syllables");
}

#[test]
fn the_next_run_sees_what_was_learned_before_it() {
    let mut d = Dictionary::new(Arc::new(table()));
    let typed = |run: &mut Run| {
        for w in ["zed", "sen"] {
            run.push(w);
            if let Some(c) = run.change(true, Reach::Run, ToneStyle::New) {
                assert!(run.apply(&c));
            }
        }
        run.shown().to_vec()
    };
    let mut before = Run::new(d.typing(), 0.99);
    before.push("nha");
    // A word the table lacks, so only learning can restore it.
    assert_ne!(typed(&mut Run::new(d.typing(), 0.99)), ["zèd", "sen"]);
    d.learn(&[Some("zèd"), Some("sen")]);
    let mut after = before.next(d.typing());
    assert_eq!(typed(&mut after), ["zèd", "sen"]);
}

#[test]
fn the_property_above_meets_fixes() {
    // The contract must hold for fixed syllables too, so the same kind of runs must contain some.
    let t = table();
    let mut runner = proptest::test_runner::TestRunner::deterministic();
    let mut fixes = 0;
    for _ in 0..300 {
        let words = prop::collection::vec(syllable(), 1..10)
            .new_tree(&mut runner)
            .unwrap()
            .current();
        let mut run = Run::new(Typing::new(&t), 0.99).fixing(Some(FIX_MARGIN));
        for w in &words {
            run.push(w);
            if let Some(c) = run.change(false, Reach::Run, ToneStyle::New) {
                let typed = &words[words.len() - run.len()..][c.first..];
                fixes += (c.new.iter().zip(typed))
                    .filter(|(new, w)| shape(new) != shape(w))
                    .count();
                assert!(run.apply(&c));
            }
        }
    }
    assert!(fixes > 0, "no syllable was fixed");
}
