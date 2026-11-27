//! Checks `Typing::fix` against the decisions `research.typo --write-fixes` wrote for a test set.
//!
//! usage: cargo run --release --example fix_check TABLE FIXES
//! FIXES: RUN, I, MODE, FIX, MARGIN per line, MODE `ended` (the whole run) or `typing` (the run up to
//! the syllable, not over), `-` for no fix. Prints how many decisions agree, how many fall on the
//! other side of `FIX_MARGIN`, and the largest difference in margin.

use timsen::{FIX_MARGIN, Table, Typing};

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let [_, table, fixes] = args.as_slice() else {
        eprintln!("usage: fix_check TABLE FIXES");
        std::process::exit(2);
    };
    let t = Table::open(table).expect("cannot open table");
    let text = std::fs::read_to_string(fixes).expect("cannot read fixes");
    let (mut lines, mut same, mut other_side, mut made, mut differ) = (0, 0, 0, [0, 0], [0, 0]);
    let mut widest = 0.0f64;
    for line in text.lines() {
        let [run, i, mode, want, margin] = line.split('\t').collect::<Vec<_>>()[..] else {
            panic!("bad line: {line}");
        };
        let (i, ended): (usize, bool) = (i.parse().unwrap(), mode == "ended");
        let words: Vec<&str> = run.split(' ').collect();
        let mut typing = Typing::new(&t);
        for w in &words[..if ended { words.len() } else { i + 1 }] {
            typing.push(w);
        }
        let got = typing.fix(i, ended);
        let want_margin: Option<f64> = margin.parse().ok();
        lines += 1;
        let agrees = match (&got, want_margin) {
            (None, None) => want == "-",
            (Some(fix), Some(m)) => {
                widest = widest.max((fix.margin - m).abs());
                other_side += usize::from((fix.margin >= FIX_MARGIN) != (m >= FIX_MARGIN));
                made[usize::from(ended)] += usize::from(m >= FIX_MARGIN);
                fix.word == want
            }
            _ => false,
        };
        if agrees {
            same += 1;
            continue;
        }
        differ[usize::from(ended)] += 1;
        if lines - same <= 5 {
            println!("differs: {line} -> {got:?}");
        }
    }
    println!(
        "{lines} decisions, {same} the same fix, {other_side} on the other side of the margin, \
         largest margin difference {widest:e}; differing: {} typing, {} ended; \
         fixes made at the margin: {} typing, {} ended",
        differ[0], differ[1], made[0], made[1]
    );
}
