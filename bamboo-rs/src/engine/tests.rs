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
    type_keys(&mut e, "chaf");
    assert_eq!(e.preedit_text, "chà");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.commit_text, "chaf ", "dict miss restores the keys");
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

// Prints the words on each line of NGOSEN_WORDS that a space would give marks if they were not
// in english-mark-lookalikes.txt, for misc/gen-english-words.py.
#[test]
#[ignore = "measurement input, not a check"]
fn print_mark_lookalikes() {
    let path = std::env::var("NGOSEN_WORDS").unwrap();
    let mut e = engine_with_default_settings("Telex");
    for word in std::fs::read_to_string(path).unwrap().lines() {
        e.reset();
        type_keys(&mut e, word);
        if let Some(marked) = e.auto_marked_word_of(word) {
            println!("LOOKALIKE\t{word}\t{marked}");
        }
    }
}

// Prints what the keys on each line of NGOSEN_WORDS commit when a space ends them, with the
// default settings and dictionary, for misc/check-word-endings.py. NGOSEN_SPACE set to `plain` or
// `fix` loads the keep list as the apps do, and `fix` lets Space fix a kept word as it does in a
// field that can take the rewrite. NGOSEN_TEENCODE=1 turns teencode mode on.
#[test]
#[ignore = "measurement input, not a check"]
fn print_committed_words() {
    let path = std::env::var("NGOSEN_WORDS").unwrap();
    let method = std::env::var("NGOSEN_INPUT_METHOD").unwrap_or_else(|_| "Telex".to_string());
    let space = std::env::var("NGOSEN_SPACE").unwrap_or_default();
    let mut e = engine_with_default_settings(&method);
    e.english_words = shipped_english_words();
    e.teencode = std::env::var("NGOSEN_TEENCODE").is_ok_and(|v| v == "1");
    match space.as_str() {
        "" => {}
        "plain" | "fix" => {
            let words = include_str!("../../../data/keep-words.txt");
            e.keep_guard = Arc::new(words.lines().map(str::to_string).collect());
        }
        other => panic!("NGOSEN_SPACE={other}: expected plain or fix"),
    }
    for line in std::fs::read_to_string(path).unwrap().lines() {
        let keys = line.split('\t').next().unwrap_or_default();
        e.reset();
        type_keys(&mut e, keys);
        e.drop_kept_tail = space == "fix";
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
fn test_marks_added_to_rimes_that_always_have_them() {
    for (keys, word) in [
        ("benhj", "bệnh"),
        ("luanj", "luận"),
        ("cuops", "cướp"),
        ("khuays", "khuấy"),
        ("quyets", "quyết"),
        ("NGUYENX", "NGUYỄN"),
        ("ddieuf", "điều"),
        ("dieuf", "diều"),
        ("ruouj", "rượu"),
    ] {
        assert_eq!(
            committed_with("Telex", keys, KEY_SPACE),
            format!("{word} "),
            "{keys}"
        );
    }
    // A digit tone in VNI could be a number, and `user` has its tone key between the vowels.
    for (method, keys) in [
        ("VNI", "tien2"),
        ("Telex + VNI", "tien2"),
        ("Telex", "user"),
        ("Telex", "techs"),
        ("Telex", "Hersch"),
    ] {
        assert_eq!(
            committed_with(method, keys, KEY_SPACE),
            format!("{keys} "),
            "{method} {keys}"
        );
    }
    // Sentence punctuation ends a word; a digit, @ or _ may go on with a name or a handle.
    for end in ['.', '"', ')'] {
        assert_eq!(
            committed_with("Telex", "tienf", end as u32),
            format!("tiền{end}"),
            "tienf{end}"
        );
    }
    for end in ['@', '_'] {
        assert_eq!(
            committed_with("Telex", "tuans", end as u32),
            format!("tuans{end}"),
            "tuans{end}"
        );
    }
    // Without spell checking luạn is a valid syllable, but the dictionary still has luận.
    let mut e = engine_with_default_settings("Telex");
    e.spell_check_with_dicts = false;
    type_keys(&mut e, "luanj");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.commit_text, "luận ");
    // Macros keep a digit in the word, so they are off for the digit to end it.
    let mut e = engine_with_default_settings("Telex");
    e.macro_enabled = false;
    type_keys(&mut e, "tuans");
    e.process_key_event('2' as u32, 0);
    assert_eq!(e.commit_text, "tuans2");
}

#[test]
fn test_tone_typed_after_a_final_the_rules_reject() {
    for (keys, word) in [
        ("peks", "pék"),
        ("PEKS", "PÉK"),
        ("ngokj", "ngọk"),
        ("puchs", "púch"),
        ("tehr", "tẻh"),
        ("tringj", "trịng"),
        ("eengf", "ềng"),
    ] {
        assert_eq!(
            committed_with("Telex", keys, KEY_SPACE),
            format!("{word} "),
            "{keys}"
        );
    }
    let mut e = engine_with_default_settings("Telex");
    e.english_words = shipped_english_words();
    type_keys(&mut e, "peks");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.commit_text, "pék ", "with the English list");
    // The last s of hursts undoes the tone its r put on, so it must not put one back.
    for keys in ["hursts", "Burnss", "barff", "sends", "peksx"] {
        let text = committed_with("Telex", keys, KEY_SPACE);
        assert_eq!(without_tones(&text.to_lowercase()).1, 0, "{keys}: {text}");
    }
    assert_eq!(committed_with("Telex", "tehr", '.' as u32), "tẻh.");
    assert_eq!(committed_with("Telex", "peks", KEY_RETURN), "pék");
    // EHR is an English abbreviation as well as the keys of ẻh.
    for keys in ["ehr", "EHR"] {
        assert_eq!(committed_with("Telex", keys, KEY_SPACE), format!("{keys} "));
    }
    assert_eq!(committed_with("Telex", "peks", '@' as u32), "peks@");
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

// Misspelt dictionary entries made these keys commit the misspelling instead of the keys typed.
#[test]
fn test_misspelt_words_commit_the_keys() {
    let wrong: Vec<String> = [
        "moot",
        "nhieet",
        "bieet",
        "vieec",
        "luwc",
        "coot",
        "teet",
        "laap",
        "tienr",
        "khyeenr",
        "quuyeets",
        "tryeenf",
        "nghoeof",
        "ngeof",
        "ngeechj",
        "ngetj",
        "geenhf",
        "nghieenh",
        "ngiuj",
        "gems",
        "nghueechs",
    ]
    .iter()
    .filter_map(|keys| {
        let shown = committed_with("Telex", keys, KEY_SPACE);
        (shown != format!("{keys} ")).then(|| format!("{keys} gave {shown:?}"))
    })
    .collect();
    assert!(wrong.is_empty(), "{} wrong: {wrong:?}", wrong.len());
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
            e.retoned_word_of(word).is_some() || e.late_tone_word_of(word).is_some(),
            "{word} is not typed like a retone"
        );
    }
}

