//! How many words the table takes back from a core's reading of mixed Vietnamese and English text.
//!
//! usage: cargo run --release --example mixed_eval TABLE READINGS
//! READINGS comes from `research.mixed_readings`. After each run a word switches to its raw reading
//! as `Choices::takes_raw` decides at the threshold. Words the core changed are split by cause: a
//! lost doubled letter (`issue` → `isue`), any other change to a word typed as written (`test` →
//! `tét`), and words whose keys do not give them back at all. A threshold above 1 never switches,
//! so it shows the core alone.

use std::collections::HashMap;

use timsen::{Table, ToneStyle, Typing};

const THRESHOLDS: [f64; 5] = [0.5, 0.9, 0.99, 0.999, 2.0];
/// The threshold whose broken and unfixed words are listed.
const LISTED: f64 = 0.99;
const LISTED_WORDS: usize = 20;

struct Reading {
    truth: String,
    interpreted: String,
    raw: String,
}

#[derive(Clone, Copy, PartialEq)]
enum Cause {
    Right,
    Doubled,
    Changed,
    Keys,
}

impl Reading {
    fn cause(&self) -> Cause {
        if self.interpreted == self.truth {
            Cause::Right
        } else if self.raw != self.truth {
            Cause::Keys
        } else if self.lost_doubled_letter() {
            Cause::Doubled
        } else {
            Cause::Changed
        }
    }

    fn lost_doubled_letter(&self) -> bool {
        let t: Vec<char> = self.truth.chars().collect();
        let i: Vec<char> = self.interpreted.chars().collect();
        t.len() == i.len() + 1
            && (0..t.len() - 1)
                .any(|k| t[k] == t[k + 1] && t[..k] == i[..k] && t[k + 1..] == i[k..])
    }
}

fn read_runs(path: &str) -> std::io::Result<Vec<Vec<Reading>>> {
    let text = std::fs::read_to_string(path)?;
    let mut runs = vec![Vec::new()];
    for line in text.lines() {
        let mut fields = line.split('\t').map(str::to_string);
        match (fields.next(), fields.next(), fields.next()) {
            (Some(truth), Some(interpreted), Some(raw)) => runs.last_mut().unwrap().push(Reading {
                truth,
                interpreted,
                raw,
            }),
            _ => runs.push(Vec::new()),
        }
    }
    Ok(runs)
}

fn print_top(label: &str, counts: HashMap<(&str, &str), usize>) {
    let mut top: Vec<_> = counts.into_iter().collect();
    top.sort_by(|a, b| b.1.cmp(&a.1).then(a.0.cmp(&b.0)));
    let words: Vec<String> = top
        .iter()
        .take(LISTED_WORDS)
        .map(|((truth, out), n)| format!("{truth}→{out}×{n}"))
        .collect();
    println!("  {label}: {}", words.join(", "));
}

/// For each cause: words, and those whose raw reading the table knows.
#[derive(Default, Clone, Copy)]
struct Tally {
    words: usize,
    known: usize,
}

#[derive(Default, Clone, Copy)]
struct Outcome {
    doubled: usize,
    changed: usize,
    keys: usize,
    broken: usize,
    wrong: usize,
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [table, readings] = &args[..] else {
        return Err("usage: mixed_eval TABLE READINGS".into());
    };
    let table = Table::open(table)?;
    let (mut words, mut doubled, mut changed, mut keys) =
        (0, Tally::default(), Tally::default(), Tally::default());
    let mut outcomes = [Outcome::default(); THRESHOLDS.len()];
    let runs = read_runs(readings)?;
    let (mut broken, mut unfixed) = (HashMap::new(), HashMap::new());
    for run in &runs {
        if run.is_empty() {
            continue;
        }
        let mut typing = Typing::new(&table);
        for r in run {
            typing.push_either(&r.interpreted, &r.raw);
        }
        let choices = typing.choices(run.len(), true, ToneStyle::New);
        for (r, c) in run.iter().zip(&choices) {
            words += 1;
            let cause = r.cause();
            let two = c.spellings.len() >= 2;
            if let Some(t) = match cause {
                Cause::Doubled => Some(&mut doubled),
                Cause::Changed => Some(&mut changed),
                Cause::Keys => Some(&mut keys),
                Cause::Right => None,
            } {
                t.words += 1;
                t.known += usize::from(two);
            }
            for (o, &t) in outcomes.iter_mut().zip(&THRESHOLDS) {
                let out = if c.takes_raw(&r.raw, t) {
                    &r.raw
                } else {
                    &r.interpreted
                };
                let right = *out == r.truth;
                if t == LISTED && !right {
                    let seen = if cause == Cause::Right {
                        &mut broken
                    } else {
                        &mut unfixed
                    };
                    *seen.entry((r.truth.as_str(), out.as_str())).or_insert(0) += 1;
                }
                o.wrong += usize::from(!right);
                match cause {
                    Cause::Right => o.broken += usize::from(!right),
                    Cause::Doubled => o.doubled += usize::from(right),
                    Cause::Changed => o.changed += usize::from(right),
                    Cause::Keys => o.keys += usize::from(right),
                }
            }
        }
    }
    println!(
        "{words} words; lost a doubled letter {} ({} known to the table); other change {} ({} known); \
         keys do not give back {}",
        doubled.words, doubled.known, changed.words, changed.known, keys.words
    );
    for (o, t) in outcomes.iter().zip(THRESHOLDS) {
        println!(
            "  >= {t}: fixed doubled {}, other {}, keys {}; broken {}; right {:.2}%",
            o.doubled,
            o.changed,
            o.keys,
            o.broken,
            100.0 * (words - o.wrong) as f64 / words as f64
        );
    }
    print_top("broken at 0.99", broken);
    print_top("still wrong at 0.99", unfixed);
    Ok(())
}
