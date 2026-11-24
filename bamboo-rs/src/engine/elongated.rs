// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

//! A word stretched by holding its last letter (`afaaa` for àaaa, `ooiiii` for ôiiii).

use super::teencode::spends_every_key;
use super::*;

impl BambooEngine {
    /// The dictionary word the keys start with, followed by the keys after it that type its last
    /// letter again, unmarked so a long run stays easy to read. A tone key typed after them
    /// (`aaaf`) marks the word as well.
    pub(super) fn elongated_word(&self) -> Option<String> {
        let raw = self.raw_text();
        if !raw.chars().all(char::is_alphabetic)
            || self.get_processed_string(OutputOptions::RAW) != raw
        {
            return None;
        }
        let keys: Vec<char> = raw.chars().collect();
        self.held_word(&keys, None).or_else(|| {
            let (tone, body) = keys.split_last()?;
            let tone_keys = self.preeditor.input_method().tone_keys();
            // In capitals the last letter more likely ends an acronym (`AAAS`) than adds a tone.
            (!is_all_caps(keys.iter().copied())
                && tone_keys.contains(&tone.to_ascii_lowercase())
                && body.last().is_some_and(|c| !c.eq_ignore_ascii_case(tone)))
            .then(|| self.held_word(body, Some(*tone)))
            .flatten()
        })
    }

    fn held_word(&self, keys: &[char], tone: Option<char>) -> Option<String> {
        let held = keys.last()?.to_ascii_lowercase();
        let run = keys
            .iter()
            .rev()
            .take_while(|c| c.to_ascii_lowercase() == held)
            .count();
        // `thereee` stretches there, not thẻ, and `treeees` stretches trees.
        let lowered = |keys: &[char]| keys.iter().collect::<String>().to_lowercase();
        if (keys.len() - run + 1..=keys.len()).any(|end| {
            let mut english = keys[..end].to_vec();
            english.extend(tone);
            self.english_words.contains(&lowered(&english))
        }) {
            return None;
        }
        // The shortest word wins: `ooiiii` is ôi held, as ô does not end in i.
        (keys.len().saturating_sub(run).max(1)..keys.len() - 1).find_map(|n| {
            let (word_keys, copies) = keys.split_at(n);
            let mut word_keys = word_keys.to_vec();
            word_keys.extend(tone);
            let lower_keys = lowered(&word_keys);
            let typed = self.typed_alone(word_keys);
            let word = typed.get_processed_str(OutputOptions::NONE);
            let lower = word.to_lowercase();
            let last = word.chars().last()?;
            let ends_held =
                without_diacritics(&last.to_lowercase().to_string()) == held.to_string();
            (typed.get_processed_str(OutputOptions::FULL_TEXT) == word
                && ends_held
                && spends_every_key(&lower_keys, &lower)
                && self.dictionary.contains(&lower))
            .then(|| word + &copies.iter().collect::<String>())
        })
    }
}
