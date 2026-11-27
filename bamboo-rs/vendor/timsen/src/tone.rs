//! Tone mark placement and capitalization of restored words.

/// Where the tone mark goes in `oa`, `oe` and `uy` at the end of a syllable.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ToneStyle {
    /// `hoà`, `khoẻ`, `thuỷ`: the table's own spelling.
    New,
    /// `hòa`, `khỏe`, `thủy`.
    Old,
}

// Five tones in the same order for each vowel: grave, acute, hook, tilde, dot.
const A: [char; 5] = ['à', 'á', 'ả', 'ã', 'ạ'];
const E: [char; 5] = ['è', 'é', 'ẻ', 'ẽ', 'ẹ'];
const Y: [char; 5] = ['ỳ', 'ý', 'ỷ', 'ỹ', 'ỵ'];
const O: [char; 5] = ['ò', 'ó', 'ỏ', 'õ', 'ọ'];
const U: [char; 5] = ['ù', 'ú', 'ủ', 'ũ', 'ụ'];

/// Spells a lowercase word from the table in the given style.
pub(crate) fn with_style(word: &str, style: ToneStyle) -> String {
    if style == ToneStyle::New {
        return word.to_string();
    }
    let mut chars: Vec<char> = word.chars().collect();
    let n = chars.len();
    if n < 2 {
        return word.to_string();
    }
    let (first, last) = (chars[n - 2], chars[n - 1]);
    let moved = match first {
        'o' => A
            .iter()
            .chain(&E)
            .position(|&c| c == last)
            .map(|t| (O[t % 5], if t < 5 { 'a' } else { 'e' })),
        // In "quý" the u belongs to the consonant, so the mark stays on y.
        'u' if n < 3 || chars[n - 3] != 'q' => {
            Y.iter().position(|&c| c == last).map(|t| (U[t], 'y'))
        }
        _ => None,
    };
    if let Some((marked, plain)) = moved {
        chars[n - 2] = marked;
        chars[n - 1] = plain;
    }
    chars.into_iter().collect()
}

/// Each base letter followed by its marked forms: hat or breve or horn, then the five tones.
const MARKED: [&str; 7] = [
    "aàáảãạăằắẳẵặâầấẩẫậ",
    "eèéẻẽẹêềếểễệ",
    "iìíỉĩị",
    "oòóỏõọôồốổỗộơờớởỡợ",
    "uùúủũụưừứửữự",
    "yỳýỷỹỵ",
    "dđ",
];

/// The lowercase letters of `word` without any marks: `Hoà` → `hoa`, `đường` → `duong`.
pub(crate) fn strip(word: &str) -> String {
    word.chars()
        .flat_map(char::to_lowercase)
        .map(|c| {
            MARKED
                .iter()
                .find(|group| group.contains(c))
                .and_then(|group| group.chars().next())
                .unwrap_or(c)
        })
        .collect()
}

/// A lowercase word in the table's own tone placement: `hòa` → `hoà`. The inverse of
/// [`with_style`] with [`ToneStyle::Old`].
pub(crate) fn to_new(word: &str) -> String {
    let mut chars: Vec<char> = word.chars().collect();
    let n = chars.len();
    if n < 2 {
        return word.to_string();
    }
    let (first, last) = (chars[n - 2], chars[n - 1]);
    let moved = match last {
        'a' | 'e' => O.iter().position(|&c| c == first).map(|t| {
            let tones = if last == 'a' { &A } else { &E };
            ('o', tones[t])
        }),
        'y' if n < 3 || chars[n - 3] != 'q' => {
            U.iter().position(|&c| c == first).map(|t| ('u', Y[t]))
        }
        _ => None,
    };
    if let Some((plain, marked)) = moved {
        chars[n - 2] = plain;
        chars[n - 1] = marked;
    }
    chars.into_iter().collect()
}

/// Gives `restored` the capitalization of `typed`, letter by letter.
pub(crate) fn apply_case(typed: &str, restored: &str) -> String {
    let mut out = String::with_capacity(restored.len());
    let mut upper = typed.chars().map(char::is_uppercase);
    for c in restored.chars() {
        if upper.next().unwrap_or(false) {
            out.extend(c.to_uppercase());
        } else {
            out.push(c);
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn old_style_moves_the_mark_to_the_first_vowel() {
        for (new, old) in [
            ("hoà", "hòa"),
            ("khoẻ", "khỏe"),
            ("thuỷ", "thủy"),
            ("hoạ", "họa"),
            ("xoè", "xòe"),
        ] {
            assert_eq!(with_style(new, ToneStyle::Old), old);
        }
    }

    #[test]
    fn old_style_leaves_other_words_alone() {
        for word in ["quý", "quả", "hoàng", "tuyết", "khuya", "hoa", "y", "gõ"] {
            assert_eq!(with_style(word, ToneStyle::Old), word);
        }
    }

    #[test]
    fn old_style_reads_back_as_new() {
        for word in [
            "hoà", "khoẻ", "thuỷ", "hoạ", "xoè", "quý", "quả", "hoàng", "khuya", "hoa", "y", "gõ",
        ] {
            assert_eq!(to_new(&with_style(word, ToneStyle::Old)), word);
        }
    }

    #[test]
    fn strip_removes_every_mark() {
        assert_eq!(strip("Đường"), "duong");
        assert_eq!(strip("HOÀ"), "hoa");
        assert_eq!(strip("nghiêng"), "nghieng");
        assert_eq!(strip("test"), "test");
    }

    #[test]
    fn case_follows_the_typed_letters() {
        assert_eq!(apply_case("Toi", "tôi"), "Tôi");
        assert_eq!(apply_case("HA", "hà"), "HÀ");
        assert_eq!(apply_case("DUONG", "đường"), "ĐƯỜNG");
        assert_eq!(apply_case("go", "gõ"), "gõ");
    }
}
