//! What an input method pays for Đoán dấu, each part timed alone: opening the table, loading and
//! saving the personal file, a key at word end in runs of a given length, and memory for the table
//! and its peak while a second copy is opened in its place, as on a table update.
//!
//! usage: cargo run --release --example latency TABLE RUNS LEARNED DIR LENGTH...
//! RUNS: unmarked runs from `research.crosscheck`, one per line; their syllables are joined into
//! runs of exactly each LENGTH. LEARNED: text with marks, learned into a personal dictionary kept in
//! DIR. A key is what Ngó Sen does when a word ends: `Run::push`, `Run::change` and `Run::apply`,
//! with fixing on at `FIX_MARGIN`.
//! A LENGTH ending in `x` (`200x`) pushes one word in 20 with two readings (`Run::push_either`)
//! and counts how often the run decides them as the whole run would.
//! Linux only, since memory is read from `/proc/self/status`.

use std::path::Path;
use std::sync::Arc;
use std::time::{Duration, Instant};

use timsen::{Dictionary, FIX_MARGIN, Reach, Run, Table, ToneStyle};

fn status_mb(field: &str) -> f64 {
    let status = std::fs::read_to_string("/proc/self/status").unwrap_or_default();
    let kb = status
        .lines()
        .find_map(|l| l.strip_prefix(field))
        .and_then(|v| v.trim().trim_end_matches(" kB").trim().parse::<f64>().ok())
        .unwrap_or(f64::NAN);
    // Decimal megabytes, as file sizes are given.
    kb * 1024.0 / 1e6
}

fn ms(d: Duration) -> f64 {
    d.as_secs_f64() * 1e3
}

fn timed<T>(f: impl FnOnce() -> T) -> (T, f64) {
    let start = Instant::now();
    let out = f();
    (out, ms(start.elapsed()))
}

fn percentiles(keys: &mut [Duration]) -> String {
    keys.sort();
    let at = |p: f64| ms(keys[((keys.len() - 1) as f64 * p).round() as usize]);
    format!(
        "{} keys: p50 {:.3} p95 {:.3} p99 {:.3} max {:.3} ms",
        keys.len(),
        at(0.5),
        at(0.95),
        at(0.99),
        at(1.0)
    )
}

/// Learns every line of `learned` and saves it to `dir`, then opens it again from there.
fn personal(table: &Arc<Table>, learned: &str, dir: &Path) -> Dictionary {
    let path = dir.join("personal.txt");
    let _ = std::fs::remove_file(&path);
    let mut d = Dictionary::open(Arc::clone(table), &path);
    let text = std::fs::read_to_string(learned).expect("cannot read learned text");
    let mut tokens = 0;
    for line in text.lines() {
        let run: Vec<Option<&str>> = line.split_whitespace().map(Some).collect();
        tokens += run.len();
        d.learn(&run);
    }
    let (saved, save_ms) = timed(|| {
        let job = d.begin_save().expect("a save is owed");
        let written = job.write();
        d.finish_save(job, written)
    });
    assert!(saved.expect("cannot save"));
    let bytes = std::fs::metadata(&path).unwrap().len();
    let (d, load_ms) = timed(|| Dictionary::open(Arc::clone(table), &path));
    assert!(d.problem().is_none());
    println!(
        "personal: {tokens} words learned, file {:.1} MB, save {save_ms:.1} ms, load {load_ms:.1} ms",
        bytes as f64 / 1e6
    );
    d
}

/// One word in 20 read two ways, as an input method reads `test` as `tét` or as typed: here its two
/// likeliest spellings alone, words the table knows, so the context really chooses between them.
fn readings(d: &Dictionary, chunk: &[&str], either: bool) -> Vec<Option<(String, String)>> {
    let pair = |s: &str| {
        let mut one = d.typing();
        one.push(s);
        let c = one.choices(1, false, ToneStyle::New).remove(0);
        (c.spellings.len() >= 2).then(|| (c.spellings[0].clone(), c.spellings[1].clone()))
    };
    let at = |i: usize| either && i % 20 == 3;
    (chunk.iter().enumerate())
        .map(|(i, s)| if at(i) { pair(s) } else { None })
        .collect()
}

fn push(run: &mut Run, s: &str, reading: &Option<(String, String)>) {
    match reading {
        Some((interpreted, raw)) => run.push_either(interpreted, raw),
        None => run.push(s),
    }
}

