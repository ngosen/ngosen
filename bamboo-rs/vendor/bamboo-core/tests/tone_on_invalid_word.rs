// Expected outputs come from Go bamboo-core with the same keys and settings.
use bamboo_core::{Config, Engine, InputMethod, Mode};

fn typed(im: InputMethod, free_tone: bool, keys: &str) -> String {
    let config = Config::builder().free_tone_marking(free_tone).auto_correct(false).build();
    let mut engine = Engine::with_config(im, config);
    for key in keys.chars() {
        engine.process_key(key, Mode::Vietnamese);
    }
    engine.output().into_owned()
}

#[test]
fn tone_key_after_an_invalid_word_is_a_letter() {
    assert_eq!(typed(InputMethod::telex(), true, "enlf"), "enlf");
    assert_eq!(typed(InputMethod::telex(), true, "eds"), "eds");
    assert_eq!(typed(InputMethod::telex(), true, "olkf"), "olkf");
    assert_eq!(typed(InputMethod::vni(), true, "enl2"), "enl2");
}

#[test]
fn tone_stays_on_the_syllable_it_was_typed_on() {
    assert_eq!(typed(InputMethod::telex(), true, "mymfyk"), "mỳmyk");
}

#[test]
fn valid_syllable_after_an_invalid_prefix_takes_the_tone() {
    assert_eq!(typed(InputMethod::telex(), true, "enlaf"), "enlà");
    assert_eq!(typed(InputMethod::telex(), false, "enlf"), "enlf");
}

#[test]
fn valid_words_keep_their_tone() {
    assert_eq!(typed(InputMethod::telex(), true, "tieengs"), "tiếng");
    assert_eq!(typed(InputMethod::telex(), true, "chuyeenr"), "chuyển");
    assert_eq!(typed(InputMethod::telex(), true, "chuyrene"), "chuyển");
    assert_eq!(typed(InputMethod::vni(), true, "tie61ng"), "tiếng");
}
