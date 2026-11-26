//! Five-fold measurement of the personal dictionary, as `research.personal` does it.
//!
//! usage: cargo run --release --example personal_eval TABLE LINES
//! LINES comes from `research.crosscheck --lines`. Each fold is a contiguous fifth of the lines; the
//! dictionary learns the other four fifths and the fold is restored, with the table alone and with
//! the learned counts. Each syllable of the fold is also typed into `Typing` with the learned counts,
//! and timed.

use std::time::{Duration, Instant};

use timsen::{Personal, Table, ToneStyle, Typing};

const FOLDS: usize = 5;

/// A run as written and unmarked.
type Run = (Vec<String>, Vec<String>);

fn read_lines(path: &str) -> std::io::Result<Vec<Vec<Run>>> {
    let words = |s: &str| s.split(' ').map(str::to_string).collect::<Vec<_>>();
    let text = std::fs::read_to_string(path)?;
    Ok(text
        .lines()
        .map(|line| {
            let fields: Vec<&str> = if line.is_empty() {
                Vec::new()
            } else {
                line.split('\t').collect()
            };
            fields
                .chunks(2)
                .map(|p| (words(p[0]), words(p[1])))
                .collect()
        })
        .collect())
}

fn percentile(sorted: &[Duration], p: f64) -> f64 {
    let i = ((sorted.len() - 1) as f64 * p).round() as usize;
    sorted[i].as_secs_f64() * 1e3
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [table, lines] = &args[..] else {
        return Err("usage: personal_eval TABLE LINES".into());
    };
    let table = Table::open(table)?;
    let lines = read_lines(lines)?;
    let size = lines.len() / FOLDS + 1;
    let (mut words, mut alone, mut learned) = (0usize, 0usize, 0usize);
    let mut times = Vec::new();
    for f in 0..FOLDS {
        let (lo, hi) = (
            (f * size).min(lines.len()),
            ((f + 1) * size).min(lines.len()),
        );
        let mut personal = Personal::new(&table);
        for run in lines[..lo].iter().chain(&lines[hi..]).flatten() {
            let written: Vec<Option<&str>> = run.0.iter().map(|w| Some(w.as_str())).collect();
            personal.learn(&written);
        }
        for (written, plain) in lines[lo..hi].iter().flatten() {
            let plain: Vec<&str> = plain.iter().map(String::as_str).collect();
            let a = table.restore(&plain, ToneStyle::New);
            let b = personal.restore(&plain, ToneStyle::New);
            words += written.len();
            alone += written.iter().zip(&a).filter(|(w, g)| w == g).count();
            learned += written.iter().zip(&b).filter(|(w, g)| w == g).count();
            let mut typing = Typing::with_personal(&personal);
            for syllable in &plain {
                let t = Instant::now();
                typing.push(syllable);
                let _ = typing.guess(ToneStyle::New);
                times.push(t.elapsed());
            }
        }
    }
    times.sort();
    let pct = |n: usize| 100.0 * n as f64 / words as f64;
    println!(
        "words {words}: table alone {:.2}%, with the personal dictionary {:.2}%",
        pct(alone),
        pct(learned)
    );
    println!(
        "per syllable with the personal dictionary (ms, {} samples): p50 {:.3} p95 {:.3} p99 {:.3} max {:.3}",
        times.len(),
        percentile(&times, 0.5),
        percentile(&times, 0.95),
        percentile(&times, 0.99),
        percentile(&times, 1.0)
    );
    Ok(())
}
