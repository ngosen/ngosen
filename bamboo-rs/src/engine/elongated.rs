// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

//! A word stretched by holding its last letter (`afaaa` for àààà, `ooiiii` for ôiiii).

use super::teencode::spends_every_key;
use super::*;

impl BambooEngine {
    /// The dictionary word the keys start with, followed by one copy of its last letter, marks
    /// included, for each key after it that types that letter again.
    pub(super) fn elongated_word(&self) -> Option<String> {
        let raw = self.raw_text();
        if !raw.chars().all(char::is_alphabetic)
            || self.get_processed_string(OutputOptions::RAW) != raw
        {
            return None;
        }
        let keys: Vec<char> = raw.chars().collect();
        let held = keys.last()?.to_ascii_lowercase();
        let run = keys
            .iter()
            .rev()
            .take_while(|c| c.to_ascii_lowercase() == held)
            .count();
        // `thereee` stretches there, not thẻ.
        let lowered = |keys: &[char]| keys.iter().collect::<String>().to_lowercase();
        if (keys.len() - run + 1..=keys.len())
            .any(|end| self.english_words.contains(&lowered(&keys[..end])))
        {
            return None;
        }
        // The shortest word wins: `ooiiii` is ôi held, as ô does not end in i.
        (keys.len().saturating_sub(run).max(1)..keys.len() - 1).find_map(|n| {
            let (word_keys, copies) = keys.split_at(n);
            let lower_keys = lowered(word_keys);
            let typed = self.typed_alone(word_keys.to_vec());
            let word = typed.get_processed_str(OutputOptions::NONE);
            let lower = word.to_lowercase();
            let last = word.chars().last()?;
            let ends_held =
                without_diacritics(&last.to_lowercase().to_string()) == held.to_string();
            (typed.get_processed_str(OutputOptions::FULL_TEXT) == word
                && ends_held
                && spends_every_key(&lower_keys, &lower)
                && self.dictionary.contains(&lower))
            .then(|| word + &repeated(last, copies))
        })
    }
}

/// `last` once per key in `copies`, in the case of that key.
fn repeated(last: char, copies: &[char]) -> String {
    copies
        .iter()
        .flat_map(|c| {
            if c.is_uppercase() {
                last.to_uppercase().collect::<Vec<_>>()
            } else {
                last.to_lowercase().collect()
            }
        })
        .collect()
}
