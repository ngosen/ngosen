// SPDX-FileCopyrightText: 2018 Luong Thanh Lam <ltlam93@gmail.com>
// SPDX-FileCopyrightText: 2022 CSSlayer <wengxt@gmail.com>
// SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

//! What one key press does to the word being typed: Vietnamese composition, falling back to
//! the typed keys for English words, macros and the output charset.

use std::collections::HashSet;
use std::sync::Arc;

use bamboo_core::advanced::encode;
use bamboo_core::{Config, Engine, InputMethod, InputMethodPreset, Mode, OutputOptions, W2uMode};

use crate::macro_table::MacroTable;
use crate::text::{has_any_vietnamese_rune, has_any_vietnamese_vowel, is_word_break_symbol};
use crate::time_format::format_time;

#[cfg(test)]
mod options_tests;
#[cfg(test)]
mod tests;

pub const LOCK_MASK: u32 = 1 << 1;
pub const CONTROL_MASK: u32 = 1 << 2;
pub const MOD1_MASK: u32 = 1 << 3;
pub const IGNORED_MASK: u32 = 1 << 25;
pub const SUPER_MASK: u32 = 1 << 26;
pub const HYPER_MASK: u32 = 1 << 27;
pub const META_MASK: u32 = 1 << 28;

pub const KEY_BACKSPACE: u32 = 0xff08;
pub const KEY_SPACE: u32 = 0x020;
pub const KEY_TAB: u32 = 0xff09;

/// The input methods the addon offers, in the order it lists them.
pub const INPUT_METHOD_NAMES: [&str; 7] = [
    "Telex",
    "VNI",
    "Telex + VNI",
    "Telex + VNI + VIQR",
    "VIQR",
    "Microsoft layout",
    "VNI Bàn phím tiếng Pháp",
];

pub type Dictionary = HashSet<String>;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MacroCase {
    AllSmall,
    AllCapital,
    NoChange,
}

pub fn determine_macro_case(s: &str) -> MacroCase {
    let has_lower = s.chars().any(char::is_lowercase);
    let has_upper = s.chars().any(char::is_uppercase);
    match (has_lower, has_upper) {
        (true, false) => MacroCase::AllSmall,
        (false, true) => MacroCase::AllCapital,
        _ => MacroCase::NoChange,
    }
}

/// The last character of `s`, or NUL when it is empty.
pub fn get_last_rune(s: &str) -> char {
    s.chars().next_back().unwrap_or('\0')
}

pub fn in_key_list(list: &[char], key: char) -> bool {
    list.contains(&key)
}

/// Key values outside Unicode (fcitx keysyms) match no character class.
fn key_char(key_val: u32) -> char {
    char::from_u32(key_val).unwrap_or(char::REPLACEMENT_CHARACTER)
}

/// Unknown names give an input method without rules, so every key is typed as is.
pub fn input_method_by_name(name: &str) -> InputMethod {
    if !INPUT_METHOD_NAMES.contains(&name) {
        return InputMethod::default();
    }
    name.parse::<InputMethodPreset>()
        .map(InputMethod::from_preset)
        .unwrap_or_default()
}

/// The Go core's default flags: free tone marking, standard tone style and w → ư everywhere.
/// Its auto-correct flag had no effect, so the crate's auto-correct stays off.
pub fn default_config() -> Config {
    Config::builder()
        .free_tone_marking(true)
        .std_tone_style(true)
        .auto_correct(false)
        .w2u_mode(W2uMode::Everywhere)
        .build()
}

pub struct BambooEngine {
    pub(crate) preeditor: Engine,
    pub(crate) macro_table: Arc<MacroTable>,
    pub(crate) dictionary: Arc<Dictionary>,
    pub(crate) auto_non_vn_restore: bool,
    pub(crate) dd_free_style: bool,
    pub(crate) macro_enabled: bool,
    pub(crate) auto_capitalize_macro: bool,
    pub(crate) spell_check_with_dicts: bool,
    pub(crate) preedit_text: String,
    pub(crate) commit_text: String,
    pub(crate) should_restore_key_strokes: bool,
    pub(crate) output_charset: String,
    pub(crate) time_format: String,
    pub(crate) date_format: String,
}

