// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

use super::*;
use std::ffi::CString;
use std::io::Write;

/// Takes a string returned by the C interface.
fn take_string(p: *mut c_char) -> Option<String> {
    if p.is_null() {
        return None;
    }
    // SAFETY: `p` came from c_string, which allocates with malloc.
    unsafe {
        let s = CStr::from_ptr(p).to_string_lossy().into_owned();
        libc::free(p.cast());
        Some(s)
    }
}

fn take_string_array(array: *mut *mut c_char) -> Vec<String> {
    let mut out = Vec::new();
    // SAFETY: `array` came from c_string_array: malloc'd and null-terminated.
    unsafe {
        let mut i = 0;
        while !(*array.add(i)).is_null() {
            out.extend(take_string(*array.add(i)));
            i += 1;
        }
        libc::free(array.cast());
    }
    out
}

fn new_macro_table(pairs: &[(&str, &str)]) -> usize {
    let strings: Vec<CString> = pairs
        .iter()
        .flat_map(|&(k, v)| [k, v])
        .map(|s| CString::new(s).unwrap())
        .collect();
    let mut ptrs: Vec<*const c_char> = strings.iter().map(|s| s.as_ptr()).collect();
    ptrs.push(std::ptr::null());
    // SAFETY: a null-terminated array of valid strings.
    unsafe { NewMacroTable(ptrs.as_ptr()) }
}

/// Hands the content over through a pipe, the way C++ hands over an opened file.
fn new_dictionary(content: &str) -> usize {
    let mut fds = [0; 2];
    // SAFETY: `fds` has room for both ends of the pipe.
    assert_eq!(unsafe { libc::pipe(fds.as_mut_ptr()) }, 0);
    // SAFETY: the write end was just opened and is owned here.
    let mut writer = unsafe { std::fs::File::from_raw_fd(fds[1]) };
    writer.write_all(content.as_bytes()).unwrap();
    drop(writer);
    NewDictionary(fds[0] as usize)
}

fn new_engine(name: &str, dict: usize, table: usize) -> usize {
    let name = CString::new(name).unwrap();
    // SAFETY: a valid C string.
    unsafe { NewEngine(name.as_ptr(), dict, table) }
}

fn type_keys(engine: usize, keys: &str) {
    for c in keys.chars() {
        EngineProcessKeyEvent(engine, c as u32, 0);
    }
}

#[test]
fn input_method_names_match_go_order() {
    let names = take_string_array(GetInputMethodNames());
    assert_eq!(names, INPUT_METHOD_NAMES);
}

#[test]
fn charset_names_match_go_order() {
    let names = take_string_array(GetCharsetNames());
    let want = [
        "Unicode",
        "BKHCM 1",
        "BKHCM 2",
        "NCR Decimal",
        "NCR Hex",
        "TCVN3 (ABC)",
        "UTF-8",
        "Unicode C string Decimal",
        "Unicode C string Hex",
        "Unicode tổ hợp",
        "VIQR",
        "VISCII",
        "VNI Windows",
        "VPS",
        "Vietware Full",
        "Vietware X",
        "Windows 1258 codepage",
    ];
    assert_eq!(names, want);
}

#[test]
fn new_engine_needs_a_macro_table() {
    assert_eq!(new_engine("Telex", 0, 0), 0);
    let dict = new_dictionary("chào\n");
    assert_eq!(
        new_engine("Telex", dict, dict),
        0,
        "a dictionary is not a macro table"
    );
    DeleteObject(dict);
}

#[test]
fn invalid_handles_are_ignored() {
    assert_eq!(EngineProcessKeyEvent(0, 'a' as u32, 0), 0);
    assert!(take_string(EnginePullPreedit(0)).is_none());
    assert!(take_string(EnginePullCommit(0)).is_none());
    EngineCommitPreedit(0);
    ResetEngine(0);
    DeleteObject(0);
    DeleteObject(usize::MAX);
}

