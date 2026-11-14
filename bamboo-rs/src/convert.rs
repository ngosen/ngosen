// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

//! Converts Vietnamese text between charsets, changing letter case or dropping diacritics on the
//! way, for text typed in another charset long ago (#655).

use bamboo_core::{Charset, decode_charset, encode_charset};

use crate::text::without_diacritics;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LetterCase {
    Keep,
    Upper,
    Lower,
    Sentence,
    Title,
}

impl LetterCase {
    /// The values of the C interface, in this order.
    pub fn from_index(i: i32) -> Option<Self> {
        [
            Self::Keep,
            Self::Upper,
            Self::Lower,
            Self::Sentence,
            Self::Title,
        ]
        .get(usize::try_from(i).ok()?)
        .copied()
    }
}

/// Goes through Unicode: case and diacritics are only known there.
pub fn convert(
    text: &str,
    from: Charset,
    to: Charset,
    case: LetterCase,
    strip_diacritics: bool,
) -> String {
    let mut unicode = decode_charset(from, text).into_owned();
    if strip_diacritics {
        unicode = without_diacritics(&unicode);
    }
    let cased = match case {
        LetterCase::Keep => unicode,
        LetterCase::Upper => unicode.to_uppercase(),
        LetterCase::Lower => unicode.to_lowercase(),
        LetterCase::Sentence => capitalize(&unicode, |prev| matches!(prev, '.' | '!' | '?' | '\n')),
        LetterCase::Title => capitalize(&unicode, |_| true),
    };
    encode_charset(to, &cased).into_owned()
}

/// Lower case, except the first letter of the text and the first letter after a space that follows
/// a character `starts` accepts.
fn capitalize(text: &str, starts: impl Fn(char) -> bool) -> String {
    let mut out = String::with_capacity(text.len());
    let mut upper_next = true;
    let mut last_mark = '\n';
    for c in text.chars() {
        if c.is_alphabetic() {
            if upper_next {
                out.extend(c.to_uppercase());
            } else {
                out.extend(c.to_lowercase());
            }
            upper_next = false;
            last_mark = c;
            continue;
        }
        out.push(c);
        if c.is_whitespace() {
            upper_next |= starts(last_mark) || c == '\n';
        } else {
            last_mark = c;
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use bamboo_core::advanced::get_charset_names;

    fn unicode(text: &str, from: Charset) -> String {
        convert(text, from, Charset::Unicode, LetterCase::Keep, false)
    }

    #[test]
    fn legacy_text_becomes_unicode() {
        assert_eq!(unicode("TiÕng ViÖt", Charset::Tcvn3), "Tiếng Việt");
        assert_eq!(unicode("Tieáng Vieät", Charset::VniWindows), "Tiếng Việt");
        assert_eq!(unicode("Tie^'ng Vie^.t", Charset::Viqr), "Tiếng Việt");
        assert_eq!(unicode("\u{1b0}\u{1a1}ng", Charset::Unicode), "ương");
    }

    #[test]
    fn unicode_becomes_legacy_text() {
        let to = |cs| convert("Tiếng Việt", Charset::Unicode, cs, LetterCase::Keep, false);
        assert_eq!(to(Charset::Tcvn3), "TiÕng ViÖt");
        assert_eq!(to(Charset::VniWindows), "Tieáng Vieät");
    }

    #[test]
    fn lowercase_text_survives_every_round_trip() {
        // Capitals do not: TCVN3, VPS and VISCII share codes between cases, and the charsets that
        // write letters in ASCII decode ASCII text that looks like them.
        let text = "người việt nam yêu tiếng mẹ đẻ, ở đâu cũng nhớ quê hương";
        for name in get_charset_names() {
            let cs: Charset = name.parse().unwrap();
            let there = convert(text, Charset::Unicode, cs, LetterCase::Keep, false);
            assert_eq!(unicode(&there, cs), text, "{name}");
        }
    }

    #[test]
    fn upper_case_restores_tcvn3_capitals() {
        // TCVN3 drew toned capitals with a separate font, so the codes decode to lowercase.
        assert_eq!(unicode("Hµ Néi", Charset::Tcvn3), "Hà Nội");
        let upper = convert(
            "Hµ Néi",
            Charset::Tcvn3,
            Charset::Unicode,
            LetterCase::Upper,
            false,
        );
        assert_eq!(upper, "HÀ NỘI");
    }

    #[test]
    fn letter_cases() {
        let case = |text, case| convert(text, Charset::Unicode, Charset::Unicode, case, false);
        let text = "xin CHÀO. bạn khỏe không? tôi khỏe, 3.5 điểm\nđược";
        assert_eq!(
            case(text, LetterCase::Upper),
            "XIN CHÀO. BẠN KHỎE KHÔNG? TÔI KHỎE, 3.5 ĐIỂM\nĐƯỢC"
        );
        assert_eq!(
            case(text, LetterCase::Lower),
            "xin chào. bạn khỏe không? tôi khỏe, 3.5 điểm\nđược"
        );
        assert_eq!(
            case(text, LetterCase::Sentence),
            "Xin chào. Bạn khỏe không? Tôi khỏe, 3.5 điểm\nĐược"
        );
        assert_eq!(
            case("tiếng VIỆT (miền nam)", LetterCase::Title),
            "Tiếng Việt (Miền Nam)"
        );
    }

    #[test]
    fn diacritics_dropped_before_encoding() {
        let plain = convert(
            "TiÕng ViÖt",
            Charset::Tcvn3,
            Charset::Unicode,
            LetterCase::Keep,
            true,
        );
        assert_eq!(plain, "Tieng Viet");
    }
}
