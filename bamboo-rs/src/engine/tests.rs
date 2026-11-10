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
    let table = pairs.iter().map(|&(k, v)| (k.to_string(), v.to_string()));
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
        ("Abc", MacroCase::Capitalized),
        ("ABc", MacroCase::NoChange),
        ("Đô", MacroCase::Capitalized),
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
    assert_eq!(e.expand_macro("Ko", "không"), "Không");
    assert_eq!(e.expand_macro("kO", "không"), "không");
    for (text, want) in [
        ("iPhone", "iPhone"),
        ("iOS", "iOS"),
        ("ßa", "ßa"),
        ("đi", "Đi"),
    ] {
        assert_eq!(e.expand_macro("Ab", text), want, "{text}");
    }
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
fn test_dict_mode_accepts_either_tone_position() {
    let mut e = new_test_engine(&["loá"], true);
    type_keys(&mut e, "loas");
    assert_eq!(e.preedit_text, "lóa");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(
        e.commit_text, "lóa ",
        "old style against a modern-style entry"
    );

    let mut e = new_test_engine(&["thủy"], true);
    e.preeditor
        .set_config(Config::builder().tone_style(ToneStyle::Traditional).build());
    type_keys(&mut e, "thuyr");
    assert_eq!(e.preedit_text, "thuỷ");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(
        e.commit_text, "thuỷ ",
        "modern style against an old-style entry"
    );
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
fn test_restore_word_before_digits_with_macros() {
    for (keys, want) in [
        ("Sose2026", "Sose2026 "),
        ("sose2", "sose2 "),
        ("tooi2", "tôi2 "),
        ("2026", "2026 "),
    ] {
        let mut e = new_test_engine(&["tôi"], true);
        e.macro_enabled = true;
        type_keys(&mut e, keys);
        e.process_key_event(KEY_SPACE, 0);
        assert_eq!(e.commit_text, want, "{keys}");
    }
}

#[test]
fn test_english_word_keeps_a_letter_telex_undid() {
    for word in [
        "issue",
        "sorry",
        "coffee",
        "different",
        "mess",
        "tests",
        "costs",
    ] {
        let mut e = new_test_engine(&[], true);
        type_keys(&mut e, word);
        e.process_key_event(KEY_SPACE, 0);
        assert_eq!(e.commit_text, format!("{word} "));
    }
}

#[test]
fn test_english_word_keeps_its_case() {
    for word in ["Issue", "ISSUE"] {
        let mut e = new_test_engine(&[], true);
        type_keys(&mut e, word);
        e.process_key_event(KEY_SPACE, 0);
        assert_eq!(e.commit_text, format!("{word} "));
    }
}

#[test]
fn test_english_word_is_kept_at_tab_and_enter() {
    for key in [KEY_TAB, KEY_RETURN] {
        let mut e = new_test_engine(&[], true);
        type_keys(&mut e, "issue");
        e.process_key_event(key, 0);
        assert_eq!(e.commit_text, "issue");
    }
}

#[test]
fn test_doubled_key_still_undoes_a_tone() {
    // How people type an English word that would otherwise take a tone.
    for (keys, shown) in [
        ("tesst", "test"),
        ("besst", "best"),
        ("iss", "is"),
        ("thiss", "this"),
        ("off", "of"),
        ("ass", "as"),
        ("rooom", "room"),
        ("dowwn", "down"),
    ] {
        let mut e = new_test_engine(&[], true);
        type_keys(&mut e, keys);
        e.process_key_event(KEY_SPACE, 0);
        assert_eq!(e.commit_text, format!("{shown} "), "keys {keys}");
    }
}

#[test]
fn test_doubled_key_still_types_a_capitalized_name() {
    for (keys, shown) in [("Marry", "Mary"), ("Loss", "Los"), ("Less", "Les")] {
        let mut e = new_test_engine(&[], true);
        type_keys(&mut e, keys);
        e.process_key_event(KEY_SPACE, 0);
        assert_eq!(e.commit_text, format!("{shown} "), "keys {keys}");
    }
    let mut e = new_test_engine(&[], true);
    type_keys(&mut e, "loss");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.commit_text, "loss ");
}