#[test]
fn dictionary_words_are_lowercased_and_trimmed() {
    let dict = new_dictionary("Chào\r\n\nkhỏe\nTÔI");
    let words = handles::dictionary(dict).unwrap();
    for w in ["chào", "khỏe", "tôi"] {
        assert!(words.contains(w), "{w}");
    }
    assert_eq!(words.len(), 3);
    DeleteObject(dict);
    assert_eq!(NewDictionary(usize::MAX), 0);
}

#[test]
fn typing_through_the_c_interface() {
    let table = new_macro_table(&[("vn", "Việt Nam")]);
    let dict = new_dictionary("chào\n");
    let engine = new_engine("Telex", dict, table);
    assert_ne!(engine, 0);
    // The engine keeps its own references.
    DeleteObject(dict);
    DeleteObject(table);

    type_keys(engine, "chaof");
    assert_eq!(
        take_string(EnginePullPreedit(engine)).as_deref(),
        Some("chào")
    );
    type_keys(engine, " ");
    assert_eq!(
        take_string(EnginePullCommit(engine)).as_deref(),
        Some("chào ")
    );
    assert_eq!(
        take_string(EnginePullCommit(engine)).as_deref(),
        Some(""),
        "commit is taken once"
    );

    EngineSetMacroEnabled(engine, 1);
    type_keys(engine, "vn ");
    assert_eq!(
        take_string(EnginePullCommit(engine)).as_deref(),
        Some("Việt Nam ")
    );

    // SAFETY: a valid C string.
    unsafe { EngineRebuildFromText(engine, c"tôi".as_ptr()) };
    type_keys(engine, "s");
    assert_eq!(
        take_string(EnginePullPreedit(engine)).as_deref(),
        Some("tối")
    );
    DeleteObject(engine);
}

#[test]
fn set_option_applies_settings_and_charset() {
    let table = new_macro_table(&[]);
    let engine = new_engine("Telex", 0, table);
    let option = EngineOption {
        auto_non_vn_restore: true,
        dd_free_style: true,
        macro_enabled: false,
        auto_capitalize_macro: false,
        spell_check_with_dicts: false,
        output_charset: c"VIQR".as_ptr(),
        modern_style: true,
        free_marking: true,
        w2u: 0,
        bracket_transform: 2,
        time_format: c"%H:%M".as_ptr(),
        date_format: c"%d/%m/%Y".as_ptr(),
    };
    // SAFETY: a valid option struct.
    unsafe { EngineSetOption(engine, &option) };
    type_keys(engine, "hoaf");
    EngineCommitPreedit(engine);
    assert_eq!(
        take_string(EnginePullCommit(engine)).as_deref(),
        Some("hoa`")
    );
    type_keys(engine, "t[");
    assert_eq!(
        take_string(EnginePullPreedit(engine)).as_deref(),
        Some("to+")
    );
    DeleteObject(engine);
    DeleteObject(table);
}

#[test]
fn unknown_input_method_types_keys_as_is() {
    let table = new_macro_table(&[]);
    let engine = new_engine("Telex 2", 0, table);
    assert_ne!(engine, 0);
    type_keys(engine, "aa");
    EngineCommitPreedit(engine);
    assert_eq!(take_string(EnginePullCommit(engine)).as_deref(), Some("aa"));
    DeleteObject(engine);
    DeleteObject(table);
}

#[test]
fn custom_engine_uses_its_definition() {
    let table = new_macro_table(&[]);
    let keys: Vec<CString> = ["q", "DauSac", "a", "A_Â"]
        .iter()
        .map(|s| CString::new(*s).unwrap())
        .collect();
    let mut ptrs: Vec<*const c_char> = keys.iter().map(|s| s.as_ptr()).collect();
    ptrs.push(std::ptr::null());
    // SAFETY: a null-terminated array of valid strings.
    let engine = unsafe { NewCustomEngine(ptrs.as_ptr(), 0, table) };
    type_keys(engine, "aaq");
    assert_eq!(take_string(EnginePullPreedit(engine)).as_deref(), Some("ấ"));
    DeleteObject(engine);
    DeleteObject(table);
}
