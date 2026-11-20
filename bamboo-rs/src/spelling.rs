// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

//! Whether the word an engine composes is Vietnamese, the same with every `bamboo-core` 0.5.x.

use bamboo_core::{Engine, OutputOptions};

pub(crate) trait VietnameseSpelling {
    fn is_vietnamese_prefix(&self) -> bool;
    fn is_vietnamese_word(&self) -> bool;
}

impl VietnameseSpelling for Engine {
    fn is_vietnamese_prefix(&self) -> bool {
        self.is_valid_prefix() && !starts_with_teencode_onset(self)
    }

    fn is_vietnamese_word(&self) -> bool {
        self.is_valid_word() && !starts_with_teencode_onset(self)
    }
}

/// bamboo-core 0.5.5 takes `f`, `j` and `w` as onsets (`fải`, `jậy`). Teencode mode reads them
/// itself, so everywhere else they keep English words such as `just` English.
fn starts_with_teencode_onset(engine: &Engine) -> bool {
    let word = engine.get_processed_str(OutputOptions::LOWER_CASE);
    matches!(word.chars().next(), Some('f' | 'j' | 'w'))
}
