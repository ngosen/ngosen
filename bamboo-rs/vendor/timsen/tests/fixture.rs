//! Compares the crate with `research.model` on the table in `fixtures/`, built by `research.fixture`.

use std::path::Path;
use std::sync::Arc;

use timsen::{Dictionary, Nearby, Personal, Table, ToneStyle, Typing};

fn fixture(name: &str) -> std::path::PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("tests/fixtures")
        .join(name)
}

fn table() -> Table {
    Table::open(fixture("tiny.timsen")).unwrap()
}

#[test]
fn restores_exactly_like_the_python_model() {
    let t = table();
    let runs = std::fs::read_to_string(fixture("tiny.runs")).unwrap();
    let expected = std::fs::read_to_string(fixture("tiny.expected")).unwrap();
    assert_eq!(runs.lines().count(), expected.lines().count());
    for (run, want) in runs.lines().zip(expected.lines()) {
        let words: Vec<&str> = run.split(' ').collect();
        assert_eq!(
            t.restore(&words, ToneStyle::New).join(" "),
            want,
            "run: {run}"
        );
    }
}

#[test]
fn restores_through_a_long_trigram_group_like_the_python_model() {
    let t = Table::open(fixture("long.timsen")).unwrap();
    let runs = std::fs::read_to_string(fixture("long.runs")).unwrap();
    let expected = std::fs::read_to_string(fixture("long.expected")).unwrap();
    assert_eq!(runs.lines().count(), expected.lines().count());
    for (run, want) in runs.lines().zip(expected.lines()) {
        let words: Vec<&str> = run.split(' ').collect();
        assert_eq!(
            t.restore(&words, ToneStyle::New).join(" "),
            want,
            "run: {run}"
        );
    }
    // Only the trigram after "ta có" picks the rarer tone.
    assert_eq!(
        t.restore(&["ta", "co", "ba"], ToneStyle::New),
        ["ta", "có", "bá"]
    );
    assert_eq!(t.restore(&["anh", "thay", "ba"], ToneStyle::New)[2], "bà");
}

#[test]
fn keeps_capitalization_and_unknown_words() {
    let got = table().restore(&["Toi", "go", "PHIM", "Commit"], ToneStyle::New);
    assert_eq!(got, ["Tôi", "gõ", "PHÍM", "Commit"]);
}

#[test]
fn old_tone_placement_on_request() {
    let t = table();
    assert_eq!(t.restore(&["hoa", "binh"], ToneStyle::New), ["hoà", "bình"]);
    assert_eq!(t.restore(&["hoa", "binh"], ToneStyle::Old), ["hòa", "bình"]);
    assert_eq!(t.restore(&["Thuy", "loi"], ToneStyle::Old), ["Thủy", "lợi"]);
}

#[test]
fn empty_run_restores_to_nothing() {
    assert!(table().restore(&[], ToneStyle::New).is_empty());
}

#[test]
fn rejects_files_that_are_not_tables() {
    assert!(Table::from_bytes(b"not a table".to_vec()).is_err());
    let mut bytes = std::fs::read(fixture("tiny.timsen")).unwrap();
    bytes.truncate(bytes.len() - 100);
    assert!(Table::from_bytes(bytes).is_err());
}

/// A table file of its own, removed when dropped.
struct TempTable(std::path::PathBuf);

impl TempTable {
    fn new(name: &str, bytes: &[u8]) -> TempTable {
        let path = std::env::temp_dir().join(format!("timsen-{}-{name}", std::process::id()));
        std::fs::write(&path, bytes).unwrap();
        TempTable(path)
    }
}

impl Drop for TempTable {
    fn drop(&mut self) {
        let _ = std::fs::remove_file(&self.0);
    }
}

#[test]
fn an_open_table_ignores_its_file_being_rewritten_in_place() {
    use std::io::Write;
    let bytes = std::fs::read(fixture("tiny.timsen")).unwrap();
    let file = TempTable::new("rewritten", &bytes);
    let t = Table::open(&file.0).unwrap();
    let before = t.restore(&["toi", "go", "phim"], ToneStyle::New);
    // An update that writes over the file instead of replacing it, as `cp` onto an existing file does.
    let mut f = std::fs::OpenOptions::new()
        .write(true)
        .open(&file.0)
        .unwrap();
    f.write_all(&vec![0xff; bytes.len()]).unwrap();
    f.sync_all().unwrap();
    assert_eq!(t.restore(&["toi", "go", "phim"], ToneStyle::New), before);
}

#[test]
fn opening_a_broken_file_is_an_error() {
    let mut bytes = std::fs::read(fixture("tiny.timsen")).unwrap();
    bytes.truncate(bytes.len() - 100);
    let file = TempTable::new("truncated", &bytes);
    assert!(Table::open(&file.0).is_err());
    assert!(Table::open(std::env::temp_dir().join("timsen-no-such-table")).is_err());
}

#[test]
fn restores_with_learned_counts_exactly_like_the_python_model() {
    let t = table();
    let mut personal = Personal::new(&t);
    for run in std::fs::read_to_string(fixture("tiny.learned"))
        .unwrap()
        .lines()
    {
        let words: Vec<Option<&str>> = run.split(' ').map(Some).collect();
        personal.learn(&words);
    }
    let runs = std::fs::read_to_string(fixture("tiny.runs")).unwrap();
    let expected = std::fs::read_to_string(fixture("tiny.personal.expected")).unwrap();
    let before = std::fs::read_to_string(fixture("tiny.expected")).unwrap();
    assert_ne!(expected, before, "the learned text should change some runs");
    assert_eq!(runs.lines().count(), expected.lines().count());
    for (run, want) in runs.lines().zip(expected.lines()) {
        let words: Vec<&str> = run.split(' ').collect();
        let got = personal.restore(&words, ToneStyle::New).join(" ");
        assert_eq!(got, want, "run: {run}");
    }
}

