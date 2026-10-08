// SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

use super::*;
use bamboo_core::BracketMode;

pub fn new_test_engine(dict: &[&str], spell_check_with_dicts: bool) -> BambooEngine {
    let dictionary = dict.iter().map(|w| w.to_string()).collect();
    let mut e = BambooEngine::new(
        input_method_by_name("Telex"),
        Arc::new(dictionary),
        Arc::default(),
    );
    e.spell_check_with_dicts = spell_check_with_dicts;
    e
}

pub fn type_keys(e: &mut BambooEngine, keys: &str) {
    for c in keys.chars() {
        e.process_key_event(c as u32, 0);
    }
}

pub fn macro_table(pairs: &[(&str, &str)]) -> Arc<MacroTable> {
    let table = pairs
        .iter()
        .map(|&(k, v)| (k.to_string(), v.to_string()))
        .collect();
    Arc::new(MacroTable::new(table))
}

const SHIFT_MASK: u32 = 1 << 0;

/// Checks `s` against a shape such as "dd:dd", where `d` stands for a digit.
fn has_shape(s: &str, shape: &str) -> bool {
    s.chars().count() == shape.chars().count()
        && s.chars()
            .zip(shape.chars())
            .all(|(c, p)| if p == 'd' { c.is_ascii_digit() } else { c == p })
}

#[test]
fn test_determine_macro_case() {
    let cases = [
        ("abc", MacroCase::AllSmall),
        ("ABC", MacroCase::AllCapital),
        ("aBc", MacroCase::NoChange),
        ("123", MacroCase::NoChange),
        ("", MacroCase::NoChange),
        ("áộ", MacroCase::AllSmall),
        ("ÁỘ", MacroCase::AllCapital),
    ];
    for (input, want) in cases {
        assert_eq!(
            determine_macro_case(input),
            want,
            "determine_macro_case({input:?})"
        );
    }
}

#[test]
fn test_get_last_rune() {
    assert_eq!(get_last_rune(""), '\0');
    assert_eq!(get_last_rune("abc"), 'c');
    assert_eq!(get_last_rune("tắk"), 'k');
}

#[test]
fn test_in_key_list() {
    let list = ['a', 'w', 's'];
    assert!(in_key_list(&list, 'a'));
    assert!(!in_key_list(&list, 'z'));
    assert!(!in_key_list(&[], 'a'));
}

#[test]
fn test_format_time() {
    assert_eq!(format_time(""), "");
    assert_eq!(format_time("no placeholders"), "no placeholders");
    assert!(has_shape(&format_time("%H:%M"), "dd:dd"));
    assert!(has_shape(&format_time("%d/%m/%Y"), "dd/dd/dddd"));
    let fallback = format_time("%Q");
    assert!(!fallback.is_empty() && fallback != "%Q", "got {fallback:?}");
}

#[test]
fn test_expand_macro() {
    let mut e = new_test_engine(&[], false);
    e.time_format.clear();
    e.date_format.clear();
    assert_eq!(e.expand_macro("VN", "Việt Nam"), "Việt Nam");
    e.auto_capitalize_macro = true;
    assert_eq!(e.expand_macro("vn", "Việt Nam"), "việt nam");
    assert_eq!(e.expand_macro("VN", "việt nam"), "VIỆT NAM");
    assert_eq!(e.expand_macro("Vn", "Việt Nam"), "Việt Nam");
    e.auto_capitalize_macro = false;
    e.time_format = "%H:%M".to_string();
    let now = e.expand_macro("now", "giờ $TIME");
    assert!(now.chars().any(|c| c.is_ascii_digit()), "got {now:?}");
    e.time_format.clear();
    assert_eq!(e.expand_macro("now", "giờ $TIME"), "giờ $TIME");
    e.date_format = "%d/%m/%Y".to_string();
    assert!(e.expand_macro("now", "$DATE").contains('/'));
}