#[test]
fn test_doubled_key_still_types_a_short_word() {
    let mut e = new_test_engine(&[], true);
    type_keys(&mut e, "arr");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.commit_text, "ar ");
}

#[test]
fn test_english_word_with_a_digit_after_it() {
    let mut e = new_test_engine(&[], true);
    e.macro_enabled = true;
    type_keys(&mut e, "issue1");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.commit_text, "issue1 ");
}

#[test]
fn test_english_word_is_kept_when_committed_without_a_break() {
    let mut e = new_test_engine(&[], true);
    type_keys(&mut e, "issue");
    e.commit_preedit();
    assert_eq!(e.commit_text, "issue");

    let mut e = new_test_engine(&[], true);
    type_keys(&mut e, "issue");
    e.process_key_event(KEY_RETURN, CONTROL_MASK);
    assert_eq!(e.commit_text, "issue");
}

#[test]
fn test_english_word_list_needs_auto_restore() {
    let mut e = new_test_engine(&[], true);
    e.auto_non_vn_restore = false;
    type_keys(&mut e, "issue");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.commit_text, "isue ");
}

#[test]
fn test_english_word_list_is_typed_back() {
    for word in ENGLISH_WORDS.lines() {
        let mut e = new_test_engine(&[], true);
        type_keys(&mut e, word);
        e.process_key_event(KEY_SPACE, 0);
        assert_eq!(e.commit_text, format!("{word} "));
    }
}

// Prints what each word in NGOSEN_WORDS shows before its word ends, for misc/gen-english-words.py.
#[test]
#[ignore = "generator input, not a check"]
fn print_shown_words() {
    let path = std::env::var("NGOSEN_WORDS").unwrap();
    for word in std::fs::read_to_string(path).unwrap().lines() {
        let mut e = new_test_engine(&[], true);
        type_keys(&mut e, word);
        println!("SHOWN\t{word}\t{}", e.preedit_text);
    }
}

/// The engine the settings' defaults and the shipped dictionary give.
fn engine_with_default_settings(method: &str) -> BambooEngine {
    let dictionary: Vec<&str> = include_str!("../../../data/dictionaries/vietnamese.cm.dict")
        .lines()
        .collect();
    let mut e = new_test_engine(&dictionary, true);
    // What EngineSetOption builds from the settings' defaults.
    let config = Config::builder()
        .free_tone_marking(true)
        .tone_style(ToneStyle::Traditional)
        .auto_correct(false)
        .w2u_mode(W2uMode::NonStart)
        .bracket_mode(BracketMode::Disabled)
        .build();
    e.preeditor = Engine::with_config(input_method_by_name(method), config);
    e.macro_enabled = true;
    e
}

// Prints the Telex words on each line of NGOSEN_WORDS that a space would retone if they were not
// in english-retone-lookalikes.txt, for misc/gen-english-words.py.
#[test]
#[ignore = "measurement input, not a check"]
fn print_retone_lookalikes() {
    let path = std::env::var("NGOSEN_WORDS").unwrap();
    let mut e = engine_with_default_settings("Telex");
    for word in std::fs::read_to_string(path).unwrap().lines() {
        e.reset();
        type_keys(&mut e, word);
        if let Some(retoned) = e.retoned_word_of(word) {
            println!("LOOKALIKE\t{word}\t{retoned}");
        }
    }
}

// Prints what the keys on each line of NGOSEN_WORDS commit when a space ends them, with the
// default settings and dictionary, for misc/check-word-endings.py.
#[test]
#[ignore = "measurement input, not a check"]
fn print_committed_words() {
    let path = std::env::var("NGOSEN_WORDS").unwrap();
    let method = std::env::var("NGOSEN_INPUT_METHOD").unwrap_or_else(|_| "Telex".to_string());
    let mut e = engine_with_default_settings(&method);
    for line in std::fs::read_to_string(path).unwrap().lines() {
        let keys = line.split('\t').next().unwrap_or_default();
        e.reset();
        type_keys(&mut e, keys);
        e.process_key_event(KEY_SPACE, 0);
        // Debug format, so a lost or doubled space shows.
        println!("COMMITTED\t{keys}\t{:?}", e.commit_text);
    }
}