impl BambooEngine {
    pub fn new(
        input_method: InputMethod,
        dictionary: Arc<Dictionary>,
        macro_table: Arc<MacroTable>,
    ) -> Self {
        Self {
            preeditor: Engine::with_config(input_method, default_config()),
            macro_table,
            dictionary,
            auto_non_vn_restore: true,
            dd_free_style: true,
            macro_enabled: false,
            auto_capitalize_macro: false,
            spell_check_with_dicts: true,
            preedit_text: String::new(),
            commit_text: String::new(),
            should_restore_key_strokes: false,
            output_charset: "Unicode".to_string(),
            time_format: "%H:%M".to_string(),
            date_format: "%d/%m/%Y".to_string(),
        }
    }

    pub(crate) fn expand_macro(&self, s: &str, macro_text: &str) -> String {
        let mut text = macro_text.to_string();
        if !self.time_format.is_empty() && text.contains("$TIME") {
            text = text.replace("$TIME", &format_time(&self.time_format));
        }
        if !self.date_format.is_empty() && text.contains("$DATE") {
            text = text.replace("$DATE", &format_time(&self.date_format));
        }
        if self.auto_capitalize_macro {
            match determine_macro_case(s) {
                MacroCase::AllSmall => return text.to_lowercase(),
                MacroCase::AllCapital => return text.to_uppercase(),
                MacroCase::NoChange => {}
            }
        }
        text
    }

    fn get_macro_text(&self) -> Option<String> {
        if !self.macro_enabled || self.macro_table.is_empty() {
            return None;
        }
        let text = self.last_spaced_word();
        let macro_val = self.macro_table.get(&text)?;
        Some(self.expand_macro(&text, macro_val))
    }

    fn should_fallback_to_english(&self, check_vn_rune: bool) -> bool {
        if !self.auto_non_vn_restore {
            return false;
        }
        let vn_seq = self.get_processed_string(OutputOptions::LOWER_CASE);
        if vn_seq.is_empty() {
            return false;
        }
        if self.macro_enabled && self.macro_table.get(&self.last_spaced_word()).is_some() {
            return false;
        }
        // dd stays đ even outside Vietnamese words, since abbreviations use it a lot.
        if self.dd_free_style
            && !has_any_vietnamese_vowel(&vn_seq)
            && (vn_seq.ends_with('d') || vn_seq.contains('đ'))
        {
            return false;
        }
        if check_vn_rune && !has_any_vietnamese_rune(&vn_seq) {
            return false;
        }
        !self.preeditor.is_valid(false)
    }

    fn get_processed_string(&self, options: OutputOptions) -> String {
        self.preeditor.get_processed_str(options)
    }

    /// The text typed since the last space. The crate's punctuation mode reads only the active
    /// syllable, which a digit or symbol has already committed: macro keys like `123` need this.
    fn last_spaced_word(&self) -> String {
        let full = self.get_processed_string(OutputOptions::FULL_TEXT);
        full.rsplit(' ').next().unwrap_or_default().to_string()
    }

    fn raw_text(&self) -> String {
        self.get_processed_string(OutputOptions::RAW | OutputOptions::FULL_TEXT)
    }

    pub(crate) fn get_preedit_string(&self) -> String {
        if self.should_fallback_to_english(true) {
            return self.raw_text();
        }
        self.get_processed_string(OutputOptions::PUNCTUATION_MODE | OutputOptions::FULL_TEXT)
    }

    fn rune_count(&self) -> usize {
        self.get_preedit_string().chars().count()
    }

    fn get_bamboo_input_mode(&self) -> Mode {
        if self.should_fallback_to_english(false) {
            Mode::English
        } else {
            Mode::Vietnamese
        }
    }

    /// Caps Lock swaps the brackets that type ơ and ư, so it gives Ơ and Ư.
    pub(crate) fn to_upper(&self, key: char) -> char {
        let swapped = match key {
            '[' => '{',
            ']' => '}',
            '{' => '[',
            '}' => ']',
            _ => return key,
        };
        if self.preeditor.can_process_key(key) {
            swapped
        } else {
            key
        }
    }