#[test]
fn test_mark_lookalikes_are_listed_once_in_order() {
    let words: Vec<&str> = ENGLISH_MARK_LOOKALIKES.lines().collect();
    assert!(words.is_sorted_by(|a, b| a < b), "sorted, no duplicates");
    for word in words {
        assert_eq!(committed_with("Telex", word, KEY_SPACE), format!("{word} "));
        let mut e = engine_with_default_settings("Telex");
        type_keys(&mut e, word);
        assert!(
            e.auto_marked_word_of(word).is_some(),
            "{word} would not get marks anyway"
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

/// An engine as the apps set it up, expanding the shipped chat abbreviations.
fn shorthand_engine(method: &str) -> BambooEngine {
    let mut e = engine_with_default_settings(method);
    e.english_words = shipped_english_words();
    let pairs: Vec<_> = include_str!("../../../data/chat-shorthand.tsv")
        .lines()
        .filter_map(|line| line.split_once('\t'))
        .collect();
    e.shorthand = macro_table(&pairs);
    e
}

#[test]
fn shorthand_expands_at_a_word_end() {
    for (keys, want) in [
        ("ko ", "không "),
        ("Ko ", "Không "),
        ("hnay?", "hôm nay?"),
        ("tq,", "Trung Quốc,"),
        ("Tq ", "Trung Quốc "),
        ("ddc ", "được "),
        ("Ddc ", "Được "),
    ] {
        let mut e = shorthand_engine("Telex");
        type_keys(&mut e, keys);
        assert_eq!(e.commit_text, want, "{keys}");
    }
}

#[test]
fn shorthand_in_capitals_stays_as_typed() {
    for (keys, want) in [
        ("KO ", "KO "),
        ("kO ", "kO "),
        ("TQ ", "TQ "),
        ("DDC ", "ĐC "),
    ] {
        let mut e = shorthand_engine("Telex");
        type_keys(&mut e, keys);
        assert_eq!(e.commit_text, want, "{keys}");
    }
}

#[test]
fn every_shipped_abbreviation_can_be_typed() {
    for (method, d_stroke) in [("Telex", "dd"), ("VNI", "d9")] {
        for line in include_str!("../../../data/chat-shorthand.tsv").lines() {
            let (key, text) = line.split_once('\t').unwrap();
            let mut e = shorthand_engine(method);
            type_keys(&mut e, &key.replace('đ', d_stroke));
            e.process_key_event(KEY_SPACE, 0);
            assert_eq!(e.commit_text, format!("{text} "), "{method} {key}");
        }
    }
}

#[test]
fn shorthand_is_off_without_a_table() {
    let mut e = engine_with_default_settings("Telex");
    type_keys(&mut e, "ko ");
    assert_eq!(e.commit_text, "ko ");
}

#[test]
fn own_macro_wins_over_shorthand() {
    let mut e = shorthand_engine("Telex");
    e.macro_table = macro_table(&[("ko", "Kansas")]);
    type_keys(&mut e, "ko ");
    assert_eq!(e.commit_text, "Kansas ");

    // The abbreviations do not need the user's macros turned on.
    e.macro_enabled = false;
    type_keys(&mut e, "ko ");
    assert_eq!(e.commit_text, "không ");
}

#[test]
fn enter_commits_shorthand_and_tab_does_not() {
    let mut e = shorthand_engine("Telex");
    type_keys(&mut e, "ko");
    assert!(!e.process_key_event(KEY_RETURN, 0));
    assert_eq!(e.commit_text, "không");

    let mut e = shorthand_engine("Telex");
    type_keys(&mut e, "ko");
    assert!(!e.process_key_event(KEY_TAB, 0), "Tab goes to the app");
    assert_eq!(e.commit_text, "ko");
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
    for key in "suoos2026".chars() {
        e.process_key_event(key as u32, 0);
        shown.push(e.preedit_text.clone());
    }
    assert_eq!(
        shown,
        [
            "s",
            "su",
            "suo",
            "suô",
            "suố",
            "suoos2",
            "suoos20",
            "suoos202",
            "suoos2026"
        ]
        .map(String::from)
    );
}

#[test]
fn test_non_dictionary_prefix_shown_as_typed() {
    let mut e = engine_with_default_settings("Telex");
    let mut shown = Vec::new();
    for key in "roses".chars() {
        e.process_key_event(key as u32, 0);
        shown.push(e.preedit_text.clone());
    }
    assert_eq!(shown, ["r", "ro", "ró", "rose", "roses"].map(String::from));
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.commit_text, "roses ");

    for (keys, want) in [
        ("rose", "rose"),
        ("Sose2026", "Sose2026"),
        ("more", "more"),
        ("case", "case"),
        ("cases", "cases"),
        ("house", "house"),
        ("moses", "moses"),
        ("poisson", "poisson"),
        ("lose", "loé"),
        ("khuyr", "khuỷ"),
        ("khuyru", "khuỷu"),
        ("tienf", "tièn"),
    ] {
        let mut e = engine_with_default_settings("Telex");
        type_keys(&mut e, keys);
        assert_eq!(e.preedit_text, want, "{keys}");
    }
}

#[test]
fn test_backspace_keeps_non_dictionary_prefix_as_typed() {
    // As for a word in the English list: Backspace takes the last key shown, and the next key
    // decides the marks.
    for (method, keys, backspaces, shown, spaced, typed_on, then) in [
        ("Telex", "Sose", 1, "Sos", "Sos ", "t", "Sót"),
        ("Telex", "Sose", 2, "So", "So ", "s", "Só"),
        ("VNI", "ro1e", 1, "ro1", "ro1 ", "t", "rót"),
    ] {
        let label = format!("{method} {keys} + {backspaces} Backspace");
        let mut e = engine_with_default_settings(method);
        type_keys(&mut e, keys);
        for _ in 0..backspaces {
            e.process_key_event(KEY_BACKSPACE, 0);
        }
        assert_eq!(e.preedit_text, shown, "{label}");
        let mut next = engine_with_default_settings(method);
        type_keys(&mut next, keys);
        for _ in 0..backspaces {
            next.process_key_event(KEY_BACKSPACE, 0);
        }
        e.process_key_event(KEY_SPACE, 0);
        assert_eq!(e.commit_text, spaced, "{label}, Space");
        type_keys(&mut next, typed_on);
        assert_eq!(next.preedit_text, then, "{label}, {typed_on}");
    }
}

#[test]
fn test_repeated_tone_key_keeps_a_macro() {
    let mut e = engine_with_default_settings("Telex");
    e.macro_table = macro_table(&[("roes", "X")]);
    type_keys(&mut e, "roess");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.commit_text, "X ");
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

/// The English list the apps load next to the dictionary.
fn shipped_english_words() -> Arc<WordList> {
    let words = include_str!("../../../data/english-common.txt");
    Arc::new(words.lines().map(str::to_string).collect())
}

#[test]
fn test_shipped_lists_keep_doubled_keys_and_names() {
    for (keys, shown) in [
        ("tesst", "test"),
        ("iss", "is"),
        ("off", "of"),
        ("ass", "as"),
        ("suss", "sus"),
        ("Marry", "Mary"),
        ("Loss", "Los"),
        ("cross", "cross"),
        ("dds", "đs"),
        ("draw", "draw"),
        ("drawng", "drăng"),
        ("ploong", "plông"),
        ("grats", "grát"),
        ("slinf", "slìn"),
        ("sreepoocs", "srêpốc"),
    ] {
        let mut e = engine_with_default_settings("Telex");
        e.english_words = shipped_english_words();
        type_keys(&mut e, keys);
        e.process_key_event(KEY_SPACE, 0);
        assert_eq!(e.commit_text, format!("{shown} "), "keys {keys}");
    }
}

#[test]
fn test_shipped_dictionary_has_place_and_ethnic_names() {
    for (keys, shown) in [
        ("ddakroong", "đakrông"),
        ("mnoong", "mnông"),
        ("kmawl", "kmăl"),
        ("knoops", "knốp"),
    ] {
        let mut e = engine_with_default_settings("Telex");
        e.english_words = shipped_english_words();
        type_keys(&mut e, keys);
        e.process_key_event(KEY_SPACE, 0);
        assert_eq!(e.commit_text, format!("{shown} "), "keys {keys}");
    }
}

#[test]
fn test_english_keys_keep_a_doubled_tone_key_after_a_name() {
    let mut e = engine_with_default_settings("Telex");
    e.english_words = shipped_english_words();
    type_keys(&mut e, "grass");
    assert_eq!(e.preedit_text, "grass");
    for word in ["jussive", "justs"] {
        let mut e = engine_with_english(&[word]);
        type_keys(&mut e, word);
        e.process_key_event(KEY_SPACE, 0);
        assert_eq!(e.commit_text, format!("{word} "));
    }
}

#[test]
fn test_backspace_into_a_name_keeps_what_the_app_shows() {
    let mut e = engine_with_default_settings("Telex");
    e.english_words = shipped_english_words();
    type_keys(&mut e, "grats");
    e.process_key_event(KEY_BACKSPACE, 0);
    assert_eq!(e.preedit_text, "grá");
    type_keys(&mut e, "t");
    assert_eq!(e.preedit_text, "grát");
}

#[test]
fn test_handle_keys_do_not_end_a_name() {
    for (keys, end) in [("croo", '@'), ("croo", '_')] {
        let mut e = engine_with_default_settings("Telex");
        e.english_words = shipped_english_words();
        type_keys(&mut e, keys);
        e.process_key_event(end as u32, 0);
        assert_eq!(e.commit_text, format!("{keys}{end}"));
    }
}

#[test]
fn test_dictionary_names_need_an_english_list() {
    let mut e = engine_with_default_settings("Telex");
    type_keys(&mut e, "draw");
    assert_eq!(e.preedit_text, "draw");
    e.reset();
    type_keys(&mut e, "broom");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.commit_text, "broom ");
}

#[test]
fn test_enter_keeps_english_keys_that_spell_no_name() {
    let mut e = engine_with_default_settings("Telex");
    e.english_words = shipped_english_words();
    type_keys(&mut e, "gras");
    e.process_key_event(KEY_RETURN, 0);
    assert_eq!(e.commit_text, "gras");
}

#[test]
fn test_z_still_removes_the_tone_of_a_name() {
    let mut e = engine_with_default_settings("Telex");
    e.english_words = shipped_english_words();
    type_keys(&mut e, "grasz");
    assert_eq!(e.preedit_text, "gra");
}

#[test]
fn test_dictionary_names_need_spell_checking() {
    let mut e = engine_with_default_settings("Telex");
    e.english_words = shipped_english_words();
    e.spell_check_with_dicts = false;
    type_keys(&mut e, "ploo");
    assert_eq!(e.preedit_text, "ploo");
    type_keys(&mut e, "ng");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.commit_text, "ploong ");
}

#[test]
fn test_dictionary_prefix_keeps_typed_marks() {
    let dictionary: Dictionary = ["plông", "nêu"].into_iter().map(String::from).collect();
    assert!(dictionary.starts_word("plo"));
    assert!(dictionary.starts_word("plô"));
    assert!(!dictionary.starts_word("plơ"));
    assert!(dictionary.starts_word("ne"));
    assert!(!dictionary.starts_word("neư"));
    let dictionary: Dictionary = ["grát", "crông"].into_iter().map(String::from).collect();
    assert!(dictionary.starts_word("grat"));
    assert!(dictionary.starts_word("grá"));
    assert!(!dictionary.starts_word("grà"));
    assert!(!dictionary.starts_word("cró"));
}

#[test]
fn test_dictionary_prefix_waits_for_english_keys_to_end() {
    let mut e = engine_with_english(&["draw", "drawn"]);
    type_keys(&mut e, "draw");
    assert_eq!(e.preedit_text, "draw");
    type_keys(&mut e, "n");
    assert_eq!(e.preedit_text, "drawn");
    type_keys(&mut e, "g");
    assert_eq!(e.preedit_text, "drăng");
    e.reset();
    type_keys(&mut e, "ploo");
    assert_eq!(e.preedit_text, "plô");
}

#[test]
fn test_name_ends_as_composed_when_keys_only_start_english() {
    let mut e = engine_with_english(&["canoodle", "draw"]);
    type_keys(&mut e, "canoo");
    assert_eq!(e.preedit_text, "canoo");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.commit_text, "canô ");
    e.reset();
    type_keys(&mut e, "canoo");
    e.process_key_event(KEY_RETURN, 0);
    assert_eq!(e.commit_text, "canô");
    e.reset();
    type_keys(&mut e, "draw");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.commit_text, "draw ");
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
fn test_backspace_after_a_d_keeps_the_stroke() {
    // The last d takes the stroke off đ while the keys are shown as typed (#202).
    for (keys, left) in [
        ("dduowjcd", "được"),
        ("Dduowjcd", "Được"),
        ("ddeerd", "để"),
        ("ddiajd", "địa"),
    ] {
        let mut e = engine_with_english(&["are", "perl"]);
        type_keys(&mut e, keys);
        assert!(e.shown_as_typed(), "keys {keys}");
        e.process_key_event(KEY_BACKSPACE, 0);
        assert_eq!(e.preedit_text, left, "keys {keys}");
    }
}

fn engine_keeping_words(method: &str, guard: &[&str]) -> BambooEngine {
    let mut e = engine_with_default_settings(method);
    e.english_words = Arc::new(["are", "perl"].iter().map(|w| w.to_string()).collect());
    e.keep_guard = Arc::new(guard.iter().map(|w| w.to_string()).collect());
    e
}

fn commit_of(e: &mut BambooEngine, keys: &str) -> String {
    e.reset();
    type_keys(e, keys);
    e.process_key_event(KEY_SPACE, 0);
    e.pull_commit()
}

#[test]
fn test_key_breaking_a_word_keeps_it() {
    for (method, keys, kept) in [
        ("Telex", "dduowjcd", "đượcd"),
        ("Telex", "Dduowjcd", "Đượcd"),
        ("Telex", "dduowcjdi", "đượcdi"),
        ("Telex", "yeeuvn", "yêuvn"),
        ("VNI", "d9u7o7c5d", "đượcd"),
    ] {
        let mut e = engine_keeping_words(method, &["bank"]);
        type_keys(&mut e, keys);
        assert_eq!(e.preedit_text, kept, "{method} {keys}");
        assert!(!e.shown_as_typed(), "{method} {keys}");
        assert_eq!(
            commit_of(&mut e, keys),
            format!("{kept} "),
            "{method} {keys}"
        );
    }
}

#[test]
fn test_kept_word_left_alone_where_today_it_is_not() {
    for (guard, keys, typed) in [
        // The guard list starts with these keys.
        (&["axit"][..], "axit", "axit "),
        // An empty guard list turns keeping off, as in builds that do not load it.
        (&[][..], "dduowjcd", "dduowjcd "),
        // All caps, as abbreviations are typed.
        (&["bank"][..], "DDUOWJCD", "DDUOWJCD "),
    ] {
        let mut e = engine_keeping_words("Telex", guard);
        assert_eq!(commit_of(&mut e, keys), typed, "{guard:?} {keys}");
    }
    let mut e = engine_keeping_words("Telex", &["bank"]);
    assert_eq!(commit_of(&mut e, "axit"), "ãit ");
}

#[test]
fn test_backspace_into_a_kept_word_composes_it_again() {
    let mut e = engine_keeping_words("Telex", &["bank"]);
    type_keys(&mut e, "dduowjcdi");
    e.process_key_event(KEY_BACKSPACE, 0);
    assert_eq!(e.preedit_text, "đượcd");
    e.process_key_event(KEY_BACKSPACE, 0);
    assert_eq!(e.preedit_text, "được");
    // Composing goes on as for any word: a tone key retones it.
    type_keys(&mut e, "s");
    assert_eq!(e.preedit_text, "đước");
}

#[test]
fn test_kept_word_commits_on_enter_and_tab() {
    let mut e = engine_keeping_words("Telex", &["bank"]);
    type_keys(&mut e, "dduowjcd");
    e.commit_preedit();
    assert_eq!(e.pull_commit(), "đượcd");
    type_keys(&mut e, "dduowjcd");
    e.process_key_event(KEY_TAB, 0);
    assert_eq!(e.pull_commit(), "đượcd");
}

