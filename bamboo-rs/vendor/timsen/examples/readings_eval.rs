//! Words the user typed with marks, and words the input method could read two ways.
//!
//! usage: cargo run --release --example readings_eval TABLE RUNS TRUTH
//! RUNS and TRUTH come from `research.crosscheck`.
//!
//! Mixed input: every other word keeps its marks (the rest are typed plain); the plain words are
//! scored when the marked ones are kept as context, against guessing every word.
//!
//! Two readings: each word of TRUTH is typed in Telex through the Ngó Sen engine. A marked word is
//! typed as its Telex keys, so its raw reading is those keys (`tieengs`); an unmarked word is typed
//! as is, so its interpreted reading is whatever the engine makes of it (`test` → `tét`). The run is
//! restored with both readings; a word switches to its raw reading only when that is the best choice
//! with at least the threshold's confidence, and is otherwise kept, with the raw reading offered as
//! a suggestion when it ranks first.

use bamboo_core::{Config, Engine, InputMethod, Mode};
use timsen::{Table, ToneStyle, Typing};

const THRESHOLDS: [f64; 4] = [0.5, 0.9, 0.99, 0.999];

fn read_runs(path: &str) -> std::io::Result<Vec<Vec<String>>> {
    let text = std::fs::read_to_string(path)?;
    Ok(text
        .lines()
        .map(|l| l.split(' ').map(str::to_string).collect())
        .collect())
}

/// Each marked vowel or `đ`: its plain letter, the Telex keys for its shape, and its tone.
fn telex_char(c: char) -> Option<(char, &'static str, Option<char>)> {
    const ROWS: [(&str, char, &str); 13] = [
        ("aàáảãạ", 'a', ""),
        ("ăằắẳẵặ", 'a', "w"),
        ("âầấẩẫậ", 'a', "a"),
        ("eèéẻẽẹ", 'e', ""),
        ("êềếểễệ", 'e', "e"),
        ("iìíỉĩị", 'i', ""),
        ("oòóỏõọ", 'o', ""),
        ("ôồốổỗộ", 'o', "o"),
        ("ơờớởỡợ", 'o', "w"),
        ("uùúủũụ", 'u', ""),
        ("ưừứửữự", 'u', "w"),
        ("yỳýỷỹỵ", 'y', ""),
        ("đđđđđđ", 'd', "d"),
    ];
    const TONES: [char; 5] = ['f', 's', 'r', 'x', 'j'];
    ROWS.iter().find_map(|(row, base, keys)| {
        let t = row.chars().position(|r| r == c)?;
        Some((*base, *keys, t.checked_sub(1).map(|t| TONES[t])))
    })
}

/// The Telex keys for a word: shapes after their letter, the tone at the end.
fn telex_keys(word: &str) -> String {
    let (mut keys, mut tone) = (String::new(), None);
    for c in word.chars() {
        let lower = c.to_lowercase().next().unwrap_or(c);
        match telex_char(lower) {
            Some((base, shape, t)) => {
                keys.push(if c.is_uppercase() {
                    base.to_ascii_uppercase()
                } else {
                    base
                });
                keys.push_str(shape);
                tone = t.or(tone);
            }
            None => keys.push(c),
        }
    }
    keys.extend(tone);
    keys
}

fn type_keys(engine: &mut Engine, keys: &str) -> String {
    engine.reset();
    engine.process(keys, Mode::Vietnamese)
}

fn mixed(table: &Table, runs: &[Vec<String>], truth: &[Vec<String>]) {
    let (mut plain, mut alone, mut with_marks) = (0, 0, 0);
    for (run, truth) in runs.iter().zip(truth) {
        let all = table.restore(
            &run.iter().map(String::as_str).collect::<Vec<_>>(),
            ToneStyle::New,
        );
        let mut typing = Typing::new(table);
        for (j, (r, t)) in run.iter().zip(truth).enumerate() {
            typing.push(if j % 2 == 0 { t } else { r });
        }
        let kept = typing.finish(ToneStyle::New);
        for j in (1..run.len()).step_by(2) {
            plain += 1;
            alone += usize::from(all[j] == truth[j]);
            with_marks += usize::from(kept[j] == truth[j]);
        }
    }
    let pct = |n: usize| 100.0 * n as f64 / plain as f64;
    println!(
        "mixed: {plain} plain words, right {:.2}% when every word is guessed, {:.2}% next to marked words",
        pct(alone),
        pct(with_marks)
    );
}

#[derive(Default)]
struct Outcome {
    fixed: usize,
    broken: usize,
    suggested_right: usize,
    suggested_wrong: usize,
}

fn two_readings(table: &Table, truth: &[Vec<String>]) {
    // The engine's "standard" placement is `hòa`; the test sets use `hoà`.
    let config = Config {
        std_tone_style: false,
        ..Config::default()
    };
    let mut engine = Engine::with_config(InputMethod::telex(), config);
    let (mut words, mut misread_marked, mut two, mut mangled, mut mangled_known) = (0, 0, 0, 0, 0);
    let mut outcomes: Vec<Outcome> = THRESHOLDS.iter().map(|_| Outcome::default()).collect();
    for run in truth {
        let mut typing = Typing::new(table);
        let mut readings = Vec::with_capacity(run.len());
        for word in run {
            words += 1;
            let keys = telex_keys(word);
            let interpreted = type_keys(&mut engine, &keys);
            misread_marked += usize::from(keys != *word && interpreted != *word);
            typing.push_either(&interpreted, &keys);
            readings.push((interpreted, keys));
        }
        let choices = typing.choices(run.len(), true, ToneStyle::New);
        for ((word, (interpreted, raw)), c) in run.iter().zip(&readings).zip(&choices) {
            mangled += usize::from(interpreted != word && raw == word);
            if c.spellings.len() < 2 {
                continue;
            }
            two += 1;
            mangled_known += usize::from(interpreted != word && raw == word);
            let raw_first = c.spellings[0] == *raw;
            for (o, &t) in outcomes.iter_mut().zip(&THRESHOLDS) {
                let out = if raw_first && c.confidence >= t {
                    raw
                } else {
                    interpreted
                };
                o.fixed += usize::from(interpreted != word && out == word);
                o.broken += usize::from(interpreted == word && out != word);
                if raw_first && c.confidence < t {
                    o.suggested_right += usize::from(raw == word);
                    o.suggested_wrong += usize::from(raw != word);
                }
            }
        }
    }
    println!(
        "two readings: {words} words; {misread_marked} marked words the keys do not give back; \
         {mangled} unmarked words the engine changes, {mangled_known} of them known to the table; \
         {two} words with a raw reading the table knows"
    );
    for (o, t) in outcomes.iter().zip(THRESHOLDS) {
        println!(
            "  >= {t}: fixed {}, broken {}, suggested right {}, suggested wrong {}",
            o.fixed, o.broken, o.suggested_right, o.suggested_wrong
        );
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [table, runs, truth] = &args[..] else {
        return Err("usage: readings_eval TABLE RUNS TRUTH".into());
    };
    let table = Table::open(table)?;
    let (runs, truth) = (read_runs(runs)?, read_runs(truth)?);
    mixed(&table, &runs, &truth);
    two_readings(&table, &truth);
    Ok(())
}
