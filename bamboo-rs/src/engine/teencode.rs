// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

//! Teencode mode: a syllable written the way people type online (`zậy`, `fải`, `đag`) keeps
//! its marks instead of showing the keys, though no dictionary has it.

use super::*;

/// Loanwords written as one word (`moto`, `caosu`), which would read as one teencode syllable.
const TYPED_WORDS: &str = include_str!("teencode-typed-words.txt");

/// English words whose keys type a teencode word people write more often than the English one
/// (`was` for wá); other English words stay English.
const OVER_ENGLISH: &str = include_str!("teencode-over-english.txt");

const VOWELS: &str = "aăâeêioôơuưy";

const ONSETS: &[&str] = &[
    "", "b", "c", "ch", "d", "đ", "g", "gh", "gi", "h", "k", "kh", "l", "m", "n", "ng", "ngh",
    "nh", "p", "ph", "q", "qu", "r", "s", "t", "th", "tr", "v", "x", "z", "dz", "j", "f", "w",
    "ck", "nk", "dh", "bh",
];

const FINALS: &[&str] = &[
    "", "c", "ch", "m", "n", "ng", "nh", "p", "t", "g", "k", "nk", "h", "ck", "q", "nq", "x", "z",
    "j", "w",
];

/// What an onset, rime or final written in teencode stands for in a dictionary word.
const ONSET_SWAPS: &[(&str, &[&str])] = &[
    ("z", &["v", "d", "gi", "r"]),
    ("dz", &["v", "d", "gi"]),
    ("p", &["b"]),
    ("ch", &["tr"]),
    ("tr", &["ch"]),
    ("g", &["r"]),
    ("q", &["qu"]),
    ("j", &["gi"]),
    ("f", &["ph"]),
    ("k", &["c", "kh"]),
    ("d", &["v"]),
    ("c", &["k"]),
    ("w", &["qu"]),
];

const RIME_SWAPS: &[(&str, &[&str])] = &[
    ("ưi", &["ươi"]),
    ("iu", &["iêu"]),
    ("oi", &["ôi"]),
    ("oy", &["ôi", "oi"]),
    ("ôy", &["ôi"]),
    ("ui", &["ôi", "uôi"]),
    ("oa", &["o", "a"]),
    ("oe", &["e", "eo"]),
    ("ia", &["e"]),
    ("ê", &["i"]),
    ("i", &["y", "iê"]),
    ("u", &["ươ", "uô"]),
    ("ư", &["ươ", "i"]),
    ("ơ", &["ươ"]),
    ("e", &["a", "ê"]),
    ("y", &["i"]),
    ("uy", &["i"]),
    ("o", &["ô", "u"]),
];

const FINAL_SWAPS: &[(&str, &[&str])] = &[
    ("g", &["ng"]),
    ("k", &["c", "ch"]),
    ("nk", &["nh", "ng"]),
    ("h", &["nh", ""]),
    ("m", &["ng"]),
    ("n", &["ng", "nh"]),
    ("ck", &["c", "ch"]),
];

/// The onset, vowels and final of one syllable without tones, or None if it is not one.
fn syllable_parts(plain: &str) -> Option<(String, String, String)> {
    let chars: Vec<char> = plain.chars().collect();
    let start = chars.iter().position(|c| VOWELS.contains(*c))?;
    let end = start
        + chars[start..]
            .iter()
            .take_while(|c| VOWELS.contains(**c))
            .count();
    if chars[end..].iter().any(|c| VOWELS.contains(*c)) {
        return None;
    }
    let mut onset: String = chars[..start].iter().collect();
    let mut vowels: String = chars[start..end].iter().collect();
    // gi and qu read as onsets when another vowel follows.
    for (letter, glide) in [("g", 'i'), ("q", 'u')] {
        if onset == letter && vowels.chars().count() > 1 && vowels.starts_with(glide) {
            onset.push(glide);
            vowels.remove(0);
        }
    }
    Some((onset, vowels, chars[end..].iter().collect()))
}

/// Whether a lower-case word reads as one syllable typed in teencode.
fn is_teencode_shaped(word: &str) -> bool {
    let (plain, tones) = without_tones(word);
    let tripled = plain
        .chars()
        .collect::<Vec<_>>()
        .windows(3)
        .any(|w| w[0] == w[1] && w[1] == w[2]);
    if tones > 1 || tripled || word.is_ascii() {
        return false;
    }
    syllable_parts(&plain).is_some_and(|(onset, vowels, last)| {
        ONSETS.contains(&onset.as_str())
            && FINALS.contains(&last.as_str())
            && vowels.chars().count() <= 3
    })
}

/// Whether the word spends every key: each letter comes from a key, in order, and no key is left
/// over beyond those the marks and the tone take. A tangle such as `caras` for cấ loses the `r`.
fn spends_every_key(keys: &str, word: &str) -> bool {
    let (letters, tones) = without_tones(word);
    let plain = without_diacritics(&letters);
    let marks = letters
        .chars()
        .zip(plain.chars())
        .filter(|(a, b)| a != b)
        .count();
    let mut rest = keys.chars();
    plain.chars().all(|letter| rest.any(|key| key == letter))
        && keys.chars().count() <= plain.chars().count() + marks + tones
}