/// Times each word end in runs of `length` syllables; returns the keys and a hash of the text shown
/// at the end, so two builds can be checked to restore alike.
fn keys(
    d: &Dictionary,
    syllables: &[&str],
    length: usize,
    either: bool,
) -> (Vec<Duration>, Vec<Duration>, u64) {
    let (mut all, mut late) = (Vec::new(), Vec::new());
    let mut checksum = 0xcbf2_9ce4_8422_2325u64;
    for chunk in syllables.chunks_exact(length) {
        let readings = readings(d, chunk, either);
        let mut run = Run::new(d.typing(), 0.99).fixing(Some(FIX_MARGIN));
        for (i, (s, reading)) in chunk.iter().zip(&readings).enumerate() {
            let start = Instant::now();
            push(&mut run, s, reading);
            if let Some(c) = run.change(false, Reach::Run, ToneStyle::New) {
                assert!(run.apply(&c));
            }
            let took = start.elapsed();
            all.push(took);
            if i >= length * 9 / 10 {
                late.push(took);
            }
        }
        if let Some(c) = run.change(true, Reach::Run, ToneStyle::New) {
            assert!(run.apply(&c));
        }
        // FNV-1a over the shown text: a count of letters would not see marks change.
        for b in run.shown().join(" ").bytes() {
            checksum = (checksum ^ u64::from(b)).wrapping_mul(0x100_0000_01b3);
        }
    }
    (all, late, checksum)
}

/// How often a word read two ways ends as it would were the whole run weighed at its end. Returns
/// the words that agree, all of them, and how many end raw in the run and in the whole-run answer.
fn agreement(d: &Dictionary, syllables: &[&str], length: usize, confidence: f64) -> [usize; 4] {
    let mut counts = [0; 4];
    for chunk in syllables.chunks_exact(length) {
        let readings = readings(d, chunk, true);
        let (mut run, mut whole) = (
            Run::new(d.typing(), confidence).fixing(Some(FIX_MARGIN)),
            d.typing(),
        );
        for (s, reading) in chunk.iter().zip(&readings) {
            push(&mut run, s, reading);
            match reading {
                Some((interpreted, raw)) => whole.push_either(interpreted, raw),
                None => whole.push(s),
            }
            if let Some(c) = run.change(false, Reach::Run, ToneStyle::New) {
                assert!(run.apply(&c));
            }
        }
        if let Some(c) = run.change(true, Reach::Run, ToneStyle::New) {
            assert!(run.apply(&c));
        }
        let choices = whole.choices(length, true, ToneStyle::New);
        for (i, reading) in readings.iter().enumerate() {
            let Some((interpreted, raw)) = reading else {
                continue;
            };
            let want = if choices[i].takes_raw(raw, confidence) {
                raw
            } else {
                interpreted
            };
            let shown = &run.shown()[i];
            counts[0] += usize::from(shown == want);
            counts[1] += 1;
            counts[2] += usize::from(shown == raw);
            counts[3] += usize::from(want == raw);
        }
    }
    counts
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let [_, table_path, runs, learned, dir, lengths @ ..] = args.as_slice() else {
        eprintln!("usage: latency TABLE RUNS LEARNED DIR LENGTH...");
        std::process::exit(2);
    };
    let before = status_mb("VmRSS:");
    let (table, open_ms) = timed(|| Arc::new(Table::open(table_path).expect("cannot open table")));
    println!(
        "open {open_ms:.1} ms, resident {:.1} MB more",
        status_mb("VmRSS:") - before
    );
    let d = personal(&table, learned, Path::new(dir));
    let text = std::fs::read_to_string(runs).expect("cannot read runs");
    let syllables: Vec<&str> = text.split_whitespace().collect();
    for length in lengths {
        let (digits, either) = match length.strip_suffix('x') {
            Some(digits) => (digits, true),
            None => (length.as_str(), false),
        };
        let length: usize = digits
            .parse()
            .expect("LENGTH is a number, x for two readings");
        let (mut all, mut late, checksum) = keys(&d, &syllables, length, either);
        let readings = if either { " with two readings" } else { "" };
        println!(
            "runs of {length}{readings}: {}, hash {checksum:016x}",
            percentiles(&mut all)
        );
        println!("  last tenth of each run: {}", percentiles(&mut late));
        for confidence in [0.99, 0.5].into_iter().filter(|_| either) {
            let [agree, all, run, whole] = agreement(&d, &syllables, length, confidence);
            println!(
                "  raw confidence {confidence}: {agree} of {all} agree with the whole run; \
                 raw kept {run} times, by the whole run {whole}"
            );
        }
    }
    let typed = status_mb("VmRSS:");
    // Resets the peak, so it is the peak of the reload and not of learning above.
    std::fs::write("/proc/self/clear_refs", "5").expect("cannot reset the peak");
    assert!(status_mb("VmHWM:") <= typed + 1.0, "the peak did not reset");
    let (again, reopen_ms) = timed(|| Arc::new(Table::open(table_path).unwrap()));
    // The dictionary is read again for the new table, then the old pair goes.
    let new = Dictionary::open(Arc::clone(&again), Path::new(dir).join("personal.txt"));
    drop((d, table));
    println!(
        "resident after typing {:.1} MB; reopening {reopen_ms:.1} ms, peak {:.1} MB, then {:.1} MB",
        typed - before,
        status_mb("VmHWM:") - before,
        status_mb("VmRSS:") - before
    );
    drop(new);
}
