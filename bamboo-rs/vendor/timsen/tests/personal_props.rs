//! Invariants of `Personal` that must hold for any input, on the fixture table.

use proptest::prelude::*;
use timsen::{Personal, Table, ToneStyle};

fn table() -> Table {
    Table::open(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/tests/fixtures/tiny.timsen"
    ))
    .unwrap()
}

fn word() -> impl Strategy<Value = String> {
    let known = prop::sample::select(vec!["gõ", "gỗ", "nhà", "quý", "hòa", "phím", "go", "ngó"]);
    prop_oneof![known.prop_map(str::to_string), "[a-zđ]{1,5}"]
}

fn runs() -> impl Strategy<Value = Vec<Vec<Option<String>>>> {
    prop::collection::vec(
        prop::collection::vec(prop::option::weighted(0.8, word()), 1..6),
        0..6,
    )
}

proptest! {
    #[test]
    fn loading_any_text_never_panics(text in ".{0,200}", head in any::<bool>()) {
        let t = table();
        let text = if head { format!("timsen-personal 1\n{text}") } else { text };
        let _ = Personal::load(&t, text.as_bytes());
    }

    #[test]
    fn a_saved_dictionary_restores_like_the_original(learned in runs(), typed in prop::collection::vec("[a-z]{1,4}", 1..6)) {
        let t = table();
        let mut p = Personal::new(&t);
        for run in &learned {
            let run: Vec<Option<&str>> = run.iter().map(Option::as_deref).collect();
            p.learn(&run);
        }
        let mut file = Vec::new();
        p.save(&mut file).unwrap();
        let q = Personal::load(&t, file.as_slice()).unwrap();
        let typed: Vec<&str> = typed.iter().map(String::as_str).collect();
        let got = p.restore(&typed, ToneStyle::New);
        prop_assert_eq!(got.len(), typed.len());
        prop_assert_eq!(q.restore(&typed, ToneStyle::New), got);
    }
}
