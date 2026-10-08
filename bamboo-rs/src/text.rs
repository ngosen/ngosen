// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

//! Character classes the per-key logic needs. `bamboo-core` keeps its own copies private, so
//! these follow the Go core's `utils.go`.

const VOWELS: &str = "aàáảãạăằắẳẵặâầấẩẫậeèéẻẽẹêềếểễệiìíỉĩịoòóỏõọôồốổỗộơờớởỡợuùúủũụưừứửữựyỳýỷỹỵ";

const PUNCTUATION_MARKS: &[char] = &[
    ',', ';', ':', '.', '"', '\'', '!', '?', ' ', '<', '>', '=', '+', '-', '*', '/', '\\', '_',
    '~', '`', '@', '#', '$', '%', '^', '&', '(', ')', '{', '}', '[', ']', '|',
];

const MARKED_LETTERS: &str = "âăêôơưđ";

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
    fn word_breaks() {
        for c in [' ', ',', '1', '[', '|'] {
            assert!(is_word_break_symbol(c), "{c}");
        }
        for c in ['a', 'đ', '€', '\n'] {
            assert!(!is_word_break_symbol(c), "{c}");
        }
    }
}