#[test]
fn test_kept_at_last_key_only_on_the_breaking_key() {
    let mut e = engine_keeping_words("Telex", &["bank"]);
    type_keys(&mut e, "dduowjc");
    assert!(!e.kept_at_last_key());
    type_keys(&mut e, "d");
    assert!(e.kept_at_last_key());
    type_keys(&mut e, "i");
    assert!(!e.kept_at_last_key());
}

#[test]
fn test_kept_word_review_cases() {
    let mut e = engine_keeping_words("Telex", &["bank"]);
    e.rebuild_from_text("tôi");
    type_keys(&mut e, "q");
    e.backspace();
    assert_eq!(e.preedit_text, "tôi", "rebuilt word");
    e.reset();
    type_keys(&mut e, "AWG");
    assert_eq!(e.preedit_text, "AWG", "all-caps keys");
    for (keys, want) in [("ROMraid", "ROMraid"), ("ROMr", "RỎM"), ("OFun", "OFun")] {
        e.reset();
        type_keys(&mut e, keys);
        assert_eq!(e.preedit_text, want, "all-caps word");
    }
    let mut e = engine_keeping_words("VNI", &["bank"]);
    type_keys(&mut e, "O6sin");
    assert_eq!(e.preedit_text, "Ôsin", "a digit is not a capital");
    e.reset();
    type_keys(&mut e, "A8G");
    assert_eq!(e.preedit_text, "A8G", "all-caps VNI keys");
}

#[test]
fn test_kept_word_takes_its_macro_like_any_word() {
    for key in [KEY_SPACE, KEY_RETURN, KEY_TAB] {
        let mut e = engine_keeping_words("Telex", &["bank"]);
        e.macro_enabled = true;
        e.macro_table = macro_table(&[("đượcd", "được đâu")]);
        type_keys(&mut e, "dduowjcd");
        e.process_key_event(key, 0);
        assert!(
            e.commit_text.starts_with("được đâu"),
            "{key}: {}",
            e.commit_text
        );
    }
}

#[test]
fn test_backspace_after_restored_keys_keeps_reopened_text() {
    for (im, text, keys, left) in [
        ("Telex", "tooi2", "asq", "tooi2á"),
        ("Telex", "tôi", "q", "tôi"),
        ("VNI", "tôi", "q", "tôi"),
        ("VNI", "1", "a6q", "1â"),
        ("VIQR", "'", "a^q", "'â"),
    ] {
        let mut e = engine_with_default_settings(im);
        e.rebuild_from_text(text);
        type_keys(&mut e, keys);
        e.process_key_event(KEY_BACKSPACE, 0);
        assert_eq!(e.preedit_text, left, "{im} {text} {keys}");
    }
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

#[test]
fn test_dictionary_has_no_unmarked_rimes() {
    // These rimes are only ever written with ê, ơ or ư, so a line without them is a typo.
    const UNMARKED: [&str; 25] = [
        "ien", "ieu", "iem", "ieng", "iet", "iep", "iec", "yen", "yeu", "yem", "yet", "uyen",
        "uyet", "enh", "eu", "ech", "ue", "uech", "uenh", "uan", "uat", "uay", "uu", "uou", "uop",
    ];
    let dictionary = include_str!("../../../data/dictionaries/vietnamese.cm.dict");
    let typos: Vec<&str> = dictionary
        .lines()
        .filter(|word| {
            let (plain, _) = without_tones(&word.to_lowercase());
            // gi and qu read as one consonant: giếng is gi + ếng, quan is qu + an.
            let rime = match plain.strip_prefix("gi").or(plain.strip_prefix("qu")) {
                Some(rest) if !rest.is_empty() => rest,
                _ => plain.trim_start_matches(|c: char| "bcdđghklmnpqrstvx".contains(c)),
            };
            UNMARKED.contains(&rime)
        })
        .collect();
    assert!(typos.is_empty(), "{typos:?}");
}

#[test]
fn test_reopened_kept_word_keeps_its_text() {
    let mut e = engine_keeping_words("Telex", &["bank"]);
    e.rebuild_from_text("đượcd");
    assert_eq!(e.preedit_text, "đượcd");
    type_keys(&mut e, "x");
    assert_eq!(e.preedit_text, "đượcdx");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.pull_commit(), "đượcdx ");
}

#[test]
fn test_backspace_into_a_reopened_kept_word() {
    let mut e = engine_keeping_words("Telex", &["bank"]);
    e.rebuild_from_text("đượcdi");
    e.backspace();
    assert_eq!(e.preedit_text, "đượcd");
    e.backspace();
    assert_eq!(e.preedit_text, "được");
    type_keys(&mut e, "s");
    assert_eq!(e.preedit_text, "đước");
}

#[test]
fn test_reopened_word_in_another_tone_style_is_not_kept() {
    for (im, text, key) in [
        ("Telex", "hòa", "s"),
        ("Telex", "HÒA", "s"),
        ("Telex", "hòa2", "s"),
        ("VNI", "hòa", "1"),
    ] {
        let mut plain = engine_keeping_words(im, &[]);
        plain.rebuild_from_text(text);
        type_keys(&mut plain, key);
        let mut e = engine_keeping_words(im, &["bank"]);
        e.rebuild_from_text(text);
        type_keys(&mut e, key);
        assert_eq!(e.preedit_text, plain.preedit_text, "{im} {text}");
    }
}

fn commit_dropping(e: &mut BambooEngine, keys: &str) -> (String, String) {
    e.reset();
    type_keys(e, keys);
    e.drop_kept_tail = true;
    e.process_key_event(KEY_SPACE, 0);
    (e.pull_commit(), std::mem::take(&mut e.dropped_word))
}

#[test]
fn test_space_drops_keys_after_a_kept_word() {
    let mut e = engine_keeping_words("Telex", &["axit", "lênin"]);
    for (keys, commit, dropped) in [
        ("dduowjcd", "được ", "đượcd"),
        ("dduowjcdi", "được di ", "đượcdi"),
        ("Dduowjcd", "Được ", "Đượcd"),
        ("coongty", "công ty ", "côngty"),
        ("coongtys", "công tý ", "côngtys"),
        ("khoonggian", "không gian ", "khônggian"),
        ("coongTy", "công Ty ", "côngTy"),
        ("Leenin", "Lênin ", ""),
        ("axit", "axit ", ""),
    ] {
        assert_eq!(
            commit_dropping(&mut e, keys),
            (commit.to_string(), dropped.to_string()),
            "{keys}"
        );
    }
    let mut e = engine_keeping_words("VNI", &["bank"]);
    assert_eq!(commit_dropping(&mut e, "d9u7o7c5d").0, "được ");
    assert_eq!(commit_dropping(&mut e, "co6ngty1").0, "công tý ");
    assert_eq!(
        commit_dropping(&mut e, "d9u7o7c5d2").0,
        "được ",
        "VNI keys go"
    );
    let mut e = engine_keeping_words("Telex", &["bank"]);
    assert_eq!(
        commit_dropping(&mut e, "dduowjcd2").0,
        "đượcd2 ",
        "digits stay"
    );
}

#[test]
fn test_space_reads_g_as_ng() {
    let mut e = engine_keeping_words("Telex", &["bank"]);
    for (keys, commit, dropped) in [
        ("nhuwg", "nhưng ", "nhưg"),
        ("Nhuwg", "Nhưng ", "Nhưg"),
        ("khoog", "không ", "khôg"),
        ("cuxg", "cũng ", "cũg"),
        ("ddag", "đang ", "đag"),
        ("ddugs", "đúng ", "đugs"),
        ("nhuwgx", "những ", "nhưgx"),
        ("gieefgs", "giếng ", "giềgs"),
        ("ddugws", "đứng ", "đugws"),
        ("dduowjcg", "được ", "đượcg"),
        ("dduowjcgi", "được gi ", "đượcgi"),
    ] {
        assert_eq!(
            commit_dropping(&mut e, keys),
            (commit.to_string(), dropped.to_string()),
            "{keys}"
        );
    }
    let mut e = engine_keeping_words("VNI", &["bank"]);
    assert_eq!(commit_dropping(&mut e, "nhu7g").0, "nhưng ");
    assert_eq!(commit_dropping(&mut e, "gie62g1").0, "giếng ");
    assert_eq!(commit_dropping(&mut e, "d9ug71").0, "đứng ");
    // Backspace brings the word back from its text, whose keys no longer carry the marks.
    for (method, toned) in [("Telex", "đugs"), ("VNI", "đug1")] {
        let mut e = engine_keeping_words(method, &["bank"]);
        for (text, commit) in [("nhưg", "nhưng "), ("cũg", "cũng "), (toned, "đúng ")] {
            e.reset();
            e.rebuild_from_text(text);
            e.drop_kept_tail = true;
            e.process_key_event(KEY_SPACE, 0);
            assert_eq!(e.pull_commit(), commit, "{method} {text}");
        }
    }
}

#[test]
fn test_keys_after_a_kept_word_stay_unless_asked() {
    let mut e = engine_keeping_words("Telex", &["bank"]);
    assert_eq!(commit_of(&mut e, "dduowjcd"), "đượcd ");
    // Only the key right after the request may drop them.
    e.reset();
    type_keys(&mut e, "dduowjc");
    e.drop_kept_tail = true;
    type_keys(&mut e, "d");
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.pull_commit(), "đượcd ");
    for key in [KEY_RETURN, u32::from(b'.')] {
        e.reset();
        type_keys(&mut e, "dduowjcd");
        e.drop_kept_tail = true;
        e.process_key_event(key, 0);
        assert!(e.pull_commit().starts_with("đượcd"), "{key}");
    }
}

#[test]
fn test_space_keeps_a_tail_of_several_words() {
    let mut e = engine_keeping_words("Telex", &["bank"]);
    e.macro_enabled = true;
    assert_eq!(commit_dropping(&mut e, "coongty2me").0, "côngty2me ");
}

#[test]
fn test_space_keeps_stretched_words() {
    let mut e = engine_keeping_words("Telex", &["bank"]);
    for (keys, commit) in [
        ("uwfaaa", "ừaaa "),
        ("owfaaa", "ờaaa "),
        ("vuwfaaa", "vừaaa "),
        ("muwaaaa", "mưaaaa "),
        ("uwfeee", "ừeee "),
        ("owiii", "ơiii "),
        ("trowifii", "trờiii "),
        ("trowifiii", "trờiiii "),
        ("nayfyyy", "nàyyyy "),
        ("khoonggg", "khônggg "),
    ] {
        assert_eq!(
            commit_dropping(&mut e, keys),
            (commit.to_string(), String::new()),
            "{keys}"
        );
    }
    // Two-letter second words and bounced consonant keys still split or trim.
    assert_eq!(commit_dropping(&mut e, "caisoo").0, "cái ô ");
    assert_eq!(commit_dropping(&mut e, "dduowjcc").0, "được ");
    assert_eq!(commit_dropping(&mut e, "dduowjctt").0, "được ");

    let mut vni = engine_keeping_words("VNI", &["bank"]);
    assert_eq!(commit_dropping(&mut vni, "u72aaa").0, "ừaaa ");
    assert_eq!(commit_dropping(&mut vni, "o72aaa").0, "ờaaa ");

    e.reset();
    e.rebuild_from_text("ừaaa");
    e.drop_kept_tail = true;
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.pull_commit(), "ừaaa ");
}

fn teencode_engine(on: bool) -> BambooEngine {
    let mut e = engine_with_default_settings("Telex");
    e.english_words = shipped_english_words();
    e.teencode = on;
    e
}

