// SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

use super::tests::{new_test_engine, type_keys};
use super::*;
use bamboo_core::BracketMode;

fn new_opt_engine(config: Config) -> BambooEngine {
    let mut e = new_test_engine(&[], false);
    e.preeditor.set_config(config);
    e
}

fn with_w2u(mode: W2uMode) -> Config {
    Config {
        w2u_mode: mode,
        ..default_config()
    }
}

#[test]
fn test_w2u_modes() {
    let cases = [
        ("default", default_config(), "w", "ư"),
        ("default-bw", default_config(), "bw", "bư"),
        // The Go core had a w2u flag besides the mode; both map to the one crate setting.
        ("flag-off", with_w2u(W2uMode::Disabled), "w", "w"),
        ("flag-off-bw", with_w2u(W2uMode::Disabled), "bw", "bw"),
        ("disabled", with_w2u(W2uMode::Disabled), "w", "w"),
        ("nonstart", with_w2u(W2uMode::NonStart), "w", "w"),
        ("nonstart-bw", with_w2u(W2uMode::NonStart), "bw", "bư"),
        ("everywhere", with_w2u(W2uMode::Everywhere), "w", "ư"),
    ];
    for (name, config, keys, want) in cases {
        let mut e = new_opt_engine(config);
        type_keys(&mut e, keys);
        assert_eq!(e.preedit_text, want, "w2u {name}: type {keys}");
    }
    // aw -> ă wins over w -> ư.
    let mut e = new_opt_engine(default_config());
    type_keys(&mut e, "aw");
    assert_eq!(e.preedit_text, "ă");
}

#[test]
fn test_option_matrix_auto_restore_spell_check() {
    let matrix = [
        (true, true, "chào ", "boawjm "),
        (true, false, "chào ", "boặm "),
        (false, true, "chào ", "boặm "),
        (false, false, "chào ", "boặm "),
    ];
    for (auto_restore, with_dicts, want_chaof, want_boawjm) in matrix {
        for (keys, want) in [("chaof", want_chaof), ("boawjm", want_boawjm)] {
            let mut e = new_test_engine(&["chào"], with_dicts);
            e.auto_non_vn_restore = auto_restore;
            type_keys(&mut e, keys);
            e.process_key_event(KEY_SPACE, 0);
            assert_eq!(
                e.commit_text, want,
                "auto={auto_restore} dict={with_dicts}: {keys}"
            );
        }
    }
}

#[test]
fn test_spell_check_with_dicts_direct() {
    let mut e = new_test_engine(&["chào"], true);
    type_keys(&mut e, "chaof");
    assert!(!e.must_fallback_to_english(), "dict hit");
    let mut e = new_test_engine(&["chào"], true);
    type_keys(&mut e, "boawjm");
    assert!(e.must_fallback_to_english(), "dict miss");
    let mut e = new_test_engine(&[], true);
    type_keys(&mut e, "chaof");
    assert!(e.must_fallback_to_english(), "empty dict");
}

#[test]
fn test_auto_non_vn_restore_direct() {
    let mut e = new_test_engine(&[], true);
    e.auto_non_vn_restore = false;
    type_keys(&mut e, "boawjm");
    assert!(!e.must_fallback_to_english(), "auto restore off");
    let mut e = new_test_engine(&[], true);
    type_keys(&mut e, "boawjm");
    assert!(e.must_fallback_to_english(), "auto restore on");
}

#[test]
fn test_modern_style() {
    let mut e = new_opt_engine(default_config());
    type_keys(&mut e, "hoaf");
    assert_eq!(e.preedit_text, "hòa", "standard tone style");
    let mut e = new_opt_engine(Config {
        std_tone_style: false,
        ..default_config()
    });
    type_keys(&mut e, "hoaf");
    assert_eq!(e.preedit_text, "hoà", "modern tone style");
}

#[test]
fn test_bracket_transform() {
    for (keys, want) in [("[", "ơ"), ("t[", "tơ"), ("]", "ư"), ("t]", "tư")] {
        let mut e = new_opt_engine(Config {
            bracket_mode: BracketMode::Everywhere,
            ..default_config()
        });
        type_keys(&mut e, keys);
        assert_eq!(e.preedit_text, want, "bracket: type {keys}");
    }
}

#[test]
fn test_free_marking_flag() {
    let e = new_test_engine(&[], false);
    assert!(e.preeditor.config().free_tone_marking, "default flags");
    let mut e = new_opt_engine(Config {
        free_tone_marking: false,
        ..default_config()
    });
    assert!(!e.preeditor.config().free_tone_marking, "cleared");
    e.preeditor.set_config(default_config());
    assert!(e.preeditor.config().free_tone_marking, "set again");
}
