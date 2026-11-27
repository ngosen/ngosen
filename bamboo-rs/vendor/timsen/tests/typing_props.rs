//! Invariants of `Typing` that must hold for any input, on the fixture table.

use proptest::prelude::*;
use timsen::{Table, ToneStyle, Typing};

fn table() -> Table {
    Table::open(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/tests/fixtures/tiny.timsen"
    ))
    .unwrap()
}

/// One word as the input method passes it.
#[derive(Clone, Debug)]
enum Word {
    Plain(String),
    Marked(String),
    Either(String, String),
}

fn word() -> impl Strategy<Value = Word> {
    let known = prop::sample::select(vec!["go", "Ban", "phim", "QUY", "hoa", "thuy", "co", "ta"]);
    let marked = prop::sample::select(vec!["gỗ", "Gõ", "hòa", "THUỶ", "cửa", "đi"]);
    prop_oneof![
        known.prop_map(|w| Word::Plain(w.to_string())),
        "[a-zA-Z]{1,6}".prop_map(Word::Plain),
        marked.prop_map(|w| Word::Marked(w.to_string())),
        ("[a-zđ]{1,5}", "[a-z]{1,6}").prop_map(|(a, b)| Word::Either(a, b)),
    ]
}

fn case_of(w: &str) -> Vec<bool> {
    w.chars().map(char::is_uppercase).collect()
}

proptest! {
    #[test]
    fn output_follows_the_input(words in prop::collection::vec(word(), 1..8)) {
        let t = table();
        let mut typing = Typing::new(&t);
        for w in &words {
            match w {
                Word::Plain(s) | Word::Marked(s) => typing.push(s),
                Word::Either(a, b) => typing.push_either(a, b),
            }
        }
        for style in [ToneStyle::New, ToneStyle::Old] {
            let out = typing.finish(style);
            prop_assert_eq!(out.len(), words.len());
            for (w, o) in words.iter().zip(&out) {
                match w {
                    Word::Plain(s) => {
                        prop_assert_eq!(o.chars().count(), s.chars().count());
                        prop_assert_eq!(case_of(o), case_of(s));
                    }
                    Word::Marked(s) => prop_assert_eq!(o, s),
                    Word::Either(a, b) => prop_assert!(o == a || o == b),
                }
            }
        }
    }
}