    pub(crate) fn must_fallback_to_english(&self) -> bool {
        if !self.auto_non_vn_restore {
            return false;
        }
        let vn_seq = self.get_processed_string(OutputOptions::LOWER_CASE);
        if vn_seq.is_empty() {
            return false;
        }
        if self.dd_free_style && vn_seq.contains('đ') {
            return false;
        }
        if self.spell_check_with_dicts {
            return !self.dictionary.contains(&vn_seq);
        }
        !self.preeditor.is_valid(true)
    }

    /// Commits a word break typed right after a key that appends it, such as `[[` -> `[`.
    fn break_after_appended_key(&mut self) {
        self.preeditor.remove_last_output_char();
        self.preeditor.process_key(' ', Mode::English);
    }

    fn appending_key_text(&mut self, key: char, old_text: &str) -> (String, bool) {
        let full_seq = self.get_processed_string(OutputOptions::NONE);
        let new_text = if self.should_fallback_to_english(true) {
            self.raw_text()
        } else {
            full_seq.clone()
        };
        if !full_seq.is_empty() && get_last_rune(&full_seq) == key {
            // [[ => [
            let ret = self.get_preedit_string();
            let is_word_break = is_word_break_symbol(get_last_rune(&ret));
            if is_word_break {
                self.break_after_appended_key();
            }
            return (ret, is_word_break);
        }
        if !new_text.is_empty() && get_last_rune(&new_text) == key {
            // f] => f]
            let is_word_break = is_word_break_symbol(key);
            if is_word_break {
                self.break_after_appended_key();
            }
            return (format!("{old_text}{key}"), is_word_break);
        }
        // ] => o?
        (self.get_preedit_string(), false)
    }

    fn composing_key_text(&mut self, mut key: char, state: u32, old_text: &str) -> (String, bool) {
        if !self.preeditor.can_process_key(key) && self.should_fallback_to_english(true) {
            self.preeditor.restore_last_word(false);
        }
        if state & LOCK_MASK != 0 {
            key = self.to_upper(key);
        }
        let mode = self.get_bamboo_input_mode();
        self.preeditor.process_key(key, mode);
        if in_key_list(self.preeditor.input_method().appending_keys(), key) {
            return self.appending_key_text(key, old_text);
        }
        (self.get_preedit_string(), false)
    }

    fn word_break_text(&mut self, key: char, old_text: &str) -> (String, bool) {
        if self.macro_enabled
            && let Some(macro_val) = self.macro_table.get(old_text)
        {
            let expanded = self.expand_macro(old_text, macro_val);
            self.preeditor.reset();
            return (format!("{expanded}{key}"), true);
        }
        if has_any_vietnamese_rune(old_text) && self.must_fallback_to_english() {
            self.preeditor.restore_last_word(false);
            let new_text = format!("{}{key}", self.raw_text());
            self.preeditor.process_key(key, Mode::English);
            return (new_text, true);
        }
        self.preeditor.process_key(key, Mode::English);
        (format!("{old_text}{key}"), true)
    }

    /// The text after `key_val`, and whether it ends the word.
    fn get_commit_text(&mut self, key_val: u32, state: u32, old_text: &str) -> (String, bool) {
        let key = key_char(key_val);
        // Shift + Space gives back the keys typed for the word.
        if self.should_restore_key_strokes {
            self.should_restore_key_strokes = false;
            self.preeditor
                .restore_last_word(!has_any_vietnamese_rune(old_text));
            return (self.get_preedit_string(), false);
        }
        let can_process = self.preeditor.can_process_key(key);
        if can_process || (self.macro_enabled && key.is_ascii_digit()) {
            return self.composing_key_text(key, state, old_text);
        }
        if is_word_break_symbol(key) {
            return self.word_break_text(key, old_text);
        }
        (String::new(), true)
    }

    fn encode_text(&self, text: &str) -> String {
        encode(&self.output_charset, text)
    }

    pub(crate) fn commit_preedit_and_reset(&mut self, s: String) {
        self.commit_text = s;
        self.preedit_text.clear();
        self.preeditor.reset();
    }

