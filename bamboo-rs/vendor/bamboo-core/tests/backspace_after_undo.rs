use bamboo_core::{Config, Engine, InputMethod, Mode};

fn typed(keys: &str) -> String {
    let config = Config::builder().auto_correct(false).build();
    let mut engine = Engine::with_config(InputMethod::telex(), config);
    for key in keys.chars() {
        if key == '<' {
            engine.remove_last_output_char();
        } else {
            engine.process_key(key, Mode::Vietnamese);
        }
    }
    engine.output().into_owned()
}

#[test]
fn deleting_back_to_a_valid_word_types_vietnamese_again() {
    assert_eq!(typed("eete<"), "et");
    assert_eq!(typed("eete<e"), "êt");
    assert_eq!(typed("teete<e"), "têt");
}

#[test]
fn invalid_word_stays_raw_after_delete() {
    assert_eq!(typed("eetex<e"), "etee");
}