#[test]
fn test_teencode_keeps_words_typed_online() {
    let mut e = teencode_engine(true);
    for (keys, word) in [
        ("zaayj", "zậy"),
        ("fair", "fải"),
        ("was", "wá"),
        ("ddag", "đag"),
        ("nhuwg", "nhưg"),
        ("nhuwgx", "nhữg"),
        ("khoong", "không"),
        ("tienf", "tiền"),
    ] {
        assert_eq!(commit_of(&mut e, keys), format!("{word} "), "{keys}");
    }
    e.reset();
    type_keys(&mut e, "zaayj");
    assert_eq!(e.preedit_text, "zậy");
}

#[test]
fn test_teencode_leaves_english_loanwords_and_tangles() {
    let mut e = teencode_engine(true);
    for keys in ["tons", "her", "moto", "caras", "cerfs"] {
        assert_eq!(commit_of(&mut e, keys), format!("{keys} "), "{keys}");
    }
}

#[test]
fn test_teencode_off_shows_the_keys() {
    let mut e = teencode_engine(false);
    for keys in ["zaayj", "fair", "was", "nhuwgx"] {
        assert_eq!(commit_of(&mut e, keys), format!("{keys} "), "{keys}");
    }
}

#[test]
fn test_teencode_lets_stretched_words_end_as_before() {
    let mut on = teencode_engine(true);
    let mut off = teencode_engine(false);
    for keys in ["nhaa", "dduowjcc"] {
        assert_eq!(
            commit_of(&mut on, keys),
            commit_of(&mut off, keys),
            "{keys}"
        );
    }
}

#[test]
fn test_teencode_keeps_held_letters() {
    let mut e = teencode_engine(true);
    for (keys, word) in [
        ("nhaaa", "nhaaa"),
        ("chaiiif", "chàiii"),
        ("kiaaaf", "kìaaa"),
        ("chaammms", "chấmmm"),
        ("chowttttts", "chớttttt"),
        ("dzowiiiif", "dzờiiii"),
        ("khoonggg", "khônggg"),
    ] {
        assert_eq!(commit_of(&mut e, keys), format!("{word} "), "{keys}");
    }
    let mut off = teencode_engine(false);
    assert_eq!(commit_of(&mut off, "chaiiif"), "chàiii ");
    // An acronym in capitals takes no tone from its last letter.
    assert_eq!(commit_of(&mut e, "WAAAF"), commit_of(&mut off, "WAAAF"));
}

#[test]
fn test_backspace_keeps_the_tone_of_a_stretched_or_teencode_word() {
    for (teencode, keys, after) in [
        (false, "chaiiif", "chàii"),
        (false, "aaaf", "àa"),
        (true, "chaiiif", "chàii"),
        (true, "fair", "fả"),
        (true, "nhuwgx", "nhữ"),
    ] {
        let mut e = teencode_engine(teencode);
        type_keys(&mut e, keys);
        e.process_key_event(KEY_BACKSPACE, 0);
        assert_eq!(e.preedit_text, after, "{keys}");
        e.process_key_event(KEY_SPACE, 0);
        assert_eq!(e.pull_commit(), format!("{after} "), "{keys}");
    }
    let mut e = teencode_engine(false);
    type_keys(&mut e, "chaiiif");
    e.process_key_event(KEY_BACKSPACE, 0);
    type_keys(&mut e, "i");
    assert_eq!(e.preedit_text, "chàiii");
    e.drop_kept_tail = true;
    e.process_key_event(KEY_SPACE, 0);
    assert_eq!(e.pull_commit(), "chàiii ");
}

#[test]
fn test_teencode_keeps_abbreviated_phrases() {
    let mut e = teencode_engine(true);
    for (keys, word) in [
        ("cgais", "cgái"),
        ("bthuwowngf", "bthường"),
        ("ddthoaij", "đthoại"),
        ("ctrinhf", "ctrình"),
        ("ngddos", "ngđó"),
    ] {
        assert_eq!(commit_of(&mut e, keys), format!("{word} "), "{keys}");
    }
}

#[test]
fn test_teencode_abbreviations_leave_english_and_one_syllable() {
    let mut on = teencode_engine(true);
    let mut off = teencode_engine(false);
    // English clusters, acronyms and titles.
    for keys in ["Blas", "Stans", "fstore", "CMOS", "BPharm"] {
        assert_eq!(
            commit_of(&mut on, keys),
            commit_of(&mut off, keys),
            "{keys}"
        );
    }
    // One syllable is not split into a prefix and a word.
    assert_eq!(commit_of(&mut on, "nghieemr"), "nghiểm ");
    assert_eq!(commit_of(&mut on, "chawc"), "chăc ");
}

#[test]
fn test_retyped_tone_after_a_final_is_not_kept() {
    let mut e = engine_keeping_words("Telex", &["axit"]);
    for (keys, word) in [
        ("mooxtj", "một"),
        ("toorts", "tốt"),
        ("ddaafts", "đất"),
        ("gheefchs", "ghếch"),
        ("ddacxs", "đác"),
    ] {
        assert_eq!(commit_of(&mut e, keys), format!("{word} "), "{keys}");
        assert_eq!(
            commit_dropping(&mut e, keys).0,
            format!("{word} "),
            "{keys}"
        );
    }
}

#[test]
fn only_input_methods_with_digit_keys_compose_with_digits() {
    for (name, digits) in [
        ("Telex", false),
        ("Telex W", false),
        ("Telex 2", false),
        ("VIQR", false),
        ("VNI", true),
        ("Telex + VNI", true),
        ("Telex + VNI + VIQR", true),
    ] {
        let e = BambooEngine::new(input_method_by_name(name), Arc::default(), Arc::default());
        assert_eq!(e.composes_with('1' as u32), digits, "{name}");
        assert!(e.composes_with('a' as u32), "{name}");
    }
}

fn finished_word(keys: &str) -> (String, String) {
    let mut e = engine_with_default_settings("Telex");
    e.english_words = shipped_english_words();
    type_keys(&mut e, keys);
    let shown = e.preedit_text.clone();
    e.process_key_event(KEY_SPACE, 0);
    (shown, e.commit_text.clone())
}

#[test]
fn test_held_last_letter_repeats_without_marks() {
    for (keys, word) in [
        ("afaaa", "àaaa"),
        ("ooiiiii", "ôiiiii"),
        ("ddusngggg", "đúngggg"),
        ("quasaaa", "quáaaa"),
        ("chafooo", "chàooo"),
        ("AFAAA", "ÀAAA"),
        ("Owsoo", "Ớoo"),
        ("quasaa", "quáaa"),
        ("aaaaaaaf", "àaaaaaa"),
        ("chaooof", "chàooo"),
        ("quaaaas", "quáaaa"),
        ("ddungggggs", "đúnggggg"),
    ] {
        let (shown, committed) = finished_word(keys);
        assert_eq!(shown, word, "{keys} while typing");
        assert_eq!(committed, format!("{word} "), "{keys}");
    }
}

#[test]
fn test_english_words_with_doubled_ends_stay_english() {
    for keys in [
        "all", "add", "free", "coffee", "kiss", "boss", "buzz", "jazz", "wee", "fee", "www",
        "besttt", "thereee", "cheeseee", "afrooo", "boommm",
    ] {
        assert_eq!(finished_word(keys).1, format!("{keys} "), "{keys}");
    }
}

#[test]
fn test_tone_key_after_undone_mark_adds_one_letter() {
    // The third o took the circumflex off, so the s after boot must not bring that o back (#959).
    for (keys, word) in [
        ("booots", "boots"),
        ("roooms", "rooms"),
        ("rooomf", "roomf"),
        ("Booots", "Boots"),
        // A word that still composes after a removed tone keeps its marks.
        ("asznhs", "ánh"),
    ] {
        assert_eq!(
            finished_word(keys),
            (word.into(), format!("{word} ")),
            "{keys}"
        );
    }
}

fn no_words_near(_keys: &str) -> Vec<(String, f64)> {
    Vec::new()
}

fn learning_on() -> Arc<Learning> {
    Arc::new(Learning::new(true))
}

fn shared_dictionary() -> Arc<std::sync::Mutex<timsen::Dictionary>> {
    let table = timsen::Table::from_bytes(include_bytes!("tiny.timsen").to_vec()).unwrap();
    Arc::new(std::sync::Mutex::new(timsen::Dictionary::new(Arc::new(
        table,
    ))))
}

/// What the front end does with the changes: each counted back from the cursor, and only where
/// the old text still stands.
fn apply_changes(changes: Vec<FieldChange>, field: &mut [String]) {
    for FieldChange { change, after } in changes {
        let end = field.len() - after;
        let first = end - change.old.len();
        assert_eq!(field[first..end], change.old, "{change:?}");
        field[first..end].clone_from_slice(&change.new);
    }
}

fn commit_into(e: &mut BambooEngine, field: &mut Vec<String>) {
    let committed = e.pull_commit();
    field.extend(
        diacritics::words_of(&committed)
            .into_iter()
            .map(str::to_string),
    );
}

/// Types each word and the key after it, applying the changes after each.
fn type_into(e: &mut BambooEngine, field: &mut Vec<String>, words: &[(&str, char)]) {
    for &(keys, after) in words {
        type_keys(e, keys);
        e.process_key_event(after as u32, 0);
        commit_into(e, field);
        apply_changes(e.pull_changes(), field);
    }
}

fn engine_on(dictionary: &Arc<std::sync::Mutex<timsen::Dictionary>>) -> BambooEngine {
    let mut e = new_test_engine(&[], false);
    e.set_diacritics(Arc::clone(dictionary), learning_on());
    e
}

#[test]
fn word_ends_restore_the_run_through_timsen() {
    let mut e = engine_on(&shared_dictionary());
    let mut field = Vec::new();
    type_into(
        &mut e,
        &mut field,
        &[("toi", ' '), ("go", ' '), ("phim", ' ')],
    );
    assert_eq!(field, ["tôi", "gõ", "phím"]);
}

#[test]
fn a_syllable_nothing_knows_reaches_the_field_fixed() {
    let mut e = engine_on(&shared_dictionary());
    let mut field = Vec::new();
    type_into(
        &mut e,
        &mut field,
        &[("toi", ' '), ("go", ' '), ("phmi", ' ')],
    );
    assert_eq!(field, ["tôi", "gõ", "phím"]);
}

#[test]
fn a_word_one_slip_from_the_keys_of_another_is_fixed_to_it() {
    // A neighbouring key, two keys swapped, a key hit twice, and a stray key.
    for typo in ["mguoiwf", "gnuoiwf", "nguoiwff", "nguwowpif", "Mguoiwf"] {
        let mut e = engine_on(&shared_dictionary());
        let mut field = Vec::new();
        type_into(&mut e, &mut field, &[("ta", '.'), (typo, ' ')]);
        let fixed = if typo.starts_with('M') {
            "Người"
        } else {
            "người"
        };
        assert_eq!(field, ["ta", fixed], "{typo}");
    }
}

#[test]
fn a_word_with_no_word_near_its_keys_stays_as_typed() {
    let mut e = engine_on(&shared_dictionary());
    let mut field = Vec::new();
    type_into(
        &mut e,
        &mut field,
        &[("toi", ' '), ("go", ' '), ("xyzzy", ' '), ("commit", ' ')],
    );
    assert_eq!(field, ["tôi", "gõ", "xyzzy", "commit"]);
}

