//! Learning, clearing and reloading interleaved with two fields typing: each run restores exactly as
//! a run on a dictionary holding only what was known when it started.

use std::sync::Arc;

use proptest::prelude::*;
use timsen::{Dictionary, Personal, Table, ToneStyle, Typing};

fn table() -> Table {
    Table::open(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/tests/fixtures/tiny.timsen"
    ))
    .unwrap()
}

/// What the dictionary holds: the file last reloaded, if any, then the runs learned since.
#[derive(Clone, Debug, Default)]
struct Known {
    file: Option<Vec<u8>>,
    runs: Vec<Vec<Option<String>>>,
}

impl Known {
    fn personal<'t>(&self, t: &'t Table) -> Personal<'t> {
        let mut p = match &self.file {
            Some(file) => Personal::load(t, file.as_slice()).unwrap(),
            None => Personal::new(t),
        };
        for run in &self.runs {
            let run: Vec<Option<&str>> = run.iter().map(Option::as_deref).collect();
            p.learn(&run);
        }
        p
    }
}

#[derive(Clone, Debug)]
enum Op {
    Learn(Vec<Option<String>>),
    Clear,
    Save,
    Reload,
    Push(usize, String),
    Guess(usize),
    Choices(usize),
    End(usize),
}

fn learned() -> impl Strategy<Value = String> {
    let words = vec!["ngó", "sen", "gõ", "gỗ", "nhà", "quý", "phím", "go", "abc"];
    prop_oneof![
        prop::sample::select(words).prop_map(str::to_string),
        "[a-zđ]{1,4}"
    ]
}

fn typed() -> impl Strategy<Value = String> {
    let words = vec!["ngo", "sen", "go", "nha", "quy", "phim", "abc", "gõ"];
    prop_oneof![
        3 => prop::sample::select(words).prop_map(str::to_string),
        1 => "[a-z]{1,4}"
    ]
}

fn op() -> impl Strategy<Value = Op> {
    prop_oneof![
        3 => prop::collection::vec(prop::option::weighted(0.9, learned()), 1..5).prop_map(Op::Learn),
        1 => Just(Op::Clear),
        1 => Just(Op::Save),
        1 => Just(Op::Reload),
        6 => (0..2usize, typed()).prop_map(|(f, s)| Op::Push(f, s)),
        2 => (0..2usize).prop_map(Op::Guess),
        2 => (0..2usize).prop_map(Op::Choices),
        2 => (0..2usize).prop_map(Op::End),
    ]
}

/// A field's run, and what the dictionary held when the run started.
struct Field {
    typing: Typing<'static>,
    known: Known,
    typed: Vec<String>,
}

impl Field {
    fn start(d: &Dictionary, known: &Known) -> Field {
        Field {
            typing: d.typing(),
            known: known.clone(),
            typed: Vec::new(),
        }
    }

    /// The same run on a dictionary built from scratch with what this run started with.
    fn check(&self, t: &Table, op: &Op) -> Result<(), TestCaseError> {
        let p = self.known.personal(t);
        let mut want = Typing::with_personal(&p);
        for s in &self.typed {
            want.push(s);
        }
        let style = ToneStyle::New;
        match op {
            Op::Guess(_) => prop_assert_eq!(self.typing.guess(style), want.guess(style)),
            Op::Choices(_) => {
                let last = self.typed.len();
                prop_assert_eq!(
                    self.typing.choices(last, false, style),
                    want.choices(last, false, style)
                );
            }
            _ => prop_assert_eq!(self.typing.finish(style), want.finish(style)),
        }
        Ok(())
    }
}

proptest! {
    #[test]
    fn each_run_reads_the_snapshot_taken_when_it_started(ops in prop::collection::vec(op(), 1..40)) {
        let t = table();
        let mut d = Dictionary::new(Arc::new(table()));
        let mut known = Known::default();
        let mut saved: Option<(Vec<u8>, Known)> = None;
        let mut fields = [Field::start(&d, &known), Field::start(&d, &known)];
        for op in &ops {
            match op {
                Op::Learn(run) => {
                    let words: Vec<Option<&str>> = run.iter().map(Option::as_deref).collect();
                    d.learn(&words);
                    known.runs.push(run.clone());
                }
                Op::Clear => {
                    d.clear();
                    known = Known::default();
                }
                Op::Save => {
                    let mut file = Vec::new();
                    d.save(&mut file).unwrap();
                    saved = Some((file, known.clone()));
                }
                Op::Reload => {
                    let file = saved.as_ref().map_or(b"timsen-personal 1\n".to_vec(), |s| s.0.clone());
                    d.reload(file.as_slice()).unwrap();
                    known = Known { file: Some(file), runs: Vec::new() };
                }
                Op::Push(f, s) => {
                    fields[*f].typing.push(s);
                    fields[*f].typed.push(s.clone());
                }
                Op::Guess(f) | Op::Choices(f) => fields[*f].check(&t, op)?,
                Op::End(f) => {
                    fields[*f].check(&t, op)?;
                    fields[*f] = Field::start(&d, &known);
                }
            }
        }
        for field in &fields {
            field.check(&t, &Op::End(0))?;
        }
        // A reload restores like the dictionary that was saved, not only like the file it read.
        if let Some((file, then)) = saved {
            let typed = ["ngo", "sen", "go", "nha"];
            let loaded = Personal::load(&t, file.as_slice()).unwrap();
            prop_assert_eq!(
                loaded.restore(&typed, ToneStyle::New),
                then.personal(&t).restore(&typed, ToneStyle::New)
            );
        }
    }
}
