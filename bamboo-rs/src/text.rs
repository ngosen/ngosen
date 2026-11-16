// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

//! Character classes the per-key logic needs; `bamboo-core` keeps its own copies private.

const VOWELS: &str = "aàáảãạăằắẳẵặâầấẩẫậeèéẻẽẹêềếểễệiìíỉĩịoòóỏõọôồốổỗộơờớởỡợuùúủũụưừứửữựyỳýỷỹỵ";

const PUNCTUATION_MARKS: &[char] = &[
    ',', ';', ':', '.', '"', '\'', '!', '?', ' ', '<', '>', '=', '+', '-', '*', '/', '\\', '_',
    '~', '`', '@', '#', '$', '%', '^', '&', '(', ')', '{', '}', '[', ']', '|',
];

const MARKED_LETTERS: &str = "âăêôơưđ";

// A vowel typed before its mark key, such as the a of ă or â, must still match the marked letter.
const UNMARKED_ROW: [usize; 12] = [0, 0, 0, 3, 3, 5, 6, 6, 6, 9, 9, 11];

pub fn is_word_break_symbol(c: char) -> bool {
    PUNCTUATION_MARKS.contains(&c) || c.is_ascii_digit()
}

fn lower(c: char) -> char {
    c.to_lowercase().next().unwrap_or(c)
}

fn vowel_position(c: char) -> Option<usize> {
    VOWELS.chars().position(|v| v == c)
}

/// A letter carrying a tone or a mark, which only Vietnamese text has.
fn is_vietnamese_rune(lower_c: char) -> bool {
    if vowel_position(lower_c).is_some_and(|pos| pos % 6 != 0) {
        return true;
    }
    MARKED_LETTERS.contains(lower_c)
}

pub fn has_any_vietnamese_rune(word: &str) -> bool {
    word.chars().any(|c| is_vietnamese_rune(lower(c)))
}

pub fn has_any_vietnamese_vowel(word: &str) -> bool {
    word.chars().any(|c| vowel_position(lower(c)).is_some())
}

pub fn vowel_count(word: &str) -> usize {
    word.chars()
        .filter(|&c| vowel_position(lower(c)).is_some())
        .count()
}

/// The word in lower case without its tones, and its tones as digits in order.
fn split_tones(word: &str) -> (String, String) {
    let mut letters = String::with_capacity(word.len());
    let mut tones = String::new();
    for c in word.chars() {
        let lower_c = lower(c);
        match vowel_position(lower_c) {
            Some(pos) if pos % 6 != 0 => {
                letters.extend(VOWELS.chars().nth(pos - pos % 6));
                tones.extend(char::from_digit((pos % 6) as u32, 10));
            }
            _ => letters.push(lower_c),
        }
    }
    (letters, tones)
}

/// The word in lower case without its tones, and how many tones it had.
pub fn without_tones(word: &str) -> (String, usize) {
    let (letters, tones) = split_tones(word);
    (letters, tones.len())
}

/// The word with its tones moved to the end, so `hoà` and `hòa` compare equal whichever tone
/// style typed them.
pub fn without_tone_position(word: &str) -> String {
    let (letters, tones) = split_tones(word);
    letters + &tones
}

/// The word in lower case without the marks of ă, â, ê, ô, ơ, ư and đ, keeping its tones.
pub fn without_marks(word: &str) -> String {
    word.chars()
        .map(|c| match lower(c) {
            'đ' => 'd',
            c => vowel_position(c)
                .and_then(|pos| VOWELS.chars().nth(UNMARKED_ROW[pos / 6] * 6 + pos % 6))
                .unwrap_or(c),
        })
        .collect()
}

/// The text without tones, marks and combining diacritics, keeping case: `Đà Nẵng` becomes `Da Nang`.
pub fn without_diacritics(text: &str) -> String {
    text.chars()
        .filter(|c| !('\u{300}'..='\u{36f}').contains(c))
        .map(|c| {
            let plain = match lower(c) {
                'đ' => 'd',
                l => vowel_position(l)
                    .and_then(|pos| VOWELS.chars().nth(UNMARKED_ROW[pos / 6] * 6))
                    .unwrap_or(c),
            };
            if plain != c && c.is_uppercase() {
                plain.to_ascii_uppercase()
            } else {
                plain
            }
        })
        .collect()
}

fn tones(word: &str) -> impl Iterator<Item = usize> + '_ {
    word.chars()
        .filter_map(|c| vowel_position(lower(c)))
        .map(|pos| pos % 6)
        .filter(|&t| t != 0)
}

/// Whether more keys can turn `typed` into the start of `word`: each letter is the same or lacks a
/// mark the word's letter has, and any tone typed matches the word's tone even if more letters move
/// it (`khủy` to khuỷu), while `cró` never grows into crông.
pub fn grows_into(typed: &str, word: &str) -> bool {
    let covers = |t: char, w: char| {
        if t == w {
            return true;
        }
        if t == 'd' {
            return w == 'đ';
        }
        let (Some(t), Some(w)) = (vowel_position(t), vowel_position(w)) else {
            return false;
        };
        let (t_row, w_row) = (t / 6, w / 6);
        t_row == w_row || t_row == UNMARKED_ROW[w_row]
    };
    (tones(typed).next().is_none() || tones(typed).eq(tones(word)))
        && typed.chars().count() <= word.chars().count()
        && typed
            .chars()
            .map(lower)
            .zip(word.chars().map(lower))
            .all(|(t, w)| covers(t, w))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn vietnamese_runes() {
        assert!(has_any_vietnamese_rune("tôi"));
        assert!(has_any_vietnamese_rune("Đi"));
        assert!(has_any_vietnamese_rune("ẤY"));
        assert!(!has_any_vietnamese_rune("toi"));
        assert!(!has_any_vietnamese_rune(""));
    }

    #[test]
    fn vietnamese_vowels() {
        assert!(has_any_vietnamese_vowel("bờ"));
        assert!(has_any_vietnamese_vowel("Y"));
        assert!(!has_any_vietnamese_vowel("đđ"));
    }

    #[test]
    fn tones_removed() {
        assert_eq!(without_tones("Được"), ("đươc".to_string(), 1));
        assert_eq!(without_tones("hoà"), ("hoa".to_string(), 1));
        assert_eq!(without_tones("đuo3"), ("đuo3".to_string(), 0));
        assert_eq!(vowel_count("đươc"), 2);
    }

    #[test]
    fn diacritics_removed() {
        assert_eq!(
            without_diacritics("Tiếng Việt, ĐÀ NẴNG"),
            "Tieng Viet, DA NANG"
        );
        assert_eq!(without_diacritics("Ư ơ ỹ"), "U o y");
        // Unicode tổ hợp: a letter followed by combining marks.
        assert_eq!(without_diacritics("Vie\u{302}\u{323}t"), "Viet");
        assert_eq!(without_diacritics("naïve 2€"), "naïve 2€");
    }

    #[test]
    fn word_breaks() {
        for c in [' ', ',', '1', '[', '|'] {
            assert!(is_word_break_symbol(c), "{c}");
        }
        for c in ['a', 'đ', '€', '\n'] {
            assert!(!is_word_break_symbol(c), "{c}");
        }
    }
}