#[test]
fn the_text_of_a_macro_is_not_fixed_by_its_keys() {
    let mut e = engine_on(&shared_dictionary());
    e.macro_enabled = true;
    e.macro_table = macro_table(&[("mguoiwf", "xyzzy")]);
    let mut field = Vec::new();
    type_keys(&mut e, "mguoiwf");
    // Tab commits the text while the engine still holds the keys.
    e.process_key_event(KEY_TAB, 0);
    commit_into(&mut e, &mut field);
    apply_changes(e.pull_changes(), &mut field);
    assert_eq!(field, ["xyzzy"]);
}

#[test]
fn words_near_the_keys_come_with_the_cost_of_the_slip() {
    let e = new_test_engine(&[], false);
    let near = e.words_near("Mguoiwf");
    assert!(near.contains(&("người".to_string(), 1.0)), "{near:?}");
    let near = e.words_near("nguwowpif");
    assert!(near.contains(&("người".to_string(), 2.0)), "{near:?}");
    assert!(e.words_near("nguoi72").is_empty());
}

/// Backspace right after a commit, as the front end handles it: the field loses the text the
/// engine names, which holds the last word, and the engine shows the word as typed.
fn undo_with_backspace(e: &mut BambooEngine, field: &mut Vec<String>) -> bool {
    let Some(shown) = e.undo_fix() else {
        return false;
    };
    assert_eq!(diacritics::words_of(&shown), [field.pop().unwrap()]);
    true
}

/// Space after the reopened word, applying the changes.
fn commit_reopened(e: &mut BambooEngine, field: &mut Vec<String>) {
    e.process_key_event(KEY_SPACE, 0);
    commit_into(e, field);
    apply_changes(e.pull_changes(), field);
}

#[test]
fn backspace_right_after_a_fix_reopens_the_word_as_typed() {
    let dictionary = shared_dictionary();
    let mut e = engine_on(&dictionary);
    let mut field = Vec::new();
    type_into(
        &mut e,
        &mut field,
        &[("toi", ' '), ("go", ' '), ("phmi", ' ')],
    );
    assert_eq!(field, ["tôi", "gõ", "phím"]);
    assert!(undo_with_backspace(&mut e, &mut field));
    assert_eq!(e.get_preedit_string(), "phmi");
    commit_reopened(&mut e, &mut field);
    assert_eq!(
        field,
        ["tôi", "gõ", "phmi"],
        "committed again, it stays as typed"
    );
    type_into(&mut e, &mut field, &[("rat", '.')]);
    assert_eq!(field[2], "phmi");
    let mut again = Vec::new();
    type_into(
        &mut e,
        &mut again,
        &[("toi", ' '), ("go", ' '), ("phmi", ' ')],
    );
    assert_eq!(again[2], "phmi", "learned as typed, so not fixed again");
}

#[test]
fn backspace_after_the_punctuation_that_ended_the_run_reopens_the_word() {
    let dictionary = shared_dictionary();
    let mut e = engine_on(&dictionary);
    let mut field = Vec::new();
    type_into(
        &mut e,
        &mut field,
        &[("toi", ' '), ("go", ' '), ("phmi", '.')],
    );
    assert_eq!(field, ["tôi", "gõ", "phím"]);
    assert!(undo_with_backspace(&mut e, &mut field));
    commit_reopened(&mut e, &mut field);
    assert_eq!(field, ["tôi", "gõ", "phmi"]);
    assert_eq!(
        e.diacritics.as_ref().unwrap().run_len(),
        3,
        "the run goes on"
    );
    type_into(&mut e, &mut field, &[("rat", '.')]);
    let mut again = Vec::new();
    type_into(
        &mut e,
        &mut again,
        &[("toi", ' '), ("go", ' '), ("phmi", ' ')],
    );
    assert_eq!(again[2], "phmi");
}

#[test]
fn a_reopened_word_typed_again_differently_is_guessed_afresh() {
    let mut e = engine_on(&shared_dictionary());
    let mut field = Vec::new();
    type_into(
        &mut e,
        &mut field,
        &[("toi", ' '), ("go", ' '), ("phimm", ' ')],
    );
    assert!(undo_with_backspace(&mut e, &mut field));
    e.process_key_event(KEY_BACKSPACE, 0);
    commit_reopened(&mut e, &mut field);
    assert_eq!(
        field,
        ["tôi", "gõ", "phím"],
        "phim is no longer the word reopened, so it is guessed in turn"
    );
}

#[test]
fn only_the_key_right_after_the_fix_undoes_it() {
    let mut e = engine_on(&shared_dictionary());
    let mut field = Vec::new();
    assert!(e.undo_fix().is_none(), "nothing typed");
    type_into(&mut e, &mut field, &[("toi", ' '), ("go", ' ')]);
    assert!(e.undo_fix().is_none(), "nothing fixed");
    type_into(&mut e, &mut field, &[("phmi", ' '), ("rat", ' ')]);
    assert!(e.undo_fix().is_none(), "a word came after the fix");
    type_into(
        &mut e,
        &mut field,
        &[("toi", ' '), ("go", ' '), ("phmi", ' ')],
    );
    e.process_key_event(0xff50, 0); // Home, which the app moves the cursor with
    assert!(e.undo_fix().is_none(), "a key came after the fix");
    type_into(
        &mut e,
        &mut field,
        &[("toi", ' '), ("go", ' '), ("phmi", ' '), ("", ' ')],
    );
    assert!(e.undo_fix().is_none(), "a second space came after the fix");
    type_into(&mut e, &mut field, &[("toi", ' '), ("go", ' ')]);
    type_keys(&mut e, "phmi");
    e.process_key_event(0xff50, 0);
    commit_into(&mut e, &mut field);
    apply_changes(e.pull_changes(), &mut field);
    assert_eq!(field.last().map(String::as_str), Some("phím"));
    assert!(
        e.undo_fix().is_none(),
        "the key that fixed it went to the app"
    );
    type_into(&mut e, &mut field, &[("toi", ' '), ("go", ' ')]);
    type_keys(&mut e, "phmi");
    e.process_key_event(KEY_TAB, 0);
    commit_into(&mut e, &mut field);
    apply_changes(e.pull_changes(), &mut field);
    assert_eq!(field.last().map(String::as_str), Some("phím"));
    assert!(
        e.undo_fix().is_none(),
        "Tab, which the app gets, came after it"
    );
    type_into(
        &mut e,
        &mut field,
        &[("toi", ' '), ("go", ' '), ("phmi", ' ')],
    );
    type_keys(&mut e, "r");
    assert!(e.undo_fix().is_none(), "a letter came after the fix");
    e.reset();
    e.macro_enabled = true;
    type_into(
        &mut e,
        &mut field,
        &[("toi", ' '), ("go", ' '), ("phmi", ' '), ("", ' ')],
    );
    assert!(
        e.undo_fix().is_none(),
        "a second space, which macros let the engine take"
    );
    type_into(
        &mut e,
        &mut field,
        &[("toi", ' '), ("go", ' '), ("phmi", ' ')],
    );
    e.reset();
    assert!(e.undo_fix().is_none(), "the cursor moved");
    type_into(
        &mut e,
        &mut field,
        &[("toi", ' '), ("go", ' '), ("phmi", ' ')],
    );
    assert!(e.undo_fix().is_some());
    assert!(e.undo_fix().is_none(), "one fix, undone once");
}

#[test]
fn an_undo_names_the_text_to_delete() {
    for (after, text) in [(' ', "phím "), ('.', "phím.")] {
        let mut e = engine_on(&shared_dictionary());
        let mut field = Vec::new();
        type_into(
            &mut e,
            &mut field,
            &[("toi", ' '), ("go", ' '), ("phmi", after)],
        );
        assert_eq!(e.undo_fix().as_deref(), Some(text));
    }
    let mut e = engine_on(&shared_dictionary());
    e.macro_enabled = true;
    e.macro_table = macro_table(&[("tgp", "toi go phmi")]);
    let mut field = Vec::new();
    type_keys(&mut e, "tgp");
    e.process_key_event(KEY_TAB, 0);
    commit_into(&mut e, &mut field);
    apply_changes(e.pull_changes(), &mut field);
    assert_eq!(field, ["tôi", "gõ", "phím"]);
    assert_eq!(
        e.undo_fix().as_deref(),
        Some("phím"),
        "Tab adds nothing after it"
    );
}

#[test]
fn a_fix_not_applied_yet_is_not_undone() {
    let mut e = engine_on(&shared_dictionary());
    let mut field = Vec::new();
    type_into(&mut e, &mut field, &[("toi", ' '), ("go", ' ')]);
    type_keys(&mut e, "phmi");
    e.process_key_event(KEY_SPACE, 0);
    commit_into(&mut e, &mut field);
    assert!(e.undo_fix().is_none(), "the field still shows phmi");
    apply_changes(e.pull_changes(), &mut field);
    assert_eq!(
        field,
        ["tôi", "gõ", "phím"],
        "the fix is still there to apply"
    );
}

#[test]
fn an_undo_names_the_text_in_the_charset_of_the_field() {
    let mut e = engine_on(&shared_dictionary());
    e.output_charset = "VIQR".to_string();
    let mut field = Vec::new();
    type_into(
        &mut e,
        &mut field,
        &[("toi", ' '), ("go", ' '), ("phmi", ' ')],
    );
    let text = e.undo_fix().unwrap();
    assert_ne!(text, "phím ");
    assert_eq!(text, encode("VIQR", "phím "));
}

#[test]
fn a_word_taken_back_from_the_field_ends_the_run() {
    let mut e = engine_on(&shared_dictionary());
    let mut field = Vec::new();
    type_into(
        &mut e,
        &mut field,
        &[("toi", ' '), ("go", ' '), ("phmi", ' ')],
    );
    e.rebuild_from_text("ngo");
    assert!(e.undo_fix().is_none(), "the field changed under the fix");
    assert_eq!(e.diacritics.as_ref().unwrap().run_len(), 0);
}

#[test]
fn a_tone_removed_from_the_word_being_typed_keeps_the_run() {
    let mut e = engine_on(&shared_dictionary());
    let mut field = Vec::new();
    type_into(&mut e, &mut field, &[("toi", ' ')]);
    type_keys(&mut e, "gox");
    e.rebuild_from_text("go");
    assert_eq!(e.diacritics.as_ref().unwrap().run_len(), 1);
}

#[test]
fn backspace_the_app_gets_ends_the_run() {
    let mut e = engine_on(&shared_dictionary());
    // With macros on, the engine takes Backspace itself rather than passing it on unseen.
    e.macro_enabled = true;
    let mut field = Vec::new();
    type_into(&mut e, &mut field, &[("toi", ' '), ("go", ' ')]);
    assert!(
        !e.process_key_event(KEY_BACKSPACE, 0),
        "the app deletes the space"
    );
    assert_eq!(e.diacritics.as_ref().unwrap().run_len(), 0);
}

/// What the front end does with a key's commit and changes: the field must end with the old text,
/// which then makes way for the new.
fn commit_text_into(e: &mut BambooEngine, field: &mut String) {
    field.push_str(&e.pull_commit());
    if let Some((old, new)) = e.pull_field_change() {
        assert!(field.ends_with(&old), "{field:?} does not end with {old:?}");
        field.truncate(field.len() - old.len());
        field.push_str(&new);
    }
}

fn type_text(e: &mut BambooEngine, field: &mut String, words: &[(&str, char)]) {
    for &(keys, after) in words {
        type_keys(e, keys);
        e.process_key_event(after as u32, 0);
        commit_text_into(e, field);
    }
}

