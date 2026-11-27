// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

//! The key strings one slip of the fingers away from the keys of a word, for Đoán dấu to ask
//! which words those would have typed.

use std::collections::BTreeMap;
use std::sync::Arc;

use bamboo_core::Engine;

use super::BambooEngine;

/// Each row of letter keys with how far it is shifted to the right of the top row, in key widths.
const ROWS: [(&str, f32); 3] = [("qwertyuiop", 0.0), ("asdfghjkl", 0.25), ("zxcvbnm", 0.75)];

/// The cost, in timsen's units, of a slip made easily: a neighbouring key, two keys in the wrong
/// order, a key hit twice or once for twice, an extra key beside the one meant.
const EASY: f64 = 1.0;
/// An extra key that is neither a repeat nor beside the key before or after it.
const STRAY: f64 = 2.0;

/// Every further key of a word is some ten more key strings to type through the engine, and no
/// syllable takes more keys than this, even with one too many.
const LONGEST: usize = 12;

fn place(key: u8) -> Option<(usize, f32)> {
    ROWS.iter().enumerate().find_map(|(row, (keys, shift))| {
        let i = keys.bytes().position(|k| k == key)?;
        Some((row, i as f32 + shift))
    })
}

fn near(a: u8, b: u8) -> bool {
    let (Some((row_a, x_a)), Some((row_b, x_b))) = (place(a), place(b)) else {
        return false;
    };
    a != b && row_a.abs_diff(row_b) <= 1 && (x_a - x_b).abs() <= 1.0
}

/// The key strings that `keys`, lowercase letters, are one slip away from, each with the cost of
/// the cheapest slip that leads there. A key hit for one far from it and a key left out are not
/// tried, unless the key left out is one of a doubled pair: either takes a string for every
/// letter at every place.
pub(crate) fn one_slip_away(keys: &str) -> Vec<(String, f64)> {
    let typed = keys.as_bytes();
    if typed.len() > LONGEST || !typed.iter().all(u8::is_ascii_lowercase) {
        return Vec::new();
    }
    let mut out: BTreeMap<Vec<u8>, f64> = BTreeMap::new();
    let mut add = |keys: Vec<u8>, cost: f64| {
        let least = out.entry(keys).or_insert(cost);
        *least = least.min(cost);
    };
    for i in 0..typed.len() {
        let (before, after) = typed.split_at(i);
        let (here, rest) = (after[0], &after[1..]);
        let (prev, next) = (before.last().copied(), rest.first().copied());
        add([before, &[here], after].concat(), EASY);
        // After a or o, the letter again and w each add a mark, and one is typed for the other.
        let marks = |key: u8| {
            prev.is_some_and(|p| {
                b"ao".contains(&p) && [here, key].contains(&p) && [here, key].contains(&b'w')
            })
        };
        for key in (b'a'..=b'z').filter(|&k| k != here && (near(k, here) || marks(k))) {
            add([before, &[key], rest].concat(), EASY);
        }
        let slipped = |k: Option<u8>| k.is_some_and(|k| k == here || near(k, here));
        let extra = if slipped(prev) || slipped(next) {
            EASY
        } else {
            STRAY
        };
        add([before, rest].concat(), extra);
        if let Some(next) = next.filter(|&n| n != here) {
            add([before, &[next, here], &rest[1..]].concat(), EASY);
        }
    }
    out.remove(typed);
    (out.into_iter())
        .filter_map(|(keys, cost)| Some((String::from_utf8(keys).ok()?, cost)))
        .collect()
}