/// What `keys` commit when `end` ends the word, with the settings' defaults.
fn committed_with(method: &str, keys: &str, end: u32) -> String {
    let mut e = engine_with_default_settings(method);
    type_keys(&mut e, keys);
    e.process_key_event(end, 0);
    e.commit_text
}

#[test]
fn test_tone_changed_after_final_consonant() {
    for (keys, word) in [
        ("dduoxwcj", "được"),
        ("DDUOXWCJ", "ĐƯỢC"),
        ("Dduoxwcj", "Được"),
        ("thwxcs", "thức"),
        ("dduowcxj", "được"),
        ("mootxj", "một"),
        ("hocxj", "học"),
    ] {
        assert_eq!(
            committed_with("Telex", keys, KEY_SPACE),
            format!("{word} "),
            "{keys}"
        );
        assert_eq!(
            committed_with("Telex", keys, ',' as u32),
            format!("{word},"),
            "{keys}"
        );
        assert_eq!(committed_with("Telex", keys, KEY_RETURN), word, "{keys}");
    }
    for (keys, word) in [
        ("d9uo37c5", "được"),
        ("d9uo7c35", "được"),
        ("mo6t45", "một"),
    ] {
        assert_eq!(
            committed_with("VNI", keys, KEY_SPACE),
            format!("{word} "),
            "{keys}"
        );
    }
}

#[test]
fn test_tone_change_keeps_other_words() {
    for keys in [
        "hists",
        "carr",
        "parts",
        "texts",
        "nextjs",
        "expressjs",
        "sexxts",
    ] {
        let shown = committed_with("Telex", keys, KEY_SPACE);
        assert!(shown.is_ascii(), "{keys} gave {shown}");
    }
    assert_eq!(committed_with("Telex", "chuwaxs", KEY_SPACE), "chứa ");
    assert_eq!(committed_with("Telex", "dduowcj", KEY_SPACE), "được ");
}

#[test]
fn test_tone_change_needs_dictionary_word_without_spell_check() {
    for (keys, want) in [
        ("ports", "ports "),
        ("gifts", "gifts "),
        ("dduoxwcj", "được "),
    ] {
        let mut e = engine_with_default_settings("Telex");
        e.spell_check_with_dicts = false;
        type_keys(&mut e, keys);
        e.process_key_event(KEY_SPACE, 0);
        assert_eq!(e.commit_text, want, "{keys}");
    }
}

#[test]
fn test_english_word_list_is_typed_back_with_default_settings() {
    let mut e = engine_with_default_settings("Telex");
    for word in ENGLISH_WORDS.lines() {
        e.reset();
        type_keys(&mut e, word);
        e.process_key_event(KEY_SPACE, 0);
        assert_eq!(e.commit_text, format!("{word} "));
    }
}

// Gõ Sen keeps the shown word on these keys, so Preedit must too.
#[test]
fn test_tone_change_waits_for_space_or_enter() {
    let mut e = engine_with_default_settings("Telex");
    type_keys(&mut e, "dduoxwcj");
    assert!(e.preedit_text.is_ascii(), "{}", e.preedit_text);
    e.commit_preedit();
    assert!(e.commit_text.is_ascii(), "{}", e.commit_text);
    assert!(committed_with("Telex", "dduoxwcj", KEY_TAB).is_ascii());
    assert!(
        committed_with("Telex", "dduoxwcj", 0xff1b).is_ascii(),
        "Escape"
    );
}

#[test]
fn test_tone_change_needs_free_marking_and_restore() {
    for off in [
        |e: &mut BambooEngine| e.auto_non_vn_restore = false,
        |e: &mut BambooEngine| {
            let config = Config {
                free_tone_marking: false,
                ..e.preeditor.config()
            };
            e.preeditor = Engine::with_config(input_method_by_name("Telex"), config);
        },
    ] {
        let mut e = engine_with_default_settings("Telex");
        off(&mut e);
        type_keys(&mut e, "dduoxwcj");
        e.process_key_event(KEY_SPACE, 0);
        assert_ne!(e.commit_text, "được ");
    }
}

