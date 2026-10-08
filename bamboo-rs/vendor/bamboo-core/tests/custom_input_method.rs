use bamboo_core::{Config, Engine, InputMethod, Mode};

const TELEX_DEF: [(&str, &str); 11] = [
    ("z", "XoaDauThanh"),
    ("s", "DauSac"),
    ("f", "DauHuyen"),
    ("r", "DauHoi"),
    ("x", "DauNga"),
    ("j", "DauNang"),
    ("a", "A_Â"),
    ("e", "E_Ê"),
    ("o", "O_Ô"),
    ("w", "UOAÔ_ƯƠĂƠ"),
    ("d", "D_Đ"),
];

const WORDS: [&str; 12] = [
    "as",
    "aa",
    "aw",
    "dd",
    "vieetj",
    "nguwowif",
    "dduwowngf",
    "thuowr",
    "hoaf",
    "tooi",
    "khoong",
    "quaas",
];

fn type_word(im: InputMethod, keys: &str) -> String {
    let mut engine = Engine::new(im);
    engine.process_str(keys, Mode::Vietnamese);
    engine.output().into_owned()
}

#[test]
fn single_tone_key_from_custom_keymap() {
    let im = InputMethod::from_definition("Custom", [("q", "DauSac")]);
    assert_eq!(im.name(), "Custom");
    assert_eq!(type_word(im.clone(), "aq"), "á");
    assert_eq!(type_word(im, "as"), "as");
}

#[test]
fn owned_strings_are_accepted() {
    let pairs = vec![("q".to_string(), "DauSac".to_string()), ("d".to_string(), "D_Đ".to_string())];
    let im = InputMethod::from_definition("Custom", pairs);
    assert_eq!(type_word(im.clone(), "ddaq"), "đá");
}

#[test]
fn mark_and_appending_rules_from_custom_keymap() {
    let im = InputMethod::from_definition(
        "Custom",
        [("q", "DauSac"), ("a", "A_Â"), ("w", "UOA_ƯƠĂ__Ư"), ("d", "D_Đ")],
    );
    assert_eq!(type_word(im.clone(), "aaq"), "ấ");
    assert_eq!(type_word(im.clone(), "w"), "ư");
    assert_eq!(type_word(im.clone(), "ddi"), "đi");
    assert_eq!(type_word(im, "tuwowiq"), "tưới");
}

#[test]
fn custom_definition_equal_to_telex_types_like_telex() {
    let custom = InputMethod::from_definition("Custom", TELEX_DEF);
    for word in WORDS {
        assert_eq!(
            type_word(custom.clone(), word),
            type_word(InputMethod::telex(), word),
            "{word}"
        );
    }
}

#[test]
fn custom_definition_named_like_a_preset_keeps_its_own_rules() {
    // Same rule count as Telex, but the acute accent moved from `s` to `q`.
    let def = TELEX_DEF.map(|(k, v)| if k == "s" { ("q", v) } else { (k, v) });
    let im = InputMethod::from_definition("Telex", def);
    assert_eq!(im.rules().len(), InputMethod::telex().rules().len());

    let mut engine = Engine::with_config(im, Config::default());
    engine.process_str("aq", Mode::Vietnamese);
    assert_eq!(engine.output(), "á");
    assert!(engine.input_method().keys().contains(&'q'));
}

#[test]
fn keys_follow_entry_order() {
    let im =
        InputMethod::from_definition("Custom", [("q", "DauSac"), ("", "DauHuyen"), ("d", "D_Đ")]);
    assert_eq!(im.keys(), ['q', 'd']);
    assert_eq!(im.tone_keys(), ['q']);
}
