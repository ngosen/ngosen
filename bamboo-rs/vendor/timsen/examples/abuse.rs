//! Feeds the crate damaged tables, damaged personal files, odd text and calls in any order, built
//! the way Ngó Sen builds it (`panic = "abort"`), where `cargo test` would catch a panic instead.
//! Each case asserts its result; any panic aborts the process, so a run that exits 0 saw none.
//!
//!     scripts/abuse.sh                  # release profile, then again with overflow checks
//!     cargo run --example abuse -- --control   # panics on purpose: must not exit 0
//!
//! Personal files go in `TIMSEN_ABUSE_DIR`, which the script creates and removes even after an
//! abort; without it, a directory under the system's temporary directory.

use std::fs;
use std::path::{Path, PathBuf};
use std::sync::Arc;

use timsen::{Change, Dictionary, FIX_MARGIN, Personal, Reach, Run, Table, ToneStyle, Typing};

const FIXTURE: &str = concat!(env!("CARGO_MANIFEST_DIR"), "/tests/fixtures/tiny.timsen");
const LEARNED: &str = concat!(env!("CARGO_MANIFEST_DIR"), "/tests/fixtures/tiny.learned");
const RUNS: &str = concat!(env!("CARGO_MANIFEST_DIR"), "/tests/fixtures/tiny.runs");
const HEADER: &[u8] = b"timsen-personal 1\n";

/// Text an input method could hand over: empty, spaces, controls, combining marks, letters whose
/// case changes their length, emoji, and a very long word.
const ODD: [&str; 16] = [
    "",
    " ",
    "\t",
    "\n",
    "\0",
    "\u{301}",
    "a\u{301}",
    "İ",
    "ß",
    "ǅ",
    "ﬁ",
    "😀",
    "Đ",
    "<s>",
    "</s>",
    "ĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐĐ",
];
const WORDS: [&str; 10] = [
    "toi", "go", "phim", "nha", "ta", "co", "quy", "Ngo", "SEN", "gõ",
];

/// xorshift64*: the same cases on every run, with no dependency.
struct Rng(u64);

impl Rng {
    fn next(&mut self) -> u64 {
        self.0 ^= self.0 >> 12;
        self.0 ^= self.0 << 25;
        self.0 ^= self.0 >> 27;
        self.0.wrapping_mul(0x2545_f491_4f6c_dd1d)
    }

    fn below(&mut self, n: usize) -> usize {
        (self.next() % n.max(1) as u64) as usize
    }

    fn word(&mut self) -> &'static str {
        if self.below(3) == 0 {
            ODD[self.below(ODD.len())]
        } else {
            WORDS[self.below(WORDS.len())]
        }
    }

    fn bytes(&mut self, max: usize) -> Vec<u8> {
        (0..self.below(max)).map(|_| self.next() as u8).collect()
    }
}

fn main() {
    if std::env::args().any(|a| a == "--control") {
        panic!("control case: this run must not exit 0");
    }
    let dir = match std::env::var_os("TIMSEN_ABUSE_DIR") {
        Some(dir) => PathBuf::from(dir),
        None => std::env::temp_dir().join(format!("timsen-abuse-{}", std::process::id())),
    };
    fs::create_dir_all(&dir).unwrap();
    let mut rng = Rng(0x9e37_79b9_7f4a_7c15);
    let fixture = fs::read(FIXTURE).unwrap();
    let table = Arc::new(Table::from_bytes(fixture.clone()).unwrap());
    let mut done = 0;
    done += truncated_tables(&fixture);
    done += corrupt_tables(&fixture, &mut rng);
    done += broken_personal_files(&table, &mut rng);
    done += kept_personal_files(&table, &dir.join("broken.txt"), &mut rng);
    done += typing_in_any_order(&table, &mut rng);
    done += runs_in_any_order(&table, &mut rng);
    done += dictionary_in_any_order(&table, &dir.join("personal.txt"), &mut rng);
    fs::remove_dir_all(&dir).unwrap();
    println!("{done} cases, no panic");
}

/// Types eight words into a run, taking each change; the run keeps one word per word typed.
fn exercise(typing: Typing, rng: &mut Rng) {
    let mut run = Run::new(typing, 0.99).fixing(Some(FIX_MARGIN));
    for typed in 1..=8 {
        let w = rng.word();
        if rng.below(4) == 0 {
            run.push_either(w, rng.word());
        } else {
            run.push(w);
        }
        if let Some(c) = run.change(rng.below(2) == 0, Reach::Run, ToneStyle::Old) {
            assert_eq!(c.old.len(), c.new.len());
            assert!(run.apply(&c));
        }
        assert_eq!(run.shown().len(), typed);
    }
}