#[test]
fn a_run_on_the_dictionary_restores_like_the_python_model_learned_up_to_its_start() {
    let mut d = Dictionary::new(Arc::new(table()));
    let runs = std::fs::read_to_string(fixture("tiny.runs")).unwrap();
    let start =
        |d: &Dictionary| -> Vec<Typing<'static>> { runs.lines().map(|_| d.typing()).collect() };
    let before = start(&d);
    for run in std::fs::read_to_string(fixture("tiny.learned"))
        .unwrap()
        .lines()
    {
        let words: Vec<Option<&str>> = run.split(' ').map(Some).collect();
        d.learn(&words);
    }
    let after = start(&d);
    for (typings, file) in [(before, "tiny.expected"), (after, "tiny.personal.expected")] {
        let expected = std::fs::read_to_string(fixture(file)).unwrap();
        for ((mut typing, run), want) in typings.into_iter().zip(runs.lines()).zip(expected.lines())
        {
            for syllable in run.split(' ') {
                typing.push(syllable);
            }
            assert_eq!(
                typing.finish(ToneStyle::New).join(" "),
                want,
                "{file}, run: {run}"
            );
        }
    }
}

#[test]
fn fixes_unknown_syllables_exactly_like_research_typo() {
    let t = table();
    let fixes = std::fs::read_to_string(fixture("tiny.fixes")).unwrap();
    assert!(
        fixes.lines().count() >= 20,
        "the fixture lists both ways for each syllable"
    );
    for line in fixes.lines() {
        let [run, i, mode, want, margin] = line.split('\t').collect::<Vec<_>>()[..] else {
            panic!("bad line: {line}");
        };
        let (i, ended): (usize, bool) = (i.parse().unwrap(), mode == "ended");
        let words: Vec<&str> = run.split(' ').collect();
        // While typing, a syllable is decided when its word ends: nothing after it is known yet.
        let upto = if ended { words.len() } else { i + 1 };
        let mut typing = Typing::new(&t);
        for w in &words[..upto] {
            typing.push(w);
        }
        match (want, typing.fix(i, ended)) {
            ("-", None) => {}
            (want, Some(fix)) => {
                assert_eq!(fix.word, want, "{line}");
                let margin: f64 = margin.parse().unwrap();
                assert!(
                    (fix.margin - margin).abs() < 1e-9,
                    "{line}: margin {}",
                    fix.margin
                );
            }
            (want, None) => panic!("{line}: no fix, want {want}"),
        }
    }
}

#[test]
fn a_known_or_marked_syllable_is_never_a_fix_target() {
    let t = table();
    let mut typing = Typing::new(&t);
    for w in ["toi", "gõ", "phmi"] {
        typing.push(w);
    }
    assert_eq!(typing.fix(0, true), None, "known");
    assert_eq!(typing.fix(1, true), None, "typed with marks");
    assert_eq!(typing.fix(3, true), None, "past the run");
    assert_eq!(typing.fix(2, true).unwrap().word, "phim");
}

#[test]
fn fixes_to_nearby_words_exactly_like_research_telex_typo() {
    let t = table();
    let fixes = std::fs::read_to_string(fixture("tiny.nearby")).unwrap();
    assert!(fixes.lines().count() >= 8);
    for line in fixes.lines() {
        let [before, shown, nearby, want, margin] = line.split('\t').collect::<Vec<_>>()[..] else {
            panic!("bad line: {line}");
        };
        let mut typing = Typing::new(&t);
        for w in before.split(' ').filter(|w| !w.is_empty()) {
            // Both readings the same: the word is context as typed, as the research takes it.
            typing.push_either(w, w);
        }
        typing.push(shown);
        let nearby: Vec<Nearby> = (nearby.split(' '))
            .map(|n| {
                let (word, cost) = n.rsplit_once(':').unwrap();
                let cost = cost.parse().unwrap();
                Nearby { word, cost }
            })
            .collect();
        match (want, typing.fix_nearby(typing.len() - 1, false, &nearby)) {
            ("-", None) => {}
            (want, Some(fix)) => {
                assert_eq!(fix.word, want, "{line}");
                let margin: f64 = margin.parse().unwrap();
                assert!(
                    (fix.margin - margin).abs() < 1e-9,
                    "{line}: margin {}",
                    fix.margin
                );
            }
            (want, None) => panic!("{line}: no fix, want {want}"),
        }
    }
}

#[test]
fn a_known_syllable_or_one_past_the_run_takes_no_nearby_word() {
    let t = table();
    let mut typing = Typing::new(&t);
    for w in ["toi", "gõ", "phsifm"] {
        typing.push(w);
    }
    let nearby = [Nearby {
        word: "phím",
        cost: 1.0,
    }];
    assert_eq!(typing.fix_nearby(0, false, &nearby), None, "known");
    assert_eq!(typing.fix_nearby(1, false, &nearby), None, "known, marked");
    assert_eq!(typing.fix_nearby(3, false, &nearby), None, "past the run");
    assert_eq!(typing.fix_nearby(2, false, &[]), None, "nothing nearby");
    assert_eq!(typing.fix_nearby(2, false, &nearby).unwrap().word, "phím");
}
