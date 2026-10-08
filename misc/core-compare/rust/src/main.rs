//! Rust twin of ../go/main.go: same input, same output columns.

use bamboo_core::{Config, Engine, InputMethod, Mode, OutputOptions};
use std::collections::HashSet;
use std::io::{self, BufRead, BufWriter, Write};

const VOWELS: &str = "aàáảãạăằắẳẵặâầấẩẫậeèéẻẽẹêềếểễệiìíỉĩịoòóỏõọôồốổỗộơờớởỡợuùúủũụưừứửữựyỳýỷỹỵ";

// The crate has no w2u setting; it behaves like Go with w2u off, so cases
// for the comparison use w2u 0.
fn new_engine(im: &str, modern: bool, free: bool, auto_correct: bool) -> Engine {
    let method = match im {
        "Telex" => InputMethod::telex(),
        "VNI" => InputMethod::vni(),
        other => panic!("unknown input method {other}"),
    };
    // Go sets EstdToneStyle when modern style is off; std_tone_style is its port.
    let config = Config::builder()
        .free_tone_marking(free)
        .std_tone_style(!modern)
        .auto_correct(auto_correct)
        .build();
    Engine::with_config(method, config)
}

// Same rules as fallbackToEnglish and mustFallbackToEnglish in ../go/main.go.
fn has_vowel(s: &str) -> bool {
    s.chars().any(|c| VOWELS.contains(c))
}

fn is_vietnamese_rune(c: char) -> bool {
    c == 'đ' || (VOWELS.contains(c) && !"aeiouy".contains(c))
}

fn fallback_to_english(e: &Engine, check_vn_rune: bool) -> bool {
    let vn = e.get_processed_str(OutputOptions::LOWER_CASE);
    if vn.is_empty() {
        return false;
    }
    if !has_vowel(&vn) && (vn.ends_with('d') || vn.contains('đ')) {
        return false;
    }
    if check_vn_rune && !vn.chars().any(is_vietnamese_rune) {
        return false;
    }
    !e.is_valid(false)
}

fn must_fallback_to_english(e: &Engine, dictionary: &HashSet<String>) -> bool {
    let vn = e.get_processed_str(OutputOptions::LOWER_CASE);
    if vn.is_empty() || vn.contains('đ') {
        return false;
    }
    !dictionary.contains(&vn)
}

// The crate has no raw text for committed words, only for the active one.
fn raw(e: &Engine) -> String {
    e.get_processed_str(OutputOptions::RAW)
}

fn shown_and_committed(e: &Engine, dictionary: &HashSet<String>) -> (String, String) {
    let mut shown = e.get_processed_str(OutputOptions::PUNCTUATION_MODE | OutputOptions::FULL_TEXT);
    if fallback_to_english(e, true) {
        shown = raw(e);
    }
    let has_vn_rune = shown.to_lowercase().chars().any(is_vietnamese_rune);
    let committed = if has_vn_rune && must_fallback_to_english(e, dictionary) {
        raw(e)
    } else {
        shown.clone()
    };
    (shown, committed)
}

fn row(id: &str, step: usize, op: &str, e: &Engine, dictionary: &HashSet<String>) -> String {
    let (shown, committed) = shown_and_committed(e, dictionary);
    [
        id.to_string(),
        step.to_string(),
        op.to_string(),
        shown,
        committed,
        e.get_processed_str(OutputOptions::PUNCTUATION_MODE | OutputOptions::FULL_TEXT),
        raw(e),
        e.get_processed_str(OutputOptions::LOWER_CASE),
        e.is_valid(false).to_string(),
        e.is_valid(true).to_string(),
    ]
    .join("\t")
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let Some(path) = args.get(1) else {
        panic!("usage: core-compare DICTIONARY [--no-auto-correct] < cases");
    };
    // Go leaves invalid words to the caller; the crate can do the same.
    let auto_correct = args.get(2).map(String::as_str) != Some("--no-auto-correct");
    let dictionary: HashSet<String> = std::fs::read_to_string(path)
        .expect("read dictionary")
        .split('\n')
        .filter(|l| !l.is_empty())
        .map(str::to_lowercase)
        .collect();
    let stdin = io::stdin();
    let mut out = BufWriter::new(io::stdout().lock());
    for line in stdin.lock().lines() {
        let line = line.expect("read stdin");
        let f: Vec<&str> = line.split('\t').collect();
        if f.len() != 6 {
            continue;
        }
        let mut e = new_engine(f[1], f[2] == "1", f[3] == "1", auto_correct);
        for (i, k) in f[5].chars().enumerate() {
            let op = match k {
                '<' => {
                    // Go's RemoveLastChar drops the last letter with its marks;
                    // the crate's remove_last_char undoes a keystroke instead.
                    e.remove_last_output_char();
                    "bs".to_string()
                }
                '!' => {
                    e.restore_last_word(false);
                    "restore".to_string()
                }
                _ => {
                    if fallback_to_english(&e, false) {
                        e.process_key(k, Mode::English);
                        format!("en:{k}")
                    } else {
                        e.process_key(k, Mode::Vietnamese);
                        format!("vn:{k}")
                    }
                }
            };
            writeln!(out, "{}", row(f[0], i + 1, &op, &e, &dictionary)).expect("write stdout");
        }
    }
}
