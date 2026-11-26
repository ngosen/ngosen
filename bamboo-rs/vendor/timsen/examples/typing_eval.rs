//! Accuracy and speed of restoring while typing.
//!
//! usage: cargo run --release --example typing_eval TABLE RUNS TRUTH
//! RUNS and TRUTH come from `research.crosscheck`. Each run is typed one syllable at a time; after
//! each syllable the whole guess and the choices for the last few words are asked for and timed.
//! "Fixed after k" scores each word as it was shown once k more syllables had been typed (the end of
//! the run counts as known). Confidence is scored twice: for each word when it is first shown, and
//! for the finished run.

use std::time::{Duration, Instant};

use timsen::{Choices, Table, ToneStyle, Typing};

const WINDOWS: usize = 5;
const CHOICE_WORDS: usize = 3;
const THRESHOLDS: [f64; 5] = [0.5, 0.7, 0.9, 0.95, 0.99];

fn read_runs(path: &str) -> std::io::Result<Vec<Vec<String>>> {
    let text = std::fs::read_to_string(path)?;
    Ok(text
        .lines()
        .map(|l| l.split(' ').map(str::to_string).collect())
        .collect())
}

fn percentile(sorted: &[Duration], p: f64) -> f64 {
    let i = ((sorted.len() - 1) as f64 * p).round() as usize;
    sorted[i].as_secs_f64() * 1e3
}

/// How many times word j changes after it is first shown, including the final restoration.
fn changes(shown: &[Vec<String>], last: &[String], j: usize) -> usize {
    let mut seen = &shown[j][j];
    let mut n = 0;
    for word in shown[j + 1..].iter().map(|g| &g[j]).chain([&last[j]]) {
        if word != seen {
            n += 1;
            seen = word;
        }
    }
    n
}

/// Counts for one confidence threshold.
#[derive(Default)]
struct Sure {
    sure: usize,
    sure_right: usize,
    unsure_in_top3: usize,
    changed: usize,
    changed_flagged: usize,
}

impl Sure {
    fn add(&mut self, c: &Choices, truth: &str, threshold: f64, changed: bool) {
        if c.confidence >= threshold {
            self.sure += 1;
            self.sure_right += usize::from(c.spellings[0] == truth);
        } else {
            self.unsure_in_top3 += usize::from(c.spellings.iter().any(|s| s == truth));
            self.changed_flagged += usize::from(changed);
        }
        self.changed += usize::from(changed);
    }

    fn print(&self, threshold: f64, words: usize) {
        let pct = |n: usize, d: usize| 100.0 * n as f64 / d.max(1) as f64;
        print!(
            "    >= {threshold:.2}: sure {:.2}%, right when sure {:.2}%, unsure in top 3 {:.2}%",
            pct(self.sure, words),
            pct(self.sure_right, self.sure),
            pct(self.unsure_in_top3, words - self.sure),
        );
        if self.changed > 0 {
            print!(
                ", changed words flagged {:.2}%",
                pct(self.changed_flagged, self.changed)
            );
        }
        println!();
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [table, runs, truth] = &args[..] else {
        return Err("usage: typing_eval TABLE RUNS TRUTH".into());
    };
    let start = Instant::now();
    let table = Table::open(table)?;
    let open = start.elapsed();
    let (runs, truth) = (read_runs(runs)?, read_runs(truth)?);
    let (mut words, mut right, mut whole) = (0, [0usize; WINDOWS], 0);
    let (mut changed, mut total_changes) = (0, 0);
    let mut at_shown: Vec<Sure> = THRESHOLDS.iter().map(|_| Sure::default()).collect();
    let mut at_end: Vec<Sure> = THRESHOLDS.iter().map(|_| Sure::default()).collect();
    let (mut times, mut choice_times) = (Vec::new(), Vec::new());
    for (run, truth) in runs.iter().zip(&truth) {
        let mut typing = Typing::new(&table);
        let mut shown = Vec::with_capacity(run.len());
        let mut first_choices = Vec::with_capacity(run.len());
        for syllable in run {
            let t = Instant::now();
            typing.push(syllable);
            let guess = typing.guess(ToneStyle::New);
            times.push(t.elapsed());
            let t = Instant::now();
            let mut choices = typing.choices(CHOICE_WORDS, false, ToneStyle::New);
            choice_times.push(t.elapsed());
            shown.push(guess);
            first_choices.push(choices.pop().expect("one choice per word typed"));
        }
        let last = typing.finish(ToneStyle::New);
        let final_choices = typing.choices(run.len(), true, ToneStyle::New);
        for j in 0..run.len() {
            words += 1;
            whole += usize::from(last[j] == truth[j]);
            for (k, r) in right.iter_mut().enumerate() {
                let word = shown.get(j + k).map_or(&last[j], |g| &g[j]);
                *r += usize::from(*word == truth[j]);
            }
            let c = changes(&shown, &last, j);
            changed += usize::from(c > 0);
            total_changes += c;
            for (t, &threshold) in THRESHOLDS.iter().enumerate() {
                at_shown[t].add(&first_choices[j], &truth[j], threshold, c > 0);
                at_end[t].add(&final_choices[j], &truth[j], threshold, false);
            }
        }
    }
    let first = times[0];
    times.sort();
    choice_times.sort();
    let pct = |n: usize| 100.0 * n as f64 / words as f64;
    println!("words {words}  whole run {:.2}%", pct(whole));
    for (k, r) in right.iter().enumerate() {
        println!("  fixed after {k}: {:.2}%", pct(*r));
    }
    println!(
        "  changed after shown: {:.2}% of words, {total_changes} changes",
        pct(changed)
    );
    println!("  confidence when first shown:");
    for (s, &t) in at_shown.iter().zip(&THRESHOLDS) {
        s.print(t, words);
    }
    println!("  confidence for the finished run:");
    for (s, &t) in at_end.iter().zip(&THRESHOLDS) {
        s.print(t, words);
    }
    let ms = |d: Duration| d.as_secs_f64() * 1e3;
    println!(
        "  open {:.3} ms, first syllable {:.3} ms",
        ms(open),
        ms(first)
    );
    for (name, t) in [("per syllable", &times), ("choices", &choice_times)] {
        println!(
            "  {name} (ms, {} samples): p50 {:.3} p95 {:.3} p99 {:.3} max {:.3}",
            t.len(),
            percentile(t, 0.5),
            percentile(t, 0.95),
            percentile(t, 0.99),
            percentile(t, 1.0)
        );
    }
    Ok(())
}