/// A table cut short is an `Error`, unless the cut only took the padding after the last section:
/// then it restores exactly like the whole table.
fn truncated_tables(fixture: &[u8]) -> usize {
    let text = fs::read_to_string(RUNS).unwrap();
    let words: Vec<&str> = text.split_whitespace().collect();
    let whole = Table::from_bytes(fixture.to_vec()).unwrap();
    let want = whole.restore(&words, ToneStyle::New);
    let mut opened = 0;
    for n in 0..fixture.len() {
        if let Ok(t) = Table::from_bytes(fixture[..n].to_vec()) {
            assert!(fixture[n..].iter().all(|&b| b == 0), "cut at {n} took data");
            assert_eq!(t.restore(&words, ToneStyle::New), want, "cut at {n}");
            opened += 1;
        }
    }
    assert!(opened < 8, "{opened} cuts opened: more than the padding");
    fixture.len()
}

/// Corrupt tables open or give an `Error`; random bytes without the magic number never open.
fn corrupt_tables(fixture: &[u8], rng: &mut Rng) -> usize {
    let cases = 3000;
    for _ in 0..cases {
        let mut bytes = fixture.to_vec();
        for _ in 0..1 + rng.below(4) {
            let at = rng.below(bytes.len());
            bytes[at] = rng.next() as u8;
        }
        if let Ok(t) = Table::from_bytes(bytes) {
            let words = ["toi", "go", "phim", "Ngo", "", "xyz"];
            assert_eq!(t.restore(&words, ToneStyle::New).len(), words.len());
            exercise(Typing::new(&t), rng);
        }
    }
    for _ in 0..500 {
        let mut bytes = rng.bytes(200);
        let magic = rng.below(2) == 0 && bytes.len() >= 8;
        if magic {
            bytes[..8].copy_from_slice(&fixture[..8]);
        }
        let opened = Table::from_bytes(bytes.clone()).is_ok();
        assert!(!opened || bytes.starts_with(&fixture[..8]), "{bytes:?}");
    }
    cases + 500
}

fn learned_file(table: &Arc<Table>) -> Vec<u8> {
    let mut d = Dictionary::new(Arc::clone(table));
    for line in fs::read_to_string(LEARNED).unwrap().lines() {
        let run: Vec<Option<&str>> = line.split(' ').map(Some).collect();
        d.learn(&run);
    }
    let mut file = Vec::new();
    d.save(&mut file).unwrap();
    file
}

/// Truncated and corrupt personal files load or give an `Error`; invalid UTF-8 is always an
/// `Error`.
fn broken_personal_files(table: &Arc<Table>, rng: &mut Rng) -> usize {
    let file = learned_file(table);
    let load = |bytes: &[u8]| Personal::load_shared(Arc::clone(table), bytes);
    let mut cases = 0;
    for n in 0..file.len() {
        if let Ok(p) = load(&file[..n]) {
            exercise(Typing::with_shared_personal(Arc::new(p)), rng);
        }
        cases += 1;
    }
    for _ in 0..2000 {
        let mut bytes = file.clone();
        for _ in 0..1 + rng.below(4) {
            let at = rng.below(bytes.len());
            bytes[at] = rng.next() as u8;
        }
        if let Ok(p) = load(&bytes) {
            exercise(Typing::with_shared_personal(Arc::new(p)), rng);
        }
        if std::str::from_utf8(&bytes).is_err() {
            assert!(load(&bytes).is_err(), "invalid UTF-8 loaded: {bytes:?}");
        }
        cases += 1;
    }
    cases
}

/// A personal file that does not load is left byte for byte, whatever is done with the dictionary.
/// Every file with a byte that is not UTF-8 is one of them.
fn kept_personal_files(table: &Arc<Table>, path: &Path, rng: &mut Rng) -> usize {
    let file = learned_file(table);
    let mut cases = 0;
    for i in 0..300 {
        let mut bytes = file.clone();
        let at = HEADER.len() + rng.below(bytes.len() - HEADER.len());
        bytes[at] = [0xff, b'\t', b'\n', b'x'][i % 4];
        bytes.truncate(bytes.len() - rng.below((bytes.len() - at) / 2));
        fs::write(path, &bytes).unwrap();
        let mut d = Dictionary::open(Arc::clone(table), path);
        let invalid = std::str::from_utf8(&bytes).is_err();
        assert!(!invalid || d.problem().is_some(), "invalid UTF-8 loaded");
        if d.problem().is_none() {
            continue;
        }
        d.learn(&[Some("ngó"), Some("sen")]);
        d.clear();
        assert!(d.begin_save().is_none());
        assert!(d.reload_file().is_err());
        assert_eq!(fs::read(path).unwrap(), bytes, "a broken file changed");
        cases += 1;
    }
    assert!(cases >= 75, "only {cases} broken files");
    cases
}

