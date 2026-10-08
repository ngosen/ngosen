use bamboo_core::{Config, Engine, InputMethod, Mode};

fn typed(im: InputMethod, keys: &str) -> String {
    let config = Config::builder().free_tone_marking(true).auto_correct(false).build();
    let mut engine = Engine::with_config(im, config);
    for key in keys.chars() {
        engine.process_key(key, Mode::Vietnamese);
    }
    engine.output().into_owned()
}

#[test]
fn undoing_a_mark_keeps_the_tone_telex() {
    assert_eq!(typed(InputMethod::telex(), "uwfw"), "ùw");
    assert_eq!(typed(InputMethod::telex(), "ojww"), "ọw");
}

#[test]
fn undoing_a_mark_keeps_the_tone_vni() {
    assert_eq!(typed(InputMethod::vni(), "go366"), "gỏ6");
}

#[test]
fn undoing_a_tone_still_types_the_key() {
    assert_eq!(typed(InputMethod::telex(), "ress"), "res");
    assert_eq!(typed(InputMethod::telex(), "uww"), "uw");
}