impl BambooEngine {
    /// The teencode word the keys type, when the mode is on and nothing else claims them.
    pub(super) fn teencode_word(&self) -> Option<String> {
        if !self.teencode || !self.auto_non_vn_restore {
            return None;
        }
        let raw = self.raw_text();
        if !raw.chars().all(|c| c.is_alphabetic())
            || self.get_processed_string(OutputOptions::RAW) != raw
        {
            return None;
        }
        // A dictionary word the keys ask for wins: `tienf` is tiền, not tìen.
        if self.retoned_word().is_some()
            || self.auto_marked_word().is_some()
            || self.late_tone_word().is_some()
        {
            return None;
        }
        let word = self
            .shaped_word_of(raw.chars().collect())
            .or_else(|| self.shaped_word_with_late_tone(&raw))?;
        let keys = raw.to_lowercase();
        if TYPED_WORDS.lines().any(|typed| typed == keys) {
            return None;
        }
        if self.english_words.contains(&keys) && !OVER_ENGLISH.lines().any(|over| over == keys) {
            return None;
        }
        // A doubled a, e or o at the end (`nhaa`) more likely stretches the word, unless the word
        // swaps letters of a dictionary word as teencode does (`zoo` for vô).
        let stretched = ["aa", "ee", "oo"].iter().any(|end| keys.ends_with(end));
        if stretched && !self.stands_for_word(&word.to_lowercase()) {
            return None;
        }
        Some(word)
    }

    /// The teencode-shaped syllable `keys` compose, unless it is a dictionary word.
    fn shaped_word_of(&self, keys: Vec<char>) -> Option<String> {
        let typed_keys: String = keys.iter().collect::<String>().to_lowercase();
        let typed = self.typed_alone(keys);
        let text = typed.get_processed_str(OutputOptions::NONE);
        let lower = text.to_lowercase();
        (typed.get_processed_str(OutputOptions::FULL_TEXT) == text
            && is_teencode_shaped(&lower)
            && spends_every_key(&typed_keys, &lower)
            && !self.dictionary.contains(&lower))
        .then_some(text)
    }

    /// A tone typed after a final only teencode uses (`nhuwgx` for nhữg) is moved before it,
    /// since the crate takes no tone after such a final.
    fn shaped_word_with_late_tone(&self, raw: &str) -> Option<String> {
        let keys: Vec<char> = raw.chars().collect();
        let (tone, rest) = keys.split_last()?;
        if !self
            .preeditor
            .input_method()
            .tone_keys()
            .contains(&tone.to_ascii_lowercase())
        {
            return None;
        }
        let lower: String = rest.iter().collect::<String>().to_lowercase();
        let last = ["nk", "g", "k", "h"]
            .iter()
            .find(|last| lower.ends_with(*last))?;
        let mut moved = rest.to_vec();
        moved.insert(rest.len() - last.len(), *tone);
        self.shaped_word_of(moved)
            .filter(|word| without_tones(word).1 == 1)
    }

    /// Whether swapping one onset, rime or final as teencode does gives a dictionary word.
    fn stands_for_word(&self, word: &str) -> bool {
        let (plain, _) = without_tones(word);
        let Some((onset, vowels, last)) = syllable_parts(&plain) else {
            return false;
        };
        let swaps = |table: &[(&str, &[&'static str])], part: &str| -> Vec<&'static str> {
            table
                .iter()
                .find(|(from, _)| *from == part)
                .map_or(Vec::new(), |(_, to)| to.to_vec())
        };
        let mut candidates = Vec::new();
        for o in swaps(ONSET_SWAPS, &onset) {
            candidates.push(format!("{o}{vowels}{last}"));
        }
        for v in swaps(RIME_SWAPS, &vowels) {
            candidates.push(format!("{onset}{v}{last}"));
        }
        for f in swaps(FINAL_SWAPS, &last) {
            candidates.push(format!("{onset}{vowels}{f}"));
        }
        let tone = without_tone_position(word)
            .chars()
            .last()
            .and_then(|c| c.to_digit(10));
        candidates
            .iter()
            .any(|candidate| self.is_dictionary_spelling(candidate, tone))
    }

    /// Whether `plain` with `tone` on one of its vowels is a dictionary word; trying each vowel
    /// matches whichever tone style the dictionary uses.
    fn is_dictionary_spelling(&self, plain: &str, tone: Option<u32>) -> bool {
        let Some(tone) = tone else {
            return self.dictionary.contains(plain);
        };
        let chars: Vec<char> = plain.chars().collect();
        (0..chars.len()).any(|at| {
            with_tone(chars[at], tone).is_some_and(|toned| {
                let mut word = chars.clone();
                word[at] = toned;
                self.dictionary.contains(&word.iter().collect::<String>())
            })
        })
    }
}