    pub(crate) fn update_preedit(&mut self, processed: &str) {
        let encoded = self.encode_text(processed);
        if encoded.is_empty() {
            self.preedit_text.clear();
            self.commit_text.clear();
            return;
        }
        self.preedit_text = encoded;
    }

    pub(crate) fn can_process_key(&self, key_val: u32) -> bool {
        let key = key_char(key_val);
        if key_val == KEY_SPACE || key_val == KEY_BACKSPACE || is_word_break_symbol(key) {
            return true;
        }
        if key_val == KEY_TAB
            && self.macro_enabled
            && !self.preedit_text.is_empty()
            && self.macro_table.get(&self.last_spaced_word()).is_some()
        {
            return true;
        }
        self.preeditor.can_process_key(key)
    }

    pub(crate) fn is_valid_state(state: u32) -> bool {
        const INVALID: u32 =
            CONTROL_MASK | MOD1_MASK | IGNORED_MASK | SUPER_MASK | HYPER_MASK | META_MASK;
        state & INVALID == 0
    }

    fn get_composed_string(&self, old_text: String) -> String {
        if has_any_vietnamese_rune(&old_text) && self.must_fallback_to_english() {
            return self.raw_text();
        }
        old_text
    }

    /// The crate commits a digit or symbol typed in a macro key such as `a1`, and can only
    /// delete from the active syllable, so the keys before a committed last one are replayed.
    fn remove_last_char(&mut self) {
        if !self.get_processed_string(OutputOptions::NONE).is_empty() {
            self.preeditor.remove_last_output_char();
            return;
        }
        let mut keys = self.raw_text();
        keys.pop();
        self.preeditor.reset();
        for key in keys.chars() {
            let mode = self.get_bamboo_input_mode();
            self.preeditor.process_key(key, mode);
        }
    }

    fn backspace(&mut self) -> bool {
        if self.preedit_text.is_empty() {
            return false;
        }
        if self.rune_count() == 1 {
            self.commit_preedit_and_reset(String::new());
            return true;
        }
        self.remove_last_char();
        let preedit = self.get_preedit_string();
        self.update_preedit(&preedit);
        true
    }

    fn tab(&mut self, old_text: String) -> bool {
        if let Some(macro_text) = self.get_macro_text() {
            self.commit_preedit_and_reset(macro_text);
            return true;
        }
        let composed = self.get_composed_string(old_text);
        self.commit_preedit_and_reset(composed);
        false
    }

    /// Returns whether the key was used; the caller forwards it to the app otherwise.
    pub fn process_key_event(&mut self, key_val: u32, state: u32) -> bool {
        let has_preedit = !self.preedit_text.is_empty();
        // Commit and pass the key on, which Chrome's address bar and Google Sheets need.
        if !self.should_restore_key_strokes
            && (!Self::is_valid_state(state)
                || !self.can_process_key(key_val)
                || (!self.macro_enabled
                    && !has_preedit
                    && !self.preeditor.can_process_key(key_char(key_val))))
        {
            if has_preedit {
                let preedit = self.get_preedit_string();
                self.commit_preedit_and_reset(preedit);
            }
            return false;
        }
        if key_val == KEY_BACKSPACE {
            return self.backspace();
        }
        let old_text = self.get_preedit_string();
        if key_val == KEY_TAB {
            return self.tab(old_text);
        }
        let (new_text, is_word_break) = self.get_commit_text(key_val, state, &old_text);
        if is_word_break {
            self.commit_preedit_and_reset(new_text);
            return true;
        }
        self.update_preedit(&new_text);
        true
    }

    pub fn pull_commit(&mut self) -> String {
        let text = std::mem::take(&mut self.commit_text);
        self.encode_text(&text)
    }

    pub fn commit_preedit(&mut self) {
        let preedit = self.get_preedit_string();
        self.commit_preedit_and_reset(preedit);
    }

    pub fn reset(&mut self) {
        self.commit_preedit_and_reset(String::new());
    }

    pub fn rebuild_from_text(&mut self, text: &str) {
        self.preeditor.rebuild_from_text(text);
        self.preedit_text = self.get_preedit_string();
        self.commit_text.clear();
    }
}