#[test]
fn changes_as_text_reach_the_words_pull_changes_reaches() {
    let typed: &[&[(&str, char)]] = &[
        &[("toi", ' '), ("go", ' '), ("phmi", '.')],
        &[("ban", ' '), ("thich", ' '), ("go", '.')],
        &[("toi", ' '), ("go", ' '), ("phim", ' ')],
    ];
    for &words in typed {
        let (mut e, mut twin) = (
            engine_on(&shared_dictionary()),
            engine_on(&shared_dictionary()),
        );
        let (mut text, mut field) = (String::new(), Vec::new());
        type_text(&mut e, &mut text, words);
        type_into(&mut twin, &mut field, words);
        assert_eq!(owned_words(&text), field, "{words:?}");
    }
}

#[test]
fn changes_as_text_are_in_the_charset_of_the_field() {
    let words = [("toi", ' '), ("go", ' '), ("phmi", '.')];
    let (mut e, mut twin) = (
        engine_on(&shared_dictionary()),
        engine_on(&shared_dictionary()),
    );
    e.output_charset = "VIQR".to_string();
    let (mut viqr, mut text) = (String::new(), String::new());
    type_text(&mut e, &mut viqr, &words);
    type_text(&mut twin, &mut text, &words);
    assert_eq!(viqr, encode("VIQR", &text));
    assert_ne!(viqr, text);
}

#[test]
fn a_change_as_text_keeps_what_lies_between_the_words() {
    let mut d = Diacritics::new(shared_dictionary(), learning_on());
    let mut field = "ban  thich go. ".to_string();
    d.committed(&field, "", true, &no_words_near);
    let (old, new) = d.pull_field_change().unwrap();
    assert!(field.ends_with(&old));
    field.truncate(field.len() - old.len());
    field.push_str(&new);
    assert_eq!(field, "bạn  thích gỗ. ");
}

#[test]
fn a_change_after_the_cap_still_finds_its_text() {
    let mut e = engine_on(&shared_dictionary());
    let mut field = String::new();
    type_text(&mut e, &mut field, &vec![("ta", ' '); timsen::MAX_RUN + 1]);
    type_text(
        &mut e,
        &mut field,
        &[("toi", ' '), ("go", ' '), ("phim", '.')],
    );
    assert!(field.ends_with("ta tôi gõ phím."), "{field:?}");
}

#[test]
fn guessed_words_follow_the_tone_placement_setting() {
    for (style, shown) in [
        (ToneStyle::Modern, "hòa bình"),
        (ToneStyle::Traditional, "hoà bình"),
    ] {
        let mut e = engine_on(&shared_dictionary());
        let config = Config::builder()
            .free_tone_marking(true)
            .tone_style(style)
            .auto_correct(false)
            .w2u_mode(W2uMode::Everywhere)
            .build();
        e.preeditor.set_config(config);
        let mut field = String::new();
        type_text(&mut e, &mut field, &[("hoa", ' '), ("binh", '.')]);
        assert!(field.starts_with(shown), "{style:?}: {field:?}");
    }
}

#[test]
fn only_the_word_that_ended_changes_without_the_whole_run() {
    for (whole, shown) in [(true, "bàn phím."), (false, "bạn phím.")] {
        let mut e = engine_on(&shared_dictionary());
        e.set_whole_run(whole);
        let mut field = String::new();
        type_text(&mut e, &mut field, &[("ban", ' '), ("phim", '.')]);
        assert_eq!(field, shown, "whole run {whole}");
    }
}

#[test]
fn removing_diacritics_learns_the_run_and_guesses_nothing_after() {
    let dictionary = shared_dictionary();
    let mut e = engine_on(&dictionary);
    let mut field = String::new();
    type_text(&mut e, &mut field, &[("ngos", ' '), ("sen", ' ')]);
    e.remove_diacritics();
    assert!(!dictionary.lock().unwrap().is_empty(), "the run ended");
    type_text(&mut e, &mut field, &[("toi", ' '), ("go", '.')]);
    assert_eq!(field, "ngó sen toi go.");
}

#[test]
fn the_first_fix_after_a_sentence_can_be_undone() {
    let mut e = engine_on(&shared_dictionary());
    let mut field = Vec::new();
    type_into(&mut e, &mut field, &[("ta", '.'), ("mguoifw", ' ')]);
    assert_eq!(field.last().map(String::as_str), Some("người"));
    assert!(e.undo_fix().is_some());
}

/// A slip short of the fixing margin when its word ends, then a word that settles it.
const SHORT_THEN_SETTLED: [(&str, char); 3] = [("ta", '.'), ("banfm", ' '), ("phim", ' ')];

#[test]
fn a_word_short_of_the_margin_is_fixed_once_the_next_word_ends() {
    for whole in [true, false] {
        let mut e = engine_on(&shared_dictionary());
        e.set_whole_run(whole);
        let mut field = Vec::new();
        type_into(&mut e, &mut field, &SHORT_THEN_SETTLED[..2]);
        assert_eq!(field, ["ta", "banfm"], "not when it ends");
        type_into(&mut e, &mut field, &SHORT_THEN_SETTLED[2..]);
        let fixed = if whole { "bàn" } else { "banfm" };
        assert_eq!(field, ["ta", fixed, "phím"], "whole run {whole}");
    }
}

#[test]
fn a_field_that_shows_its_text_from_the_second_word_gets_the_later_fix() {
    let mut e = engine_on(&shared_dictionary());
    let mut field = Vec::new();
    e.set_whole_run(false);
    type_into(&mut e, &mut field, &[("go", ' ')]);
    e.set_whole_run(true);
    type_into(&mut e, &mut field, &[("ggo", ' ')]);
    assert_eq!(field, ["gõ", "ggo"], "not when it ends");
    type_into(&mut e, &mut field, &[("phim", ' ')]);
    assert_eq!(field, ["gõ", "gõ", "phím"]);
}

/// Backspace after the fix a later word made: what the field shows from the fixed word on becomes
/// what was typed there.
fn undo_earlier_with_backspace(e: &mut BambooEngine, field: &mut Vec<String>) -> bool {
    let Some(old) = e.earlier_fix_shown() else {
        return false;
    };
    assert_eq!(e.earlier_fix_shown(), Some(old.clone()), "only looks");
    let new = e.undo_earlier_fix().unwrap();
    assert_eq!(e.earlier_fix_shown(), None);
    let (old, new) = (diacritics::words_of(&old), diacritics::words_of(&new));
    let from = field.len() - old.len();
    assert_eq!(field[from..], old);
    field.truncate(from);
    field.extend(new.into_iter().map(str::to_string));
    true
}

#[test]
fn backspace_right_after_a_later_fix_takes_it_back_for_good() {
    let dictionary = shared_dictionary();
    let mut e = engine_on(&dictionary);
    let mut field = Vec::new();
    type_into(&mut e, &mut field, &SHORT_THEN_SETTLED);
    assert_eq!(field, ["ta", "bàn", "phím"]);
    assert!(e.undo_fix().is_none(), "the last word was not fixed");
    assert!(undo_earlier_with_backspace(&mut e, &mut field));
    assert_eq!(field, ["ta", "banfm", "phím"]);
    assert!(!undo_earlier_with_backspace(&mut e, &mut field), "once");
    type_into(&mut e, &mut field, &[("nay", '.')]);
    assert_eq!(field, ["ta", "banfm", "phím", "này"]);
    // Learned as typed, so a later run leaves it alone.
    let mut later = engine_on(&dictionary);
    let mut field = Vec::new();
    type_into(&mut later, &mut field, &SHORT_THEN_SETTLED);
    assert_eq!(field, ["ta", "banfm", "phím"]);
}

#[test]
fn taking_a_later_fix_back_changes_that_word_alone() {
    let mut e = engine_on(&shared_dictionary());
    let mut field = Vec::new();
    let typed = [("ban", ' '), ("phmi", ' '), ("nay", ' ')];
    type_into(&mut e, &mut field, &typed);
    assert_eq!(field, ["bàn", "phím", "này"]);
    assert!(undo_earlier_with_backspace(&mut e, &mut field));
    // Without phím after it the run reads bạn again, but the field was checked for one word.
    assert_eq!(field, ["bàn", "phmi", "này"]);
}

#[test]
fn a_later_fix_is_taken_back_after_the_key_that_ended_its_run() {
    let dictionary = shared_dictionary();
    let mut e = engine_on(&dictionary);
    let mut field = Vec::new();
    type_into(&mut e, &mut field, &[("banfm", ' '), ("phim", '.')]);
    assert_eq!(field, ["bàn", "phím"]);
    assert_eq!(e.earlier_fix_shown(), Some("bàn phím.".to_string()));
    assert_eq!(e.undo_earlier_fix(), Some("banfm phím.".to_string()));
    let mut later = engine_on(&dictionary);
    let mut field = Vec::new();
    type_into(&mut later, &mut field, &[("banfm", ' '), ("phim", '.')]);
    assert_eq!(field, ["banfm", "phím"], "learned as typed");
}

#[test]
fn a_later_fix_is_taken_back_only_by_the_next_key() {
    let mut e = engine_on(&shared_dictionary());
    let mut field = Vec::new();
    type_into(&mut e, &mut field, &SHORT_THEN_SETTLED);
    type_into(&mut e, &mut field, &[("nay", ' ')]);
    assert_eq!(e.undo_earlier_fix(), None, "after another word");
    let mut e = engine_on(&shared_dictionary());
    type_into(&mut e, &mut Vec::new(), &SHORT_THEN_SETTLED);
    e.process_key_event(KEY_RETURN, 0);
    assert_eq!(e.undo_earlier_fix(), None, "after a key that ends the run");
    let mut e = engine_on(&shared_dictionary());
    type_into(&mut e, &mut Vec::new(), &SHORT_THEN_SETTLED);
    e.set_private(true);
    assert_eq!(e.undo_earlier_fix(), None, "in a private field");
    let mut e = engine_on(&shared_dictionary());
    type_into(&mut e, &mut Vec::new(), &SHORT_THEN_SETTLED[..2]);
    type_keys(&mut e, "phim ");
    assert_eq!(e.undo_earlier_fix(), None, "before the field shows the fix");
    let mut e = engine_on(&shared_dictionary());
    let mut field = Vec::new();
    let fixed_at_its_end = [("toi", ' '), ("go", ' '), ("phmi", ' '), ("nay", ' ')];
    type_into(&mut e, &mut field, &fixed_at_its_end);
    assert_eq!(field[2], "phím");
    assert_eq!(e.undo_earlier_fix(), None, "a fix made when its word ended");
}

#[test]
fn a_charset_change_ends_the_run_kept_in_the_old_one() {
    let mut e = engine_on(&shared_dictionary());
    e.output_charset = "VIQR".to_string();
    let mut field = String::new();
    type_text(&mut e, &mut field, &[("ban", ' ')]);
    e.set_output_charset("Unicode".to_string());
    type_text(&mut e, &mut field, &[("phim", '.')]);
    assert_eq!(field, format!("{}phím.", encode("VIQR", "bạn ")));
}

#[test]
fn changes_as_text_hold_a_tab_a_macro_committed() {
    let mut e = engine_on(&shared_dictionary());
    e.diacritics
        .as_mut()
        .unwrap()
        .committed("toi\tgo. ", "", true, &no_words_near);
    let text = e.pull_changes_text();
    let (length, rest) = text.split_once('\t').unwrap();
    let (old, new) = rest.split_at(length.parse().unwrap());
    assert_eq!((old, new), ("toi\tgo. ", "tôi\tgỗ. "));
}

