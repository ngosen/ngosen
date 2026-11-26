//! Cost of opening a table: time to open, resident memory, and the first and later keys.
//!
//! usage: cargo run --release --example open_cost TABLE RUNS [HOLD]
//! HOLD is how each run holds the table: `borrowed` (the default), `shared` (an `Arc`), or an empty
//! personal dictionary on top, `personal` (borrowed) or `shared-personal`.
//! RUNS comes from `research.crosscheck`. Each run is typed one syllable at a time, as in
//! `typing_eval`; the first key after opening is reported apart from the rest. Linux only, since
//! resident memory is read from `/proc/self/status`.

use std::sync::Arc;
use std::time::{Duration, Instant};

use timsen::{Personal, Table, ToneStyle, Typing};

fn resident_mb() -> f64 {
    let status = std::fs::read_to_string("/proc/self/status").unwrap_or_default();
    let kb = status
        .lines()
        .find_map(|l| l.strip_prefix("VmRSS:"))
        .and_then(|v| v.trim().trim_end_matches(" kB").trim().parse::<f64>().ok())
        .unwrap_or(f64::NAN);
    kb / 1024.0
}

fn ms(d: Duration) -> f64 {
    d.as_secs_f64() * 1e3
}

fn percentile(sorted: &[Duration], p: f64) -> f64 {
    ms(sorted[((sorted.len() - 1) as f64 * p).round() as usize])
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let (table, runs, hold) = match args.as_slice() {
        [_, t, r] => (t, r, "borrowed"),
        [_, t, r, h] => (t, r, h.as_str()),
        _ => {
            eprintln!("usage: open_cost TABLE RUNS [HOLD]");
            std::process::exit(2);
        }
    };
    let text = std::fs::read_to_string(runs).expect("cannot read runs");
    let before = resident_mb();
    let start = Instant::now();
    let t = Arc::new(Table::open(table).expect("cannot open table"));
    let open = start.elapsed();
    let opened = resident_mb();
    let personal = Personal::new(&t);
    let shared_personal = Arc::new(Personal::shared(Arc::clone(&t)));

    let mut keys = Vec::new();
    let mut words = 0;
    for run in text.lines() {
        let mut typing = match hold {
            "borrowed" => Typing::new(&t),
            "shared" => Typing::shared(Arc::clone(&t)),
            "personal" => Typing::with_personal(&personal),
            "shared-personal" => Typing::with_shared_personal(Arc::clone(&shared_personal)),
            _ => panic!("unknown HOLD {hold}"),
        };
        for syllable in run.split(' ') {
            let start = Instant::now();
            typing.push(syllable);
            words += typing.guess(ToneStyle::New).len();
            keys.push(start.elapsed());
        }
    }
    let first = keys[0];
    let mut rest = keys[1..].to_vec();
    rest.sort();
    println!("open {:.2} ms, first key {:.3} ms", ms(open), ms(first));
    println!(
        "later keys {}: p50 {:.3} p99 {:.3} max {:.3} ms",
        rest.len(),
        percentile(&rest, 0.5),
        percentile(&rest, 0.99),
        ms(*rest.last().unwrap())
    );
    println!(
        "resident MB: before open {before:.1}, after open {opened:.1}, after typing {:.1}",
        resident_mb()
    );
    println!("guessed words (checksum) {words}");
}