impl BambooEngine {
    /// An engine that types as this one does, with no macros and no Đoán dấu: it shows only what
    /// the keys themselves type.
    fn typing_alone(&self) -> BambooEngine {
        let method = self.preeditor.input_method();
        let dictionary = Arc::clone(&self.dictionary);
        let mut alone = BambooEngine::new(method.clone(), dictionary, Arc::default());
        alone.preeditor = Engine::with_config(method.clone(), self.preeditor.config());
        alone.english_words = Arc::clone(&self.english_words);
        alone.keep_guard = Arc::clone(&self.keep_guard);
        alone.auto_non_vn_restore = self.auto_non_vn_restore;
        alone.dd_free_style = self.dd_free_style;
        alone.spell_check_with_dicts = self.spell_check_with_dicts;
        alone.teencode = self.teencode;
        alone
    }

    /// The word in the field once `keys` and a space are typed, lowercase, or None when they
    /// leave anything but one word there.
    fn word_typed(&mut self, keys: &str) -> Option<String> {
        self.reset();
        for key in keys.chars().chain([' ']) {
            self.process_key_event(key as u32, 0);
        }
        if self.commit_text.is_empty() {
            self.commit_preedit();
        }
        let text = std::mem::take(&mut self.commit_text);
        let word = text.trim_end();
        (!word.is_empty() && word.chars().all(char::is_alphabetic)).then(|| word.to_lowercase())
    }

    /// The words that keys one slip away from `keys` would have typed, each with the cost of the
    /// cheapest slip that types it.
    pub(crate) fn words_near(&self, keys: &str) -> Vec<(String, f64)> {
        let slips = one_slip_away(&keys.to_ascii_lowercase());
        if slips.is_empty() {
            return Vec::new();
        }
        let mut alone = self.typing_alone();
        let mut words: BTreeMap<String, f64> = BTreeMap::new();
        for (keys, cost) in slips {
            if let Some(word) = alone.word_typed(&keys) {
                let least = words.entry(word).or_insert(cost);
                *least = least.min(cost);
            }
        }
        words.into_iter().collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn cost(typed: &str, meant: &str) -> Option<f64> {
        (one_slip_away(typed).into_iter()).find_map(|(keys, cost)| (keys == meant).then_some(cost))
    }

    #[test]
    fn slips_made_easily_cost_one() {
        // A neighbouring key, on the row above the one meant, on its row and on the row below.
        assert_eq!(cost("jguoiwf", "nguoiwf"), Some(EASY));
        assert_eq!(cost("mguoiwf", "nguoiwf"), Some(EASY));
        assert_eq!(cost("ngjoiwf", "nguoiwf"), Some(EASY));
        // Two keys in the wrong order.
        assert_eq!(cost("nguoifw", "nguoiwf"), Some(EASY));
        // A key hit twice, and one of a doubled pair left out.
        assert_eq!(cost("nguoiwff", "nguoiwf"), Some(EASY));
        assert_eq!(cost("nguoif", "nguooif"), Some(EASY));
        // An extra key beside the one before it.
        assert_eq!(cost("nguopiwf", "nguoiwf"), Some(EASY));
        // oo for ow.
        assert_eq!(cost("nguooif", "nguowif"), Some(EASY));
        assert_eq!(cost("nguowif", "nguooif"), Some(EASY));
    }

    #[test]
    fn a_stray_extra_key_costs_more() {
        assert_eq!(cost("nguwowpif", "nguwowif"), Some(STRAY));
    }

    #[test]
    fn a_far_key_and_a_key_left_out_are_not_tried() {
        assert_eq!(cost("pguoiwf", "nguoiwf"), None);
        assert_eq!(cost("nguiwf", "nguoiwf"), None);
    }

    #[test]
    fn only_lowercase_letters_of_a_syllable_are_searched() {
        assert!(one_slip_away("nguoi72").is_empty());
        assert!(one_slip_away("Nguoiwf").is_empty());
        assert!(one_slip_away("internationalization").is_empty());
        assert!(!one_slip_away("nghieengs").is_empty());
    }

    #[test]
    fn the_keys_typed_are_not_among_them() {
        assert_eq!(cost("aab", "aab"), None);
        assert!(one_slip_away("").is_empty());
    }
}