#[test]
fn ending_a_word_keeps_the_run_that_a_reset_ends() {
    for end_word in [true, false] {
        let mut e = engine_on(&shared_dictionary());
        let mut field = String::new();
        for (keys, after) in [("toi", ' '), ("go", ' '), ("phmi", ' ')] {
            type_text(&mut e, &mut field, &[(keys, after)]);
            if end_word {
                e.end_word();
            } else {
                e.reset();
            }
        }
        let run = e.diacritics.as_ref().unwrap().run_len();
        assert_eq!(run, if end_word { 3 } else { 0 });
        assert_eq!(e.undo_fix().is_some(), end_word);
    }
}

#[test]
fn a_private_field_undoes_nothing() {
    let mut e = engine_on(&shared_dictionary());
    let mut field = Vec::new();
    type_into(
        &mut e,
        &mut field,
        &[("toi", ' '), ("go", ' '), ("phmi", ' ')],
    );
    e.set_private(true);
    assert!(e.undo_fix().is_none());
}

#[test]
fn fixing_switched_off_leaves_the_typo_from_the_next_run() {
    let mut e = engine_on(&shared_dictionary());
    let mut field = Vec::new();
    type_into(&mut e, &mut field, &[("toi", ' '), ("go", ' ')]);
    e.set_fixing(None);
    type_into(&mut e, &mut field, &[("phmi", '.')]);
    assert_eq!(field[2], "phím", "the run already open still fixes");
    type_into(
        &mut e,
        &mut field,
        &[("toi", ' '), ("go", ' '), ("phmi", ' ')],
    );
    assert_eq!(field[5], "phmi");
}

#[test]
fn each_fixing_margin_fixes_only_what_wins_by_it() {
    // banfm wins by between 3 and 4 at the start of a run, ggo by between 2 and 3 after tôi gõ.
    let typos: [(&[(&str, char)], &str); 2] =
        [(&[], "banfm"), (&[("toi", ' '), ("go", ' ')], "ggo")];
    for (margin, fixed) in [
        (Some(4.0), ["banfm", "ggo"]),
        (Some(3.0), ["bạn", "ggo"]),
        (Some(2.0), ["bạn", "gõ"]),
        (None, ["banfm", "ggo"]),
    ] {
        for ((before, typo), fixed) in typos.into_iter().zip(fixed) {
            let mut e = engine_on(&shared_dictionary());
            e.set_fixing(margin);
            let mut field = Vec::new();
            type_into(&mut e, &mut field, before);
            type_into(&mut e, &mut field, &[(typo, '.')]);
            assert_eq!(
                field.last().map(String::as_str),
                Some(fixed),
                "{typo} at {margin:?}"
            );
        }
    }
}

#[test]
fn fixing_switched_off_holds_past_the_cap() {
    for on in [true, false] {
        let mut e = engine_on(&shared_dictionary());
        let mut field = Vec::new();
        type_into(&mut e, &mut field, &vec![("ta", ' '); timsen::MAX_RUN]);
        e.set_fixing(on.then_some(timsen::FIX_MARGIN));
        type_into(
            &mut e,
            &mut field,
            &[("toi", ' '), ("go", ' '), ("phmi", ' ')],
        );
        let fixed = if on { "phím" } else { "phmi" };
        assert_eq!(
            field[timsen::MAX_RUN + 2],
            fixed,
            "the run after the cap, fixing {on}"
        );
    }
}

#[test]
fn changes_pulled_late_still_find_their_words() {
    let mut e = engine_on(&shared_dictionary());
    let mut field = Vec::new();
    for keys in ["toi", "go", "phim"] {
        type_keys(&mut e, keys);
        e.process_key_event(KEY_SPACE, 0);
        commit_into(&mut e, &mut field);
    }
    apply_changes(e.pull_changes(), &mut field);
    assert_eq!(field, ["tôi", "gõ", "phím"]);
}

#[test]
fn punctuation_ends_the_run_and_learns_only_words_typed_with_marks() {
    let dictionary = shared_dictionary();
    let mut e = engine_on(&dictionary);
    let mut field = Vec::new();
    type_into(
        &mut e,
        &mut field,
        &[("toi", ' '), ("go", ' '), ("phim", '.')],
    );
    assert_eq!(field, ["tôi", "gõ", "phím"]);
    assert!(
        dictionary.lock().unwrap().is_empty(),
        "guessed words are not learned"
    );
    type_into(&mut e, &mut field, &[("ngos", ' '), ("sen", ' ')]);
    assert!(
        dictionary.lock().unwrap().is_empty(),
        "nothing learned before the run ends"
    );
    type_into(&mut e, &mut field, &[("toi", '.')]);
    assert!(!dictionary.lock().unwrap().is_empty());
    assert_eq!(e.diacritics.as_ref().unwrap().run_len(), 0);
}

#[test]
fn enter_and_a_forwarded_symbol_end_the_run() {
    let dictionary = shared_dictionary();
    let mut e = engine_on(&dictionary);
    type_into(&mut e, &mut Vec::new(), &[("ngos", ' ')]);
    type_keys(&mut e, "sen");
    e.process_key_event(KEY_RETURN, 0);
    assert_eq!(e.diacritics.as_ref().unwrap().run_len(), 0);
    assert!(!dictionary.lock().unwrap().is_empty());
    let dictionary = shared_dictionary();
    let mut e = engine_on(&dictionary);
    type_into(&mut e, &mut Vec::new(), &[("ngos", ' '), ("sen", ' ')]);
    e.process_key_event('.' as u32, 0);
    assert_eq!(e.diacritics.as_ref().unwrap().run_len(), 0);
    assert!(!dictionary.lock().unwrap().is_empty());
}

#[test]
fn a_reset_ends_the_run_without_changing_the_field() {
    let mut ended = engine_on(&shared_dictionary());
    let mut field = Vec::new();
    type_into(
        &mut ended,
        &mut field,
        &[("ban", ' '), ("thich", ' '), ("go", '.')],
    );
    let mut e = engine_on(&shared_dictionary());
    let mut open = Vec::new();
    type_into(
        &mut e,
        &mut open,
        &[("ban", ' '), ("thich", ' '), ("go", ' ')],
    );
    assert_ne!(field, open, "ending the run would change the field");
    e.reset();
    assert!(e.pull_changes().is_empty());
    assert_eq!(e.diacritics.as_ref().unwrap().run_len(), 0);
}

#[test]
fn a_private_field_never_reaches_timsen() {
    let dictionary = shared_dictionary();
    let mut e = engine_on(&dictionary);
    let mut field = Vec::new();
    type_into(&mut e, &mut field, &[("ngos", ' '), ("sen", ' ')]);
    e.set_private(true);
    type_into(
        &mut e,
        &mut field,
        &[("toi", ' '), ("go", ' '), ("phim", '.')],
    );
    assert_eq!(field, ["ngó", "sen", "toi", "go", "phim"]);
    assert!(
        dictionary.lock().unwrap().is_empty(),
        "what waited was dropped"
    );
    e.set_private(false);
    type_into(&mut e, &mut field, &[("go", ' ')]);
    assert_eq!(field[5], "go", "a field once private stays off");
}

#[test]
fn a_word_half_typed_in_a_private_field_is_never_learned() {
    let dictionary = shared_dictionary();
    let mut e = engine_on(&dictionary);
    type_keys(&mut e, "ngos");
    e.set_private(true);
    e.set_private(false);
    type_into(&mut e, &mut Vec::new(), &[("", ' '), ("sen", '.')]);
    assert!(dictionary.lock().unwrap().is_empty());
}

#[test]
fn what_one_field_learns_the_next_run_in_another_restores() {
    let dictionary = shared_dictionary();
    let (mut first, mut second) = (engine_on(&dictionary), engine_on(&dictionary));
    let mut field = Vec::new();
    type_into(&mut second, &mut field, &[("ngo", ' '), ("sen", '.')]);
    // The table lacks ngo, so it stays as typed until the dictionary learns ngó.
    assert_eq!(field, ["ngo", "sen"], "the table alone does not restore it");
    type_into(&mut first, &mut Vec::new(), &[("ngos", ' '), ("sen", '.')]);
    field.clear();
    type_into(&mut second, &mut field, &[("ngo", ' '), ("sen", '.')]);
    assert_eq!(field, ["ngó", "sen"]);
}

fn engine_learning(
    dictionary: &Arc<std::sync::Mutex<timsen::Dictionary>>,
    learning: &Arc<Learning>,
) -> BambooEngine {
    let mut e = new_test_engine(&[], false);
    e.set_diacritics(Arc::clone(dictionary), Arc::clone(learning));
    e
}

#[test]
fn nothing_is_learned_while_learning_is_off() {
    let dictionary = shared_dictionary();
    let mut e = engine_learning(&dictionary, &Arc::new(Learning::new(false)));
    type_into(&mut e, &mut Vec::new(), &[("ngos", ' '), ("sen", '.')]);
    e.reset();
    assert!(dictionary.lock().unwrap().is_empty());
}

#[test]
fn a_run_open_when_learning_is_turned_off_is_never_learned() {
    for back_on in [false, true] {
        let dictionary = shared_dictionary();
        let learning = Arc::new(Learning::new(true));
        let mut e = engine_learning(&dictionary, &learning);
        type_into(&mut e, &mut Vec::new(), &[("ngos", ' ')]);
        learning.set(false);
        if back_on {
            learning.set(true);
        }
        type_into(&mut e, &mut Vec::new(), &[("sen", '.')]);
        assert!(dictionary.lock().unwrap().is_empty(), "back on: {back_on}");
        // The next run starts after the change and learns again.
        learning.set(true);
        type_into(&mut e, &mut Vec::new(), &[("ngos", ' '), ("sen", '.')]);
        assert!(!dictionary.lock().unwrap().is_empty(), "back on: {back_on}");
    }
}

#[test]
fn a_full_run_typed_while_learning_was_off_is_never_learned() {
    let dictionary = shared_dictionary();
    let learning = Arc::new(Learning::new(true));
    let mut e = engine_learning(&dictionary, &learning);
    type_into(&mut e, &mut Vec::new(), &[("ngos", ' ')]);
    learning.set(false);
    // Past the cap the run goes on as a new one, started while learning was off.
    let words = vec![("ngos", ' '); timsen::MAX_RUN + 5];
    type_into(&mut e, &mut Vec::new(), &words);
    learning.set(true);
    type_into(&mut e, &mut Vec::new(), &[("sen", '.')]);
    assert!(dictionary.lock().unwrap().is_empty());
}

#[test]
fn a_macro_is_never_learned_whichever_key_expands_it() {
    for key in [KEY_SPACE, '.' as u32, KEY_TAB, KEY_RETURN] {
        let dictionary = shared_dictionary();
        let mut e = engine_on(&dictionary);
        e.macro_enabled = true;
        e.macro_table = macro_table(&[("nsen", "ngó sen")]);
        type_keys(&mut e, "nsen");
        e.process_key_event(key, 0);
        assert!(e.pull_commit().starts_with("ngó sen"), "{key}");
        e.reset();
        assert!(dictionary.lock().unwrap().is_empty(), "{key}");
    }
}

#[test]
fn a_chat_abbreviation_is_never_learned() {
    for key in [KEY_SPACE, '.' as u32, KEY_RETURN] {
        let dictionary = shared_dictionary();
        let mut e = engine_on(&dictionary);
        e.shorthand = macro_table(&[("nsen", "ngó sen")]);
        type_keys(&mut e, "nsen");
        e.process_key_event(key, 0);
        assert!(e.pull_commit().starts_with("ngó sen"), "{key}");
        e.reset();
        assert!(dictionary.lock().unwrap().is_empty(), "{key}");
    }
}

