// SPDX-FileCopyrightText: 2022 CSSlayer <wengxt@gmail.com>
// SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

//! The functions declared in `include/bamboo-core.h`. Strings and arrays returned to C++ are
//! allocated with `malloc`, because the caller releases them with `free`.

use std::collections::HashMap;
use std::ffi::{CStr, c_char, c_int};
use std::fs::File;
use std::io::{BufRead, BufReader};
use std::os::fd::FromRawFd;
use std::sync::Arc;

use bamboo_core::advanced::get_charset_names;
use bamboo_core::{BracketMode, Config, InputMethod, W2uMode};

use crate::engine::{BambooEngine, Dictionary, INPUT_METHOD_NAMES, input_method_by_name};
use crate::handles::{self, Object};
use crate::macro_table::MacroTable;

/// `FcitxBambooEngineOption` in the header.
#[repr(C)]
pub struct EngineOption {
    pub auto_non_vn_restore: bool,
    pub dd_free_style: bool,
    pub macro_enabled: bool,
    pub auto_capitalize_macro: bool,
    pub spell_check_with_dicts: bool,
    pub output_charset: *const c_char,
    pub modern_style: bool,
    pub free_marking: bool,
    pub w2u: c_int,
    pub bracket_transform: c_int,
    pub time_format: *const c_char,
    pub date_format: *const c_char,
}

/// # Safety
/// `p` is null or a NUL-terminated string.
unsafe fn string_from_c(p: *const c_char) -> String {
    if p.is_null() {
        return String::new();
    }
    // SAFETY: the caller passes a valid C string.
    unsafe { CStr::from_ptr(p) }.to_string_lossy().into_owned()
}

fn c_string(s: &str) -> *mut c_char {
    // SAFETY: the buffer is allocated with room for the bytes and the terminating NUL.
    unsafe {
        let p = libc::malloc(s.len() + 1).cast::<u8>();
        if p.is_null() {
            return std::ptr::null_mut();
        }
        std::ptr::copy_nonoverlapping(s.as_ptr(), p, s.len());
        *p.add(s.len()) = 0;
        p.cast()
    }
}

fn c_string_array<S: AsRef<str>>(strs: &[S]) -> *mut *mut c_char {
    let size = (strs.len() + 1) * size_of::<*mut c_char>();
    // SAFETY: the array has room for every string plus the terminating null pointer.
    unsafe {
        let array = libc::malloc(size).cast::<*mut c_char>();
        if array.is_null() {
            return array;
        }
        for (i, s) in strs.iter().enumerate() {
            *array.add(i) = c_string(s.as_ref());
        }
        *array.add(strs.len()) = std::ptr::null_mut();
        array
    }
}

/// Reads a null-terminated array of key, value strings.
///
/// # Safety
/// `definition` is null or points to such an array.
unsafe fn read_pairs(definition: *const *const c_char) -> HashMap<String, String> {
    let mut pairs = HashMap::new();
    if definition.is_null() {
        return pairs;
    }
    let mut i = 0;
    // SAFETY: the array holds pairs up to a null key.
    unsafe {
        while !(*definition.add(i)).is_null() {
            let key = string_from_c(*definition.add(i));
            let value = string_from_c(*definition.add(i + 1));
            pairs.insert(key, value);
            i += 2;
        }
    }
    pairs
}

fn w2u_mode(value: c_int) -> W2uMode {
    match value {
        1 => W2uMode::NonStart,
        2 => W2uMode::Everywhere,
        _ => W2uMode::Disabled,
    }
}

