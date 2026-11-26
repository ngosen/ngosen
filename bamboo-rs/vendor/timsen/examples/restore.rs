//! Restores one run per input line:
//! `cargo run --release --example restore TABLE [--old] [--learn FILE] < runs`.
//! With `--learn`, each line of FILE is first learned as a run known to be right.

use std::io::{BufRead, Write};

use timsen::{Personal, Table, ToneStyle};

const USAGE: &str = "usage: restore TABLE [--old] [--learn FILE] < runs";

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let path = args.first().ok_or(USAGE)?;
    let style = if args.iter().any(|a| a == "--old") {
        ToneStyle::Old
    } else {
        ToneStyle::New
    };
    let table = Table::open(path)?;
    let mut personal = Personal::new(&table);
    if let Some(i) = args.iter().position(|a| a == "--learn") {
        let learned = std::fs::read_to_string(args.get(i + 1).ok_or(USAGE)?)?;
        for line in learned.lines() {
            let run: Vec<Option<&str>> = line.split_whitespace().map(Some).collect();
            personal.learn(&run);
        }
    }
    let mut out = std::io::BufWriter::new(std::io::stdout().lock());
    for line in std::io::stdin().lock().lines() {
        let line = line?;
        let words: Vec<&str> = line.split_whitespace().collect();
        writeln!(out, "{}", personal.restore(&words, style).join(" "))?;
    }
    Ok(())
}