/// Commits `keys` ended by `end`, then does what the front end does on Backspace right after: the
/// app deletes the last character, and the word before the cursor, `taken`, is taken back to type
/// on. It is ended by '.'.
fn take_back_and_end(e: &mut BambooEngine, keys: &str, end: u32, taken: &str) {
    type_keys(e, keys);
    e.process_key_event(end, 0);
    e.pull_commit();
    assert!(!e.process_key_event(KEY_BACKSPACE, 0));
    e.reopen_committed(taken);
    e.process_key_event('.' as u32, 0);
    e.reset();
}

#[test]
fn a_macro_word_taken_back_by_backspace_is_never_learned() {
    // A Space or Tab after the word is deleted; with none, its last letter is.
    for (expansion, end, taken) in [
        ("ngó", KEY_SPACE, "ngó"),
        ("ngó1", KEY_SPACE, "ngó1"),
        ("ngón", KEY_TAB, "ngó"),
    ] {
        let dictionary = shared_dictionary();
        let mut e = engine_on(&dictionary);
        e.macro_enabled = true;
        e.macro_table = macro_table(&[("nsen", expansion)]);
        take_back_and_end(&mut e, "nsen", end, taken);
        assert!(dictionary.lock().unwrap().is_empty(), "{expansion}");
    }
}

#[test]
fn a_letter_taken_back_after_a_macro_is_still_learned() {
    let dictionary = shared_dictionary();
    let mut e = engine_on(&dictionary);
    e.macro_enabled = true;
    e.macro_table = macro_table(&[("nsen", "ngó thư")]);
    type_keys(&mut e, "nsen");
    e.process_key_event(KEY_SPACE, 0);
    e.pull_commit();
    // A new cell shows the letter whose key went to the app before the field.
    e.reset();
    e.rebuild_from_text("n");
    type_keys(&mut e, "gos");
    e.process_key_event('.' as u32, 0);
    e.reset();
    assert!(!dictionary.lock().unwrap().is_empty());
}

/// A macro's word fixed and the fix undone, as the engine does when the key after it is Backspace.
fn undone_macro_word(dictionary: &Arc<std::sync::Mutex<timsen::Dictionary>>) -> Diacritics {
    let mut d = Diacritics::new(Arc::clone(dictionary), learning_on());
    let mut field = Vec::new();
    d.committed("toi ", "toi ", true, &no_words_near);
    field.push("toi".to_string());
    d.committed("ngo ", "", false, &no_words_near);
    field.push("ngo".to_string());
    apply_changes(d.pull_changes(), &mut field);
    assert_eq!(field, ["tôi", "gõ"], "the macro's word is fixed");
    assert!(d.undo_fix().is_some());
    d
}

#[test]
fn an_undone_macro_word_is_never_learned() {
    let dictionary = shared_dictionary();
    let mut d = undone_macro_word(&dictionary);
    d.committed("ngo.", "ngo.", true, &no_words_near);
    assert!(dictionary.lock().unwrap().is_empty());
}

#[test]
fn a_word_typed_after_an_undone_macro_word_is_learned() {
    let dictionary = shared_dictionary();
    let mut d = undone_macro_word(&dictionary);
    // The reopened word is erased and another typed in its place.
    d.committed("ngó.", "ngos.", true, &no_words_near);
    assert!(!dictionary.lock().unwrap().is_empty());
}

#[test]
fn words_typed_beside_a_macro_are_still_learned() {
    let dictionary = shared_dictionary();
    let mut e = engine_on(&dictionary);
    e.macro_enabled = true;
    e.macro_table = macro_table(&[("nsen", "ngó sen")]);
    type_into(&mut e, &mut Vec::new(), &[("nsen", ' '), ("ngos", '.')]);
    assert!(!dictionary.lock().unwrap().is_empty());
}

/// Types `rest` into `field`, then `last` ended by the key `end`.
fn type_ending(
    e: &mut BambooEngine,
    field: &mut Vec<String>,
    rest: &[(&str, char)],
    last: &str,
    end: u32,
) {
    type_into(e, field, rest);
    type_keys(e, last);
    e.process_key_event(end, 0);
    commit_into(e, field);
    apply_changes(e.pull_changes(), field);
}

/// A field whose run opened with `opening`, while the dictionary held ngó sen, goes on after
/// everything learned is forgotten. Returns the field and whether anything was learned.
fn typed_across_a_forget(
    opening: &[(&str, char)],
    go_on: impl FnOnce(&mut BambooEngine, &mut Vec<String>),
) -> (Vec<String>, bool) {
    let dictionary = shared_dictionary();
    let learning = learning_on();
    let mut teacher = engine_learning(&dictionary, &learning);
    type_into(
        &mut teacher,
        &mut Vec::new(),
        &[("ngos", ' '), ("sen", '.')],
    );
    let mut e = engine_learning(&dictionary, &learning);
    let mut field = Vec::new();
    type_into(&mut e, &mut field, opening);
    dictionary.lock().unwrap().clear();
    learning.forgot();
    go_on(&mut e, &mut field);
    e.reset();
    let learned = !dictionary.lock().unwrap().is_empty();
    (field, learned)
}

#[test]
fn a_run_open_across_a_forget_stops_guessing_with_what_was_forgotten() {
    for end in ['.' as u32, KEY_TAB] {
        let mut never_learned = Vec::new();
        let mut e = engine_on(&shared_dictionary());
        type_ending(
            &mut e,
            &mut never_learned,
            &[("ta", ' '), ("ngo", ' ')],
            "sen",
            end,
        );
        let (field, _) = typed_across_a_forget(&[("ta", ' ')], |e, field| {
            type_ending(e, field, &[("ngo", ' ')], "sen", end);
        });
        assert_eq!(field, never_learned, "{end}");
    }
}

#[test]
fn a_run_open_across_a_forget_learns_nothing() {
    let (_, learned) = typed_across_a_forget(&[("ngos", ' ')], |e, field| {
        type_into(e, field, &[("sen", '.')]);
    });
    assert!(!learned, "typing on");
    // Turning Đoán dấu off ends the run with nothing committed first.
    let (_, learned) = typed_across_a_forget(&[("ngos", ' ')], |e, _| e.remove_diacritics());
    assert!(!learned, "turned off");
}

#[test]
fn a_run_goes_on_past_the_cap() {
    let mut e = engine_on(&shared_dictionary());
    let mut field = Vec::new();
    let words = vec![("ta", ' '); timsen::MAX_RUN + 1];
    type_into(&mut e, &mut field, &words);
    assert_eq!(field.len(), timsen::MAX_RUN + 1);
    assert_eq!(e.diacritics.as_ref().unwrap().run_len(), 1);
}

#[test]
fn a_commit_across_the_cap_restores_both_runs() {
    let mut d = Diacritics::new(shared_dictionary(), learning_on());
    let mut field = Vec::new();
    for _ in 0..timsen::MAX_RUN - 1 {
        d.committed("ta ", "ta ", true, &no_words_near);
        field.push("ta".to_string());
    }
    // A macro can commit several words at once; the first fills the run, the second starts the next.
    d.committed("toi go ", "toi go ", true, &no_words_near);
    field.extend(["toi".to_string(), "go".to_string()]);
    apply_changes(d.pull_changes(), &mut field);
    assert_eq!(field[timsen::MAX_RUN - 1..], ["tôi", "gõ"]);
}

#[test]
fn digits_split_words_and_end_the_run() {
    let mut e = engine_on(&shared_dictionary());
    let mut field = Vec::new();
    type_into(&mut e, &mut field, &[("toi1", ' ')]);
    assert_eq!(field, ["tôi"]);
    assert_eq!(e.diacritics.as_ref().unwrap().run_len(), 0);
}

fn owned_words(text: &str) -> Vec<String> {
    diacritics::words_of(text)
        .into_iter()
        .map(str::to_string)
        .collect()
}

#[test]
fn a_commit_of_two_sentences_ends_the_run_between_them() {
    let mut whole = Diacritics::new(shared_dictionary(), learning_on());
    let mut parts = Diacritics::new(shared_dictionary(), learning_on());
    let (mut one, mut two) = (Vec::new(), Vec::new());
    whole.committed("ban thich go. phim ", "", true, &no_words_near);
    one.extend(owned_words("ban thich go. phim "));
    apply_changes(whole.pull_changes(), &mut one);
    for text in ["ban thich go.", " phim "] {
        parts.committed(text, "", true, &no_words_near);
        two.extend(owned_words(text));
        apply_changes(parts.pull_changes(), &mut two);
    }
    assert_eq!(one, two);
    assert_eq!(one[2], "gỗ");
}

#[test]
fn a_private_word_reopened_after_its_commit_is_never_learned() {
    let dictionary = shared_dictionary();
    let mut e = engine_on(&dictionary);
    e.set_private(true);
    type_keys(&mut e, "ngos");
    e.set_private(false);
    e.process_key_event(KEY_SPACE, 0);
    e.rebuild_from_text("ngó");
    e.process_key_event(KEY_SPACE, 0);
    type_into(&mut e, &mut Vec::new(), &[("sen", '.')]);
    assert!(dictionary.lock().unwrap().is_empty());
}

#[test]
fn a_digit_inside_a_commit_ends_the_run_there() {
    let mut d = Diacritics::new(shared_dictionary(), learning_on());
    let mut field = Vec::new();
    // A macro can commit a digit inside the text; the run ends at it, as at a period.
    d.committed("ban thich go1 ", "", true, &no_words_near);
    field.extend(owned_words("ban thich go1 "));
    apply_changes(d.pull_changes(), &mut field);
    assert_eq!(field, ["bạn", "thích", "gỗ"]);
}

#[test]
fn a_private_word_rebuilt_by_backspace_is_still_never_learned() {
    let dictionary = shared_dictionary();
    let mut e = new_test_engine(&["tôi"], true);
    e.keep_guard = Arc::new(["zzz".to_string()].into_iter().collect());
    e.set_diacritics(Arc::clone(&dictionary), learning_on());
    e.set_private(true);
    e.rebuild_from_text("tôi");
    e.set_private(false);
    type_keys(&mut e, "q");
    e.process_key_event(KEY_BACKSPACE, 0);
    e.process_key_event('.' as u32, 0);
    assert!(
        e.pull_commit().contains("tôi"),
        "the word is committed after all"
    );
    assert!(dictionary.lock().unwrap().is_empty());
}

#[test]
fn restoring_text_reads_each_stretch_between_punctuation_as_one_run() {
    let runs = std::cell::RefCell::new(Vec::new());
    let out = diacritics::restore_runs("Toi, go  phim! 12 ab", |words| {
        runs.borrow_mut().push(words.join(" "));
        words.iter().map(|w| w.to_uppercase()).collect()
    });
    assert_eq!(
        out, "TOI, GO  PHIM! 12 AB",
        "what lies between the words stays"
    );
    assert_eq!(runs.into_inner(), ["Toi", "go phim", "ab"]);
}

#[test]
fn text_typed_without_marks_gets_them_for_cuu_chu() {
    let mut e = engine_on(&shared_dictionary());
    assert_eq!(
        e.restore_text("toi go phim").as_deref(),
        Some("tôi gõ phím")
    );
    assert_eq!(e.restore_text("ban phim.").as_deref(), Some("bàn phím."));
    assert_eq!(e.restore_text("tôi"), None, "nothing to change");
    e.set_private(true);
    assert_eq!(
        e.restore_text("toi go phim"),
        None,
        "never in a private field"
    );
}

#[test]
fn restoring_text_needs_diacritics() {
    let e = new_test_engine(&[], false);
    assert_eq!(e.restore_text("toi go phim"), None);
}