fn bracket_mode(value: c_int) -> BracketMode {
    match value {
        1 => BracketMode::NonStart,
        2 => BracketMode::Everywhere,
        _ => BracketMode::Disabled,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn Init() {
    // The Go core ignored SIGPIPE for the whole fcitx5 process; keep that.
    // SAFETY: setting a signal to SIG_IGN has no other effect.
    unsafe {
        libc::signal(libc::SIGPIPE, libc::SIG_IGN);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn EngineProcessKeyEvent(engine: usize, key_val: u32, state: u32) -> u8 {
    handles::with_engine(engine, |e| e.process_key_event(key_val, state)).unwrap_or(false) as u8
}

#[unsafe(no_mangle)]
pub extern "C" fn EngineSetRestoreKeyStroke(engine: usize) {
    handles::with_engine(engine, |e| e.should_restore_key_strokes = true);
}

#[unsafe(no_mangle)]
pub extern "C" fn EnginePullPreedit(engine: usize) -> *mut c_char {
    handles::with_engine(engine, |e| c_string(&e.preedit_text)).unwrap_or(std::ptr::null_mut())
}

#[unsafe(no_mangle)]
pub extern "C" fn EngineCommitPreedit(engine: usize) {
    handles::with_engine(engine, BambooEngine::commit_preedit);
}

#[unsafe(no_mangle)]
pub extern "C" fn EnginePullCommit(engine: usize) -> *mut c_char {
    handles::with_engine(engine, |e| c_string(&e.pull_commit())).unwrap_or(std::ptr::null_mut())
}

#[unsafe(no_mangle)]
pub extern "C" fn EngineSetMacroEnabled(engine: usize, enabled: u8) {
    handles::with_engine(engine, |e| e.macro_enabled = enabled != 0);
}

/// # Safety
/// `option` is null or points to a valid option struct.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn EngineSetOption(engine: usize, option: *const EngineOption) {
    // SAFETY: the caller passes a valid pointer or null.
    let Some(o) = (unsafe { option.as_ref() }) else {
        return;
    };
    let config = Config::builder()
        .free_tone_marking(o.free_marking)
        .std_tone_style(!o.modern_style)
        .auto_correct(false)
        .w2u_mode(w2u_mode(o.w2u))
        .bracket_mode(bracket_mode(o.bracket_transform))
        .build();
    // SAFETY: the strings in the option are valid or null.
    let (charset, time_format, date_format) = unsafe {
        (
            string_from_c(o.output_charset),
            string_from_c(o.time_format),
            string_from_c(o.date_format),
        )
    };
    handles::with_engine(engine, |e| {
        e.auto_non_vn_restore = o.auto_non_vn_restore;
        e.dd_free_style = o.dd_free_style;
        e.macro_enabled = o.macro_enabled;
        e.auto_capitalize_macro = o.auto_capitalize_macro;
        e.spell_check_with_dicts = o.spell_check_with_dicts;
        e.output_charset = charset;
        e.preeditor.set_config(config);
        e.time_format = time_format;
        e.date_format = date_format;
    });
}

fn new_engine_handle(
    engine: impl FnOnce(Arc<Dictionary>, Arc<MacroTable>) -> BambooEngine,
    dict_handle: usize,
    table_handle: usize,
) -> usize {
    let dictionary = handles::dictionary(dict_handle).unwrap_or_default();
    let Some(table) = handles::macro_table(table_handle) else {
        return 0;
    };
    handles::new_handle(Object::Engine(Box::new(engine(dictionary, table))))
}

/// # Safety
/// `name` is null or a NUL-terminated string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn NewEngine(
    name: *const c_char,
    dict_handle: usize,
    table_handle: usize,
) -> usize {
    // SAFETY: the caller passes a valid C string or null.
    let name = unsafe { string_from_c(name) };
    let make = |dict, table| BambooEngine::new(input_method_by_name(&name), dict, table);
    new_engine_handle(make, dict_handle, table_handle)
}

/// # Safety
/// `definition` is null or a null-terminated array of key, rule strings.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn NewCustomEngine(
    definition: *const *const c_char,
    dict_handle: usize,
    table_handle: usize,
) -> usize {
    // SAFETY: the caller passes a valid array or null.
    let pairs = unsafe { read_pairs(definition) };
    let make = |dict, table| {
        let im = InputMethod::from_definition("Custom", &pairs);
        let mut engine = BambooEngine::new(im, dict, table);
        engine.spell_check_with_dicts = false;
        engine
    };
    new_engine_handle(make, dict_handle, table_handle)
}

/// # Safety
/// `definition` is null or a null-terminated array of key, value strings.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn NewMacroTable(definition: *const *const c_char) -> usize {
    // SAFETY: the caller passes a valid array or null.
    let pairs = unsafe { read_pairs(definition) };
    handles::new_handle(Object::MacroTable(Arc::new(MacroTable::new(pairs))))
}

#[unsafe(no_mangle)]
pub extern "C" fn DeleteObject(handle: usize) {
    handles::delete_handle(handle);
}

#[unsafe(no_mangle)]
pub extern "C" fn ResetEngine(engine: usize) {
    handles::with_engine(engine, BambooEngine::reset);
}

/// # Safety
/// `text` is null or a NUL-terminated string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn EngineRebuildFromText(engine: usize, text: *const c_char) {
    // SAFETY: the caller passes a valid C string or null.
    let text = unsafe { string_from_c(text) };
    handles::with_engine(engine, |e| e.rebuild_from_text(&text));
}

#[unsafe(no_mangle)]
pub extern "C" fn GetCharsetNames() -> *mut *mut c_char {
    let mut names = get_charset_names();
    // "Unicode" comes first, the rest by name.
    names[1..].sort();
    c_string_array(&names)
}

#[unsafe(no_mangle)]
pub extern "C" fn GetInputMethodNames() -> *mut *mut c_char {
    c_string_array(&INPUT_METHOD_NAMES)
}

/// Takes ownership of `fd` and closes it. One word per line; words are matched lowercased.
#[unsafe(no_mangle)]
pub extern "C" fn NewDictionary(fd: usize) -> usize {
    let Ok(fd) = i32::try_from(fd) else {
        return 0;
    };
    // SAFETY: the caller hands over an open file descriptor it no longer uses.
    let file = unsafe { File::from_raw_fd(fd) };
    let words: Dictionary = BufReader::new(file)
        .split(b'\n')
        .map_while(Result::ok)
        .map(|mut line| {
            if line.last() == Some(&b'\r') {
                line.pop();
            }
            line
        })
        .filter(|line| !line.is_empty())
        .map(|line| String::from_utf8_lossy(&line).to_lowercase())
        .collect();
    handles::new_handle(Object::Dictionary(Arc::new(words)))
}

#[cfg(test)]
mod tests;
