//! A table and a dictionary shared through `Arc`, as an input method keeps them across fields.

use std::path::Path;
use std::sync::Arc;

use timsen::{Dictionary, Personal, Table, ToneStyle, Typing};

fn fixture(name: &str) -> std::path::PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("tests/fixtures")
        .join(name)
}

fn runs() -> Vec<Vec<String>> {
    std::fs::read_to_string(fixture("tiny.runs"))
        .unwrap()
        .lines()
        .map(|l| l.split(' ').map(str::to_string).collect())
        .collect()
}

fn learned() -> Vec<Vec<String>> {
    std::fs::read_to_string(fixture("tiny.learned"))
        .unwrap()
        .lines()
        .map(|l| l.split(' ').map(str::to_string).collect())
        .collect()
}

fn typed(mut typing: Typing<'static>, run: &[String]) -> Vec<String> {
    for syllable in run {
        typing.push(syllable);
    }
    typing.finish(ToneStyle::New)
}

/// Each field of an input method holds its run in a struct with no lifetime, behind a lock.
struct Field {
    typing: Typing<'static>,
}

#[test]
fn shared_values_can_live_in_a_field_behind_a_lock() {
    fn send_sync<T: Send + Sync>() {}
    send_sync::<Field>();
    send_sync::<Personal<'static>>();
    send_sync::<Dictionary>();
    send_sync::<Arc<Table>>();
}

#[test]
fn a_shared_table_restores_like_a_borrowed_one_after_the_caller_drops_it() {
    let table = Table::open(fixture("tiny.timsen")).unwrap();
    let want: Vec<Vec<String>> = runs()
        .iter()
        .map(|r| {
            let words: Vec<&str> = r.iter().map(String::as_str).collect();
            table.restore(&words, ToneStyle::New)
        })
        .collect();
    let shared = Arc::new(table);
    let fields: Vec<Field> = (0..runs().len())
        .map(|_| Field {
            typing: Typing::shared(Arc::clone(&shared)),
        })
        .collect();
    drop(shared);
    for ((field, run), want) in fields.into_iter().zip(runs()).zip(want) {
        assert_eq!(typed(field.typing, &run), want, "run: {run:?}");
    }
}

#[test]
fn a_shared_dictionary_restores_like_a_borrowed_one() {
    let table = Table::open(fixture("tiny.timsen")).unwrap();
    let mut borrowed = Personal::new(&table);
    let mut shared = Personal::shared(Arc::new(Table::open(fixture("tiny.timsen")).unwrap()));
    for run in learned() {
        let words: Vec<Option<&str>> = run.iter().map(|w| Some(w.as_str())).collect();
        borrowed.learn(&words);
        shared.learn(&words);
    }
    let shared = Arc::new(shared);
    let mut changed = 0;
    for run in runs() {
        let words: Vec<&str> = run.iter().map(String::as_str).collect();
        let want = borrowed.restore(&words, ToneStyle::New);
        changed += usize::from(want != table.restore(&words, ToneStyle::New));
        let got = typed(Typing::with_shared_personal(Arc::clone(&shared)), &run);
        assert_eq!(got, want, "run: {run:?}");
    }
    assert!(changed > 0, "the learned text should change some runs");
}

#[test]
fn a_dictionary_loaded_with_a_shared_table_matches_one_loaded_with_a_borrowed_table() {
    let table = Table::open(fixture("tiny.timsen")).unwrap();
    let mut p = Personal::new(&table);
    for run in learned() {
        let words: Vec<Option<&str>> = run.iter().map(|w| Some(w.as_str())).collect();
        p.learn(&words);
    }
    let mut file = Vec::new();
    p.save(&mut file).unwrap();
    let borrowed = Personal::load(&table, file.as_slice()).unwrap();
    let again = Arc::new(Table::open(fixture("tiny.timsen")).unwrap());
    let mut shared = Personal::load_shared(again, file.as_slice()).unwrap();
    let (mut a, mut b) = (Vec::new(), Vec::new());
    borrowed.save(&mut a).unwrap();
    shared.save(&mut b).unwrap();
    assert_eq!(a, b);
    shared.clear();
    assert!(shared.is_empty());
}