#[test]
fn test_retone_lookalikes_are_listed_once_in_order() {
    let words: Vec<&str> = ENGLISH_RETONE_LOOKALIKES.lines().collect();
    assert!(words.is_sorted_by(|a, b| a < b), "sorted, no duplicates");
    for word in words {
        assert!(
            committed_with("Telex", word, KEY_SPACE).is_ascii(),
            "{word}"
        );
        let mut e = engine_with_default_settings("Telex");
        type_keys(&mut e, word);
        assert!(
            e.retoned_word_of(word).is_some(),
            "{word} is not typed like a retone"
        );
    }
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
fn enter_commits_macro_expansion() {
    // Enter still goes to the app, after the expansion.
    for enter in [KEY_RETURN, KEY_KP_ENTER] {
        let mut e = new_test_engine(&[], false);
        e.macro_enabled = true;
        e.macro_table = macro_table(&[("dc", "được")]);
        type_keys(&mut e, "dc");
        assert!(!e.process_key_event(enter, 0));
        assert_eq!(e.commit_text, "được");
    }

    let mut e = new_test_engine(&[], false);
    e.macro_enabled = true;
    e.macro_table = macro_table(&[("dc", "được")]);
    type_keys(&mut e, "xx");
    assert!(!e.process_key_event(KEY_RETURN, 0));
    assert_eq!(e.commit_text, "xx", "no macro match");

    let mut e = new_test_engine(&[], false);
    e.macro_enabled = true;
    e.macro_table = macro_table(&[("dc", "được")]);
    type_keys(&mut e, "dc");
    assert!(!e.process_key_event(KEY_RETURN, CONTROL_MASK));
    assert_eq!(e.commit_text, "dc", "Ctrl+Enter keeps the typed text");
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
fn test_super_shortcut_goes_to_app() {
    let mut e = new_test_engine(&[], false);
    type_keys(&mut e, "Loi");
    // Super+Q with Num Lock on, as fcitx5 reports it.
    assert!(!e.process_key_event('q' as u32, MOD4_MASK | (1 << 4)));
    assert!(!e.process_key_event('w' as u32, MOD4_MASK));
    assert_eq!(
        e.commit_text, "Loi",
        "the word is committed, as before Ctrl"
    );
    assert_eq!(e.get_preedit_string(), "");
}

#[test]
fn test_is_valid_state() {
    assert!(BambooEngine::is_valid_state(0));
    assert!(BambooEngine::is_valid_state(SHIFT_MASK), "shift is allowed");
    for mask in [
        CONTROL_MASK,
        MOD1_MASK,
        MOD4_MASK,
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

#[test]
fn test_word_before_digits_shown_as_typed() {
    let mut e = engine_with_default_settings("Telex");
    let mut shown = Vec::new();
    for key in "Sose2026".chars() {
        e.process_key_event(key as u32, 0);
        shown.push(e.preedit_text.clone());
    }
    assert_eq!(
        shown,
        [
            "S", "So", "Só", "Soé", "Sose2", "Sose20", "Sose202", "Sose2026"
        ]
        .map(String::from)
    );
}

#[test]
fn test_vietnamese_word_before_digits_keeps_its_marks() {
    for (keys, want) in [
        ("tooi2", "tôi2"),
        ("as2", "á2"),
        ("off2", "of2"),
        ("2026", "2026"),
    ] {
        let mut e = engine_with_default_settings("Telex");
        type_keys(&mut e, keys);
        assert_eq!(e.preedit_text, want, "{keys}");
    }
}

#[test]
fn test_digit_restore_follows_the_space_rules() {
    let mut e = engine_with_default_settings("Telex");
    e.auto_non_vn_restore = false;
    type_keys(&mut e, "Sose2");
    assert_eq!(e.preedit_text, "Soé2", "restore off");

    let mut e = engine_with_default_settings("Telex");
    e.macro_table = macro_table(&[("soé2", "x")]);
    type_keys(&mut e, "Sose2");
    assert_eq!(e.preedit_text, "Soé2", "macro key");

    let mut e = engine_with_default_settings("Telex");
    type_keys(&mut e, "Sose2a");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.commit_text, "Soé2a ", "letters after the digits");
}

#[test]
fn test_backspace_keeps_word_shown_as_typed() {
    let mut e = engine_with_default_settings("Telex");
    type_keys(&mut e, "Sose2");
    e.process_key_event(KEY_BACKSPACE, 0);
    assert_eq!(e.preedit_text, "Sose");
    e.process_key_event('2' as u32, 0);
    assert_eq!(e.preedit_text, "Sose2");

    let mut e = engine_with_default_settings("Telex");
    type_keys(&mut e, "ban2");
    e.process_key_event(KEY_BACKSPACE, 0);
    type_keys(&mut e, "s");
    assert_eq!(e.preedit_text, "bán", "a plain word still takes a tone");

    // The app keeps the marks it shows, so they stay until the next key.
    let mut e = engine_with_default_settings("Telex");
    type_keys(&mut e, "Sose2a");
    e.process_key_event(KEY_BACKSPACE, 0);
    assert_eq!(e.preedit_text, "Soé2");
    e.process_key_event('6' as u32, 0);
    assert_eq!(e.preedit_text, "Sose26");
}

#[test]
fn test_backspace_keeps_typed_part_before_digit() {
    for (keys, want) in [
        ("thuwcx2tooi2", "thuwcx2tôi"),
        ("considered2tooi2", "considered2tôi"),
        ("tooi2tooi2", "tôi2tôi"),
    ] {
        let mut e = engine_with_default_settings("Telex");
        type_keys(&mut e, keys);
        e.process_key_event(KEY_BACKSPACE, 0);
        assert_eq!(e.preedit_text, want, "{keys} + Backspace");
    }
}

#[test]
fn test_word_before_digits_shown_as_space_commits() {
    for keys in ["tooi2Sose2", "Sose2tooi2", "Sose2a", "dd2"] {
        let mut shown = engine_with_default_settings("Telex");
        type_keys(&mut shown, keys);
        let mut spaced = engine_with_default_settings("Telex");
        type_keys(&mut spaced, keys);
        spaced.process_key_event(KEY_SPACE, 0);
        assert_eq!(
            format!("{} ", shown.preedit_text),
            spaced.commit_text,
            "{keys}"
        );
    }
}

#[test]
fn test_reopened_word_before_digits_backspaces_as_shown() {
    let mut e = engine_with_default_settings("Telex");
    e.rebuild_from_text("Sose2026");
    assert_eq!(e.preedit_text, "Sose2026");
    e.process_key_event(KEY_BACKSPACE, 0);
    assert_eq!(e.preedit_text, "Sose202");
}

#[test]
fn test_reopened_word_keeps_its_marks_when_a_digit_is_deleted() {
    let mut e = engine_with_default_settings("Telex");
    e.rebuild_from_text("tôi2");
    assert_eq!(e.preedit_text, "tôi2");
    e.process_key_event(KEY_BACKSPACE, 0);
    assert_eq!(e.preedit_text, "tôi");
}

fn engine_with_english(words: &[&str]) -> BambooEngine {
    let mut e = engine_with_default_settings("Telex");
    e.english_words = Arc::new(words.iter().map(|w| w.to_string()).collect());
    e
}

#[test]
fn test_backspace_keeps_english_keys_as_typed() {
    // `ar` alone composes ả; the app still shows the keys of `are` minus one.
    let mut e = engine_with_english(&["are", "perl"]);
    type_keys(&mut e, "are");
    e.process_key_event(KEY_BACKSPACE, 0);
    assert_eq!(e.preedit_text, "ar");

    let mut e = engine_with_english(&["are", "perl"]);
    type_keys(&mut e, "Perl");
    e.process_key_event(KEY_BACKSPACE, 0);
    e.process_key_event(KEY_BACKSPACE, 0);
    assert_eq!(e.preedit_text, "Pe");
}

#[test]
fn test_backspace_composes_keys_that_are_not_english() {
    let mut e = engine_with_english(&["are", "perl"]);
    type_keys(&mut e, "dduoxwc");
    e.process_key_event(KEY_BACKSPACE, 0);
    assert_eq!(e.preedit_text, "đưỡ");
    assert!(!e.shown_as_typed());
}

#[test]
fn test_key_after_kept_english_keys_composes_again() {
    let mut e = engine_with_english(&["are"]);
    type_keys(&mut e, "are");
    e.process_key_event(KEY_BACKSPACE, 0);
    type_keys(&mut e, "s");
    let mut typed = engine_with_english(&["are"]);
    type_keys(&mut typed, "ars");
    assert_eq!(e.preedit_text, typed.preedit_text);
}
