//! Checks `Typing::fix_nearby` against the decisions `research.telex_typo --write-fixes` wrote for
//! a test set.
//!
//! usage: cargo run --release --example nearby_check TABLE FIXES
//! FIXES: BEFORE, SHOWN, NEARBY, FIX, MARGIN per line. BEFORE are the words before the one shown,
//! as typed; NEARBY is `WORD:COST` for each word near its keys; FIX and MARGIN are `-` when no
//! nearby word is common enough. Prints how many decisions agree, how many fall on the other side
//! of `FIX_MARGIN`, and the largest difference in margin.

use timsen::{FIX_MARGIN, Nearby, Table, Typing};

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let [_, table, fixes] = args.as_slice() else {
        eprintln!("usage: nearby_check TABLE FIXES");
        std::process::exit(2);
    };
    let t = Table::open(table).expect("cannot open table");
    let text = std::fs::read_to_string(fixes).expect("cannot read fixes");
    let (mut lines, mut same, mut other_side, mut made) = (0, 0, 0, 0);
    let mut widest = 0.0f64;
    for line in text.lines() {
        let [before, shown, nearby, want, margin] = line.split('\t').collect::<Vec<_>>()[..] else {
            panic!("bad line: {line}");
        };
        let mut typing = Typing::new(&t);
        for w in before.split(' ').filter(|w| !w.is_empty()) {
            // Both readings the same: the word is context as typed, as the research takes it.
            typing.push_either(w, w);
        }
        typing.push(shown);
        let nearby: Vec<Nearby> = (nearby.split(' ').filter(|n| !n.is_empty()))
            .map(|n| {
                let (word, cost) = n.rsplit_once(':').expect("WORD:COST");
                Nearby {
                    word,
                    cost: cost.parse().expect("a cost"),
                }
            })
            .collect();
        let got = typing.fix_nearby(typing.len() - 1, false, &nearby);
        lines += 1;
        let agrees = match (&got, margin.parse::<f64>().ok()) {
            (None, None) => want == "-",
            (Some(fix), Some(m)) => {
                widest = widest.max((fix.margin - m).abs());
                other_side += usize::from((fix.margin >= FIX_MARGIN) != (m >= FIX_MARGIN));
                made += usize::from(m >= FIX_MARGIN);
                fix.word == want
            }
            _ => false,
        };
        if agrees {
            same += 1;
        } else if lines - same <= 5 {
            println!("differs: {line} -> {got:?}");
        }
    }
    println!(
        "{lines} decisions, {same} the same fix, {other_side} on the other side of the margin, \
         largest margin difference {widest:e}; fixes made at the margin: {made}"
    );
}