#[test]
fn test_update_preedit_clears_both() {
    let mut e = new_test_engine(&[], false);
    e.output_charset.clear();
    e.preedit_text = "x".to_string();
    e.commit_text = "y".to_string();
    e.update_preedit("");
    assert_eq!(e.preedit_text, "");
    assert_eq!(e.commit_text, "");
}

#[test]
fn test_encode_charsets() {
    // "VNI" and "TCVN3" are not charset names, so the text passes through.
    for cs in ["Unicode", "VNI", "TCVN3"] {
        assert_eq!(encode(cs, "tôi"), "tôi", "encode({cs})");
    }
}

#[test]
fn test_preedit_telex_composition() {
    for (keys, want) in [("aw", "ă"), ("dd", "đ"), ("tooi", "tôi"), ("chaof", "chào")] {
        let mut e = new_test_engine(&[], false);
        type_keys(&mut e, keys);
        assert_eq!(e.preedit_text, want, "type {keys}");
    }
}

#[test]
fn test_word_break_commit() {
    let mut e = new_test_engine(&[], false);
    type_keys(&mut e, "chao");
    assert!(e.process_key_event(KEY_SPACE, 0));
    assert_eq!(e.commit_text, "chao ");
    assert_eq!(e.preedit_text, "");
}

#[test]
fn test_auto_restore_dict_mode() {
    let mut e = new_test_engine(&["chào"], true);
    type_keys(&mut e, "chaof");
    assert_eq!(e.preedit_text, "chào");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.commit_text, "chào ", "dict hit");

    let mut e = new_test_engine(&["chào"], true);
    type_keys(&mut e, "khoawjm");
    assert_eq!(e.preedit_text, "khoặm");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.commit_text, "khoawjm ", "dict miss restores the keys");
}

#[test]
fn test_auto_restore_rules_mode() {
    let mut e = new_test_engine(&[], false);
    type_keys(&mut e, "tak");
    assert!(e.must_fallback_to_english(), "tak breaks the rules");
    let mut e = new_test_engine(&[], false);
    type_keys(&mut e, "tooi");
    assert!(!e.must_fallback_to_english(), "tooi");
    let mut e = new_test_engine(&[], false);
    type_keys(&mut e, "boawjm");
    assert_eq!(e.preedit_text, "boặm");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.commit_text, "boặm ");
}

#[test]
fn test_dd_free_style() {
    let mut e = new_test_engine(&[], true);
    type_keys(&mut e, "dd");
    assert!(!e.must_fallback_to_english(), "dd free style on");
    e.dd_free_style = false;
    assert!(e.must_fallback_to_english(), "dd free style off");
}

#[test]
fn test_auto_non_vn_restore_disabled() {
    let mut e = new_test_engine(&[], true);
    e.auto_non_vn_restore = false;
    type_keys(&mut e, "boawjm");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.commit_text, "boặm ");
}

#[test]
fn test_to_upper() {
    let mut e = new_test_engine(&[], false);
    assert_eq!(e.to_upper('b'), 'b');
    assert_eq!(e.to_upper('['), '[', "bracket mode off");
    let config = Config {
        bracket_mode: BracketMode::Everywhere,
        ..e.preeditor.config()
    };
    e.preeditor.set_config(config);
    for (input, want) in [('[', '{'), (']', '}'), ('{', '['), ('}', ']')] {
        assert_eq!(e.to_upper(input), want, "to_upper({input})");
    }
}

#[test]
fn test_shift_space_restore_key_strokes() {
    let mut e = new_test_engine(&[], false);
    type_keys(&mut e, "aw");
    assert_eq!(e.preedit_text, "ă");
    e.should_restore_key_strokes = true;
    e.process_key_event(KEY_SPACE, 0);
    assert!(!e.should_restore_key_strokes);
    assert_eq!(e.preedit_text, "aw");
}

