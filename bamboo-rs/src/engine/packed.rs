// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

//! Word lists kept in one buffer. A `Box<str>` per word costs a pointer, a length and an
//! allocator chunk, several times the bytes of a short word.

/// Strings in the order they were given, stored back to back with the offset each one starts at.
#[derive(Debug, Default)]
pub struct Packed {
    text: Box<str>,
    starts: Box<[u32]>,
}

impl Packed {
    pub fn len(&self) -> usize {
        self.starts.len()
    }

    pub fn is_empty(&self) -> bool {
        self.starts.is_empty()
    }

    pub fn get(&self, i: usize) -> &str {
        let end = self
            .starts
            .get(i + 1)
            .map_or(self.text.len(), |&e| e as usize);
        &self.text[self.starts[i] as usize..end]
    }

    /// The first index where `less` stops holding, as `slice::partition_point` on a sorted list.
    pub fn partition_point(&self, mut less: impl FnMut(&str) -> bool) -> usize {
        let (mut lo, mut hi) = (0, self.len());
        while lo < hi {
            let mid = lo + (hi - lo) / 2;
            if less(self.get(mid)) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        lo
    }

    /// Whether a list sorted ascending holds `word`.
    pub fn contains(&self, word: &str) -> bool {
        let i = self.partition_point(|w| w < word);
        i < self.len() && self.get(i) == word
    }
}

impl<'a> FromIterator<&'a str> for Packed {
    fn from_iter<I: IntoIterator<Item = &'a str>>(strings: I) -> Self {
        let mut text = String::new();
        let mut starts = Vec::new();
        for s in strings {
            starts.push(u32::try_from(text.len()).expect("word list larger than 4 GiB"));
            text.push_str(s);
        }
        Self {
            text: text.into_boxed_str(),
            starts: starts.into_boxed_slice(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::super::{Dictionary, WordList};
    use crate::text::{grows_into, without_marks, without_tone_position, without_tones};
    use std::collections::HashSet;

    // The lists as they were stored before packing, to check the packed ones answer the same.
    struct OldWordList(Box<[Box<str>]>);

    impl OldWordList {
        fn new(words: &[String]) -> Self {
            let mut words: Vec<Box<str>> = words
                .iter()
                .map(|w| w.to_lowercase().into_boxed_str())
                .collect();
            words.sort();
            words.dedup();
            Self(words.into_boxed_slice())
        }

        fn starts_word(&self, keys: &str) -> bool {
            let i = self.0.partition_point(|w| &**w < keys);
            self.0.get(i).is_some_and(|w| w.starts_with(keys))
        }

        fn contains(&self, keys: &str) -> bool {
            self.0.binary_search_by(|w| (**w).cmp(keys)).is_ok()
        }
    }

    struct OldDictionary {
        words: HashSet<String>,
        prefixes: Box<[(Box<str>, Box<str>)]>,
    }

    impl OldDictionary {
        fn new(words: &[String]) -> Self {
            let mut prefixes: Vec<_> = words
                .iter()
                .map(|w| {
                    (
                        without_marks(&without_tones(w).0).into_boxed_str(),
                        w.to_lowercase().into_boxed_str(),
                    )
                })
                .collect();
            prefixes.sort();
            prefixes.dedup();
            Self {
                prefixes: prefixes.into_boxed_slice(),
                words: words.iter().map(|w| without_tone_position(w)).collect(),
            }
        }

        fn contains(&self, word: &str) -> bool {
            self.words.contains(&without_tone_position(word))
        }

        fn starts_word(&self, typed: &str) -> bool {
            let key = without_marks(&without_tones(typed).0);
            let first = self.prefixes.partition_point(|(w, _)| **w < *key);
            self.prefixes[first..]
                .iter()
                .take_while(|(w, _)| w.starts_with(&*key))
                .any(|(_, word)| grows_into(typed, word))
        }
    }

    fn lines(text: &str) -> Vec<String> {
        text.lines().map(str::to_string).collect()
    }

    /// Every prefix of every word, the word with a letter added, and a few strings no list holds.
    fn queries(words: &[String]) -> Vec<String> {
        let mut out: Vec<String> = ["", "a", "zzzz", "\u{0}", "ǿ", "đ", "Được", "ĐƯỢC", "xyzé"]
            .iter()
            .map(|s| s.to_string())
            .collect();
        for w in words {
            let chars: Vec<char> = w.chars().collect();
            for n in 1..=chars.len() {
                out.push(chars[..n].iter().collect());
            }
            out.push(format!("{w}z"));
            out.push(w.to_uppercase());
        }
        out
    }

    fn check_word_list(words: &[String]) {
        let (old, new): (OldWordList, WordList) =
            (OldWordList::new(words), words.iter().cloned().collect());
        assert_eq!(new.is_empty(), old.0.is_empty());
        for q in queries(words) {
            assert_eq!(
                new.starts_word(&q),
                old.starts_word(&q),
                "starts_word({q:?})"
            );
            assert_eq!(new.contains(&q), old.contains(&q), "contains({q:?})");
        }
    }

    fn check_dictionary(words: &[String]) {
        let (old, new): (OldDictionary, Dictionary) =
            (OldDictionary::new(words), words.iter().cloned().collect());
        for q in queries(words) {
            assert_eq!(
                new.starts_word(&q),
                old.starts_word(&q),
                "starts_word({q:?})"
            );
            assert_eq!(new.contains(&q), old.contains(&q), "contains({q:?})");
        }
    }

    #[test]
    fn packed_lists_answer_as_unpacked_ones() {
        let edge = lines(
            "\nabc\nABC\nabc\nab\nabcd\nzz\nđược\nĐược\nhoà\nhòa\nquý\nquí\nnaïve\n\u{1F600}",
        );
        check_word_list(&[]);
        check_word_list(&edge);
        check_word_list(&lines(include_str!("../../../data/english-common.txt")));
        check_dictionary(&[]);
        check_dictionary(&edge);
        check_dictionary(&lines(include_str!(
            "../../../data/dictionaries/vietnamese.cm.dict"
        )));
    }
}