fn typing_in_any_order(table: &Arc<Table>, rng: &mut Rng) -> usize {
    let cases = 10_000;
    let mut typing = Typing::shared(Arc::clone(table));
    let mut pushed = 0usize;
    for _ in 0..cases {
        match rng.below(5) {
            0 | 1 => {
                typing.push(rng.word());
                pushed += 1;
            }
            2 => {
                typing.pop();
                pushed = pushed.saturating_sub(1);
            }
            3 => {
                let ended = rng.below(2) == 0;
                let words = if ended {
                    typing.finish(ToneStyle::Old)
                } else {
                    typing.guess(ToneStyle::New)
                };
                assert_eq!(words.len(), pushed);
            }
            _ => {
                let last = [0, 1, 3, usize::MAX][rng.below(4)];
                let choices = typing.choices(last, rng.below(2) == 0, ToneStyle::New);
                assert_eq!(choices.len(), last.min(pushed));
            }
        }
        assert_eq!(typing.len(), pushed);
    }
    cases
}

/// Pops, pushes, real changes and made-up ones, going on in a new run at the cap: a change is taken
/// exactly when its old text stands where it says and it leaves a word kept as typed alone, and
/// nothing else moves the field.
fn runs_in_any_order(table: &Arc<Table>, rng: &mut Rng) -> usize {
    let cases = 10_000;
    let mut run = Run::new(Typing::shared(Arc::clone(table)), 0.99).fixing(Some(FIX_MARGIN));
    let mut field: Vec<String> = Vec::new();
    // Each word as pushed, and whether an undone fix keeps it so.
    let mut typed: Vec<(String, bool)> = Vec::new();
    let mut full = 0;
    for _ in 0..cases {
        if run.is_full() || rng.below(500) == 0 {
            full += usize::from(run.is_full());
            run = run.next(Typing::shared(Arc::clone(table)));
            field.clear();
            typed.clear();
        }
        // Pushes outnumber pops, so runs reach the cap.
        let change = match rng.below(5) {
            0 | 4 => {
                let w = rng.word();
                run.push(w);
                field.push(w.to_string());
                typed.push((w.to_string(), false));
                None
            }
            1 => {
                // Backspace, or the key that undoes a fix: neither moves the field by itself.
                if rng.below(2) == 0 {
                    run.pop();
                    field.pop();
                    typed.pop();
                } else if let Some(i) = run.last_fix() {
                    assert!(run.undo_fix());
                    typed[i].1 = true;
                }
                None
            }
            2 => {
                let n = field.len();
                let first = [0, n, n.saturating_sub(1), usize::MAX][rng.below(4)];
                let old = match field.get(first) {
                    Some(w) if rng.below(2) == 0 => vec![w.clone()],
                    _ => vec![rng.word().to_string(); rng.below(3)],
                };
                Some(Change {
                    first,
                    old,
                    new: vec![rng.word().to_string(); rng.below(3)],
                })
            }
            _ => run.change(rng.below(2) == 0, Reach::Run, ToneStyle::New),
        };
        if let Some(c) = change {
            let end = c.first.checked_add(c.old.len());
            let stands = end.and_then(|end| field.get(c.first..end)) == Some(c.old.as_slice());
            let keeps =
                |(k, new): (usize, &String)| matches!(&typed[c.first + k], (w, true) if w != new);
            let fits = stands && c.old.len() == c.new.len() && !c.new.iter().enumerate().any(keeps);
            assert_eq!(run.apply(&c), fits, "{c:?} on {field:?}");
            if fits {
                field[c.first..c.first + c.new.len()].clone_from_slice(&c.new);
            }
        }
        assert_eq!(run.shown(), field.as_slice());
    }
    assert!(full > 0, "no run reached the cap");
    cases
}

/// A dictionary kept in a file, with learning, clears, reloads and saves interleaved; a save that
/// reaches the file always leaves one that loads, and no temporary file is left.
fn dictionary_in_any_order(table: &Arc<Table>, path: &Path, rng: &mut Rng) -> usize {
    let cases = 3000;
    let tmp = path.with_file_name(".personal.txt.tmp");
    let mut d = Dictionary::open(Arc::clone(table), path);
    let mut job = None;
    for _ in 0..cases {
        match rng.below(7) {
            0 | 1 => {
                let run: Vec<Option<&str>> = (0..rng.below(6))
                    .map(|_| (rng.below(4) != 0).then(|| rng.word()))
                    .collect();
                d.learn(&run);
            }
            2 => d.clear(),
            3 => {
                let before = d.snapshot();
                let garbage = rng.bytes(64);
                if d.reload(garbage.as_slice()).is_err() {
                    assert!(
                        Arc::ptr_eq(&before, &d.snapshot()),
                        "a failed reload changed it"
                    );
                }
            }
            4 => assert!(
                d.reload_file().is_ok(),
                "the file the dictionary saved must load"
            ),
            5 => job = job.or_else(|| d.begin_save()),
            _ => {
                if let Some(j) = job.take() {
                    let written = j.write();
                    let replaced = d.finish_save(j, written).unwrap();
                    assert!(!tmp.exists(), "a temporary file was left");
                    if replaced {
                        let saved = fs::read(path).unwrap();
                        assert!(Personal::load_shared(Arc::clone(table), saved.as_slice()).is_ok());
                    }
                }
            }
        }
        assert!(d.problem().is_none());
    }
    cases
}