#[test]
fn test_backspace() {
    let mut e = new_test_engine(&[], false);
    type_keys(&mut e, "aw");
    assert!(e.process_key_event(KEY_BACKSPACE, 0));
    assert_eq!((e.commit_text.as_str(), e.preedit_text.as_str()), ("", ""));

    let mut e = new_test_engine(&[], false);
    type_keys(&mut e, "chao");
    e.process_key_event(KEY_BACKSPACE, 0);
    assert_eq!(e.preedit_text, "cha");

    let mut e = new_test_engine(&[], false);
    assert!(!e.process_key_event(KEY_BACKSPACE, 0), "empty engine");
}

#[test]
fn test_tab_macro_expansion() {
    let mut e = new_test_engine(&[], false);
    e.macro_enabled = true;
    e.macro_table = macro_table(&[("vn", "Việt Nam")]);
    type_keys(&mut e, "vn");
    assert!(e.process_key_event(KEY_TAB, 0));
    assert_eq!(e.commit_text, "Việt Nam");

    let mut e = new_test_engine(&[], false);
    e.macro_enabled = true;
    e.macro_table = macro_table(&[("vn", "Việt Nam")]);
    type_keys(&mut e, "xx");
    assert!(!e.process_key_event(KEY_TAB, 0), "no macro match");
    assert_eq!(e.commit_text, "xx");
}

#[test]
fn backspace_deletes_digit_in_macro_mode() {
    let mut e = new_test_engine(&[], false);
    e.macro_enabled = true;
    type_keys(&mut e, "xij1");
    assert_eq!(e.preedit_text, "xị1");
    e.process_key_event(KEY_BACKSPACE, 0);
    assert_eq!(e.preedit_text, "xị");
    type_keys(&mut e, "1n");
    assert_eq!(e.preedit_text, "xị1n");
}

#[test]
fn tab_expands_digit_macro() {
    // Digits end the crate's syllable, so the macro key spans committed text.
    let mut e = new_test_engine(&[], false);
    e.macro_enabled = true;
    e.macro_table = macro_table(&[("123", "Mixed Case")]);
    type_keys(&mut e, "123");
    assert!(e.process_key_event(KEY_TAB, 0));
    assert_eq!(e.commit_text, "Mixed Case");
}

#[test]
fn test_tab_time_macro() {
    let mut e = new_test_engine(&[], false);
    e.macro_enabled = true;
    // Macro keys match the composed text: "now" composes to "nơ".
    e.macro_table = macro_table(&[("nơ", "$TIME giờ")]);
    e.time_format = "%H:%M".to_string();
    type_keys(&mut e, "now");
    e.process_key_event(KEY_TAB, 0);
    let time = e.commit_text.strip_suffix(" giờ").unwrap_or_default();
    assert!(has_shape(time, "dd:dd"), "got {:?}", e.commit_text);
}

#[test]
fn test_can_process_key() {
    let e = new_test_engine(&[], false);
    for kv in [KEY_SPACE, KEY_BACKSPACE, ',' as u32, 'a' as u32, 'w' as u32] {
        assert!(e.can_process_key(kv), "can_process_key({kv})");
    }
    // Not a letter, not punctuation, not a Vietnamese letter.
    assert!(!e.can_process_key('€' as u32));
}

#[test]
fn test_is_valid_state() {
    assert!(BambooEngine::is_valid_state(0));
    assert!(BambooEngine::is_valid_state(SHIFT_MASK), "shift is allowed");
    for mask in [
        CONTROL_MASK,
        MOD1_MASK,
        IGNORED_MASK,
        SUPER_MASK,
        HYPER_MASK,
        META_MASK,
    ] {
        assert!(!BambooEngine::is_valid_state(mask), "mask {mask}");
    }
}

#[test]
fn test_english_number_w2u_in_macro_mode() {
    let mut e = new_test_engine(&[], false);
    e.macro_enabled = true;
    type_keys(&mut e, "qwen2");
    assert_eq!(e.preedit_text, "qwen2");
    e.commit_preedit_and_reset(String::new());
    type_keys(&mut e, "new1");
    assert_eq!(e.preedit_text, "new1");
}
