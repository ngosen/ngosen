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
use std::sync::{Arc, Mutex};

use bamboo_core::advanced::charset_names;
use bamboo_core::{BracketMode, Config, InputMethod, ToneStyle, W2uMode};

use crate::convert::{LetterCase, convert};
use crate::engine::{
    BambooEngine, Dictionary, INPUT_METHOD_NAMES, Learning, WordList, input_method_by_name,
};
use crate::handles::{self, DiacriticsStore, Object};
use crate::macro_table::MacroTable;
use crate::spelling::VietnameseSpelling;

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
    pub teencode: bool,
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

/// Reads a null-terminated array of key, value strings, in their order.
///
/// # Safety
/// `definition` is null or points to such an array.
unsafe fn read_pairs(definition: *const *const c_char) -> Vec<(String, String)> {
    let mut pairs = Vec::new();
    if definition.is_null() {
        return pairs;
    }
    let mut i = 0;
    // SAFETY: the array holds pairs up to a null key.
    unsafe {
        while !(*definition.add(i)).is_null() {
            let key = string_from_c(*definition.add(i));
            let value = string_from_c(*definition.add(i + 1));
            pairs.push((key, value));
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
    // A write to a closed pipe must not end the whole fcitx5 process.
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
pub extern "C" fn EnginePullPreedit(engine: usize) -> *mut c_char {
    handles::with_engine(engine, |e| c_string(&e.preedit_text)).unwrap_or(std::ptr::null_mut())
}

/// The keys typed for the word in progress, before any marks are applied.
#[unsafe(no_mangle)]
pub extern "C" fn EnginePullRawText(engine: usize) -> *mut c_char {
    handles::with_engine(engine, |e| c_string(&e.raw_text())).unwrap_or(std::ptr::null_mut())
}

/// The word the keys compose, with its marks even while the preedit shows the keys.
#[unsafe(no_mangle)]
pub extern "C" fn EnginePullComposed(engine: usize) -> *mut c_char {
    handles::with_engine(engine, |e| c_string(&e.composed_text())).unwrap_or(std::ptr::null_mut())
}

#[unsafe(no_mangle)]
pub extern "C" fn EngineCommitPreedit(engine: usize) {
    handles::with_engine(engine, BambooEngine::commit_preedit);
}

#[unsafe(no_mangle)]
pub extern "C" fn EnginePullCommit(engine: usize) -> *mut c_char {
    handles::with_engine(engine, |e| c_string(&e.pull_commit())).unwrap_or(std::ptr::null_mut())
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
        // The settings' "modern" style (hoà) is what bamboo-core calls Traditional.
        .tone_style(if o.modern_style {
            ToneStyle::Traditional
        } else {
            ToneStyle::Modern
        })
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
        e.teencode = o.teencode;
        e.set_output_charset(charset);
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
    let pairs: HashMap<_, _> = unsafe { read_pairs(definition) }.into_iter().collect();
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

/// `EngineRebuildFromText` with text of the last commit, taken back to type on: Đoán dấu never
/// learns a macro's text, even typed on.
///
/// # Safety
/// `text` is null or a NUL-terminated string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn EngineReopenCommitted(engine: usize, text: *const c_char) {
    // SAFETY: the caller passes a valid C string or null.
    let text = unsafe { string_from_c(text) };
    handles::with_engine(engine, |e| e.reopen_committed(&text));
}

/// Converts `text` between two charsets named as in GetCharsetNames. `letter_case` is 0 to keep,
/// 1 upper, 2 lower, 3 sentence or 4 title case. Returns null for an unknown charset or case.
///
/// # Safety
/// Each pointer is null or a NUL-terminated string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ConvertText(
    text: *const c_char,
    from: *const c_char,
    to: *const c_char,
    letter_case: c_int,
    strip_diacritics: bool,
) -> *mut c_char {
    // SAFETY: the caller passes valid C strings or null.
    let (text, from, to) = unsafe { (string_from_c(text), string_from_c(from), string_from_c(to)) };
    let (Ok(from), Ok(to), Some(case)) = (
        from.parse(),
        to.parse(),
        LetterCase::from_index(letter_case),
    ) else {
        return std::ptr::null_mut();
    };
    c_string(&convert(&text, from, to, case, strip_diacritics))
}

#[unsafe(no_mangle)]
pub extern "C" fn GetCharsetNames() -> *mut *mut c_char {
    let mut names: Vec<String> = charset_names().map(String::from).collect();
    // "Unicode" comes first, the rest by name.
    names[1..].sort();
    c_string_array(&names)
}

#[unsafe(no_mangle)]
pub extern "C" fn GetInputMethodNames() -> *mut *mut c_char {
    c_string_array(&INPUT_METHOD_NAMES)
}

/// Takes ownership of `fd` and closes it; `None` for a value that is not a descriptor.
fn read_lines(fd: usize) -> Option<Vec<String>> {
    let fd = i32::try_from(fd).ok()?;
    // SAFETY: the caller hands over an open file descriptor it no longer uses.
    let file = unsafe { File::from_raw_fd(fd) };
    let lines = BufReader::new(file)
        .split(b'\n')
        .map_while(Result::ok)
        .map(|mut line| {
            if line.last() == Some(&b'\r') {
                line.pop();
            }
            line
        })
        .filter(|line| !line.is_empty())
        .map(|line| String::from_utf8_lossy(&line).into_owned())
        .collect();
    Some(lines)
}

fn read_words(fd: usize) -> Option<Vec<String>> {
    let words = read_lines(fd)?;
    Some(words.iter().map(|word| word.to_lowercase()).collect())
}

/// Takes ownership of `fd` and closes it. One word per line; words are matched lowercased.
#[unsafe(no_mangle)]
pub extern "C" fn NewDictionary(fd: usize) -> usize {
    let Some(words) = read_words(fd) else {
        return 0;
    };
    let words: Dictionary = words.into_iter().collect();
    handles::new_handle(Object::Dictionary(Arc::new(words)))
}

/// Takes ownership of `fd` and closes it. One English word per line.
#[unsafe(no_mangle)]
pub extern "C" fn NewWordList(fd: usize) -> usize {
    let Some(words) = read_words(fd) else {
        return 0;
    };
    let words: WordList = words.into_iter().collect();
    handles::new_handle(Object::WordList(Arc::new(words)))
}

/// Takes ownership of `fd` and closes it. One abbreviation per line, a tab, then what it stands
/// for; a line without a tab is skipped.
#[unsafe(no_mangle)]
pub extern "C" fn NewShorthand(fd: usize) -> usize {
    let Some(lines) = read_lines(fd) else {
        return 0;
    };
    let pairs = lines.iter().filter_map(|line| {
        let (key, text) = line.split_once('\t')?;
        Some((key.to_string(), text.to_string()))
    });
    handles::new_handle(Object::MacroTable(Arc::new(MacroTable::new(pairs))))
}

/// Expands the abbreviations of `table`, from `NewShorthand`, in this engine; 0 turns that off.
#[unsafe(no_mangle)]
pub extern "C" fn EngineSetShorthand(engine: usize, table: usize) {
    let table = handles::macro_table(table).unwrap_or_default();
    handles::with_engine(engine, |e| e.shorthand = table);
}

/// A Backspace keeps English keys as typed; 0 for `list` means no English words.
#[unsafe(no_mangle)]
pub extern "C" fn EngineSetEnglishWords(engine: usize, list: usize) {
    let words = handles::word_list(list).unwrap_or_default();
    handles::with_engine(engine, |e| e.english_words = words);
}

/// Keys that start a word in `list` are never cut back to the Vietnamese word they broke;
/// 0 for `list` turns that rule off.
#[unsafe(no_mangle)]
pub extern "C" fn EngineSetKeepGuard(engine: usize, list: usize) {
    let words = handles::word_list(list).unwrap_or_default();
    handles::with_engine(engine, |e| e.keep_guard = words);
}

/// Lets the next key, if it is Space, split a kept word or drop the keys typed after it.
#[unsafe(no_mangle)]
pub extern "C" fn EngineSetDropKeptTail(engine: usize, on: u8) {
    handles::with_engine(engine, |e| e.drop_kept_tail = on != 0);
}

/// The changes Đoán dấu made since the last call, as one replacement: the byte length of the old
/// text, a tab, the old text and the new one. Once the last commit is in the field, the field ends
/// with the old text and should end with the new one. Empty when nothing changed. The front end
/// applies it before it forwards the key.
#[unsafe(no_mangle)]
pub extern "C" fn EnginePullChanges(engine: usize) -> *mut c_char {
    handles::with_engine(engine, |e| c_string(&e.pull_changes_text()))
        .unwrap_or(std::ptr::null_mut())
}

/// Opens the shipped table at `table` and the personal dictionary kept in `dictionary`, for every
/// field to share. Returns 0 when the table does not load. A dictionary file that does not load
/// leaves it empty and never written over; `DiacriticsProblem` says why.
///
/// # Safety
/// Each pointer is null or a NUL-terminated string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn DiacriticsOpen(table: *const c_char, dictionary: *const c_char) -> usize {
    // SAFETY: the caller passes valid C strings or null.
    let (table, dictionary) = unsafe { (string_from_c(table), string_from_c(dictionary)) };
    let Ok(table) = timsen::Table::open(&table) else {
        return 0;
    };
    let path = std::path::PathBuf::from(dictionary);
    let dictionary = timsen::Dictionary::open(Arc::new(table), &path);
    handles::new_handle(Object::Diacritics(Arc::new(DiacriticsStore {
        dictionary: Arc::new(Mutex::new(dictionary)),
        path,
        // Nothing is learned until the front end says the user wants it.
        learning: Arc::new(Learning::new(false)),
        saving: Mutex::default(),
    })))
}

/// Turns learning on or off in every field; a run open while it changes learns nothing.
#[unsafe(no_mangle)]
pub extern "C" fn DiacriticsSetLearning(handle: usize, on: u8) {
    if let Some(store) = handles::diacritics(handle) {
        store.learning.set(on != 0);
    }
}

/// Why the personal dictionary file did not load; null when it did.
#[unsafe(no_mangle)]
pub extern "C" fn DiacriticsProblem(handle: usize) -> *mut c_char {
    let store = handles::diacritics(handle);
    let dictionary = store.as_deref().map(|s| lock_dictionary(&s.dictionary));
    match dictionary.as_ref().and_then(|d| d.problem()) {
        Some(problem) => c_string(&problem.to_string()),
        None => std::ptr::null_mut(),
    }
}

fn lock_dictionary(d: &Mutex<timsen::Dictionary>) -> std::sync::MutexGuard<'_, timsen::Dictionary> {
    d.lock().unwrap_or_else(|e| e.into_inner())
}

/// Saves what the personal dictionary learned since the last save, writing on another thread so
/// typing never waits for the disk; with `wait`, as at exit, returns once everything learned is
/// written, a save already running included. Returns whether there was anything to save.
#[unsafe(no_mangle)]
pub extern "C" fn DiacriticsSave(handle: usize, wait: u8) -> u8 {
    handles::diacritics(handle).is_some_and(|store| save_diacritics(&store, wait != 0)) as u8
}

pub(crate) fn save_diacritics(store: &DiacriticsStore, wait: bool) -> bool {
    let mut saving = store.saving.lock().unwrap_or_else(|e| e.into_inner());
    if wait && let Some(running) = saving.take() {
        let _ = running.join();
    }
    let Some(job) = lock_dictionary(&store.dictionary).begin_save() else {
        return false;
    };
    let dictionary = Arc::clone(&store.dictionary);
    let save = std::thread::spawn(move || {
        let written = job.write();
        // A failed save leaves the dictionary owing one, so the next save tries again.
        let Ok(Some(sync)) = lock_dictionary(&dictionary).finish_save(job, written) else {
            return;
        };
        // The directory sync is slow on a busy disk, so it runs without holding the dictionary,
        // which every typed word needs.
        let synced = sync.sync();
        let _ = lock_dictionary(&dictionary).finish_sync(sync, synced);
    });
    if wait {
        let _ = save.join();
    } else {
        *saving = Some(save);
    }
    true
}

/// Saves what was learned and returns once it is on the disk, for the settings window to read the
/// file. Returns null and sets `written` to whether anything was new, or returns why it failed.
///
/// # Safety
/// `written` points to a writable byte.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn DiacriticsSaveNow(handle: usize, written: *mut u8) -> *mut c_char {
    let saved = match handles::diacritics(handle) {
        Some(store) => store.save_now(timsen::SyncJob::sync),
        None => Ok(crate::learned::Saved::Unchanged),
    };
    match saved {
        Ok(saved) => {
            // SAFETY: the caller passes a writable byte.
            unsafe { *written = u8::from(saved == crate::learned::Saved::Written) };
            std::ptr::null_mut()
        }
        Err(e) => c_string(&e),
    }
}

/// Forgets everything learned, in every field and in the file. Returns null, or why it failed.
#[unsafe(no_mangle)]
pub extern "C" fn DiacriticsForget(handle: usize) -> *mut c_char {
    match handles::diacritics(handle).map(|store| store.forget(timsen::SyncJob::sync)) {
        Some(Ok(())) => std::ptr::null_mut(),
        Some(Err(e)) => c_string(&e),
        None => c_string("no Đoán dấu is open"),
    }
}

/// Forgets the dictionary kept in `path` while no Đoán dấu has it open. Returns null, or why it
/// failed.
///
/// # Safety
/// `path` is null or a NUL-terminated string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn DiacriticsRemoveFiles(path: *const c_char) -> *mut c_char {
    // SAFETY: the caller passes a valid C string or null.
    let path = unsafe { string_from_c(path) };
    match crate::learned::remove_files(std::path::Path::new(&path)) {
        Ok(()) => std::ptr::null_mut(),
        Err(e) => c_string(&e.to_string()),
    }
}

/// Gives the engine Đoán dấu with the dictionary of `DiacriticsOpen`, or with 0 takes it away,
/// as when the user switches to English.
#[unsafe(no_mangle)]
pub extern "C" fn EngineSetDiacritics(engine: usize, handle: usize) {
    let store = handles::diacritics(handle);
    handles::with_engine(engine, |e| match store {
        Some(store) => e.set_diacritics(Arc::clone(&store.dictionary), Arc::clone(&store.learning)),
        None => e.remove_diacritics(),
    });
}

/// A password or other private field: nothing typed there is guessed or learned.
#[unsafe(no_mangle)]
pub extern "C" fn EngineSetPrivate(engine: usize, on: u8) {
    handles::with_engine(engine, |e| e.set_private(on != 0));
}

/// Whether Đoán dấu may change words before the one that just ended: only where the front end
/// can check them in the field before replacing them.
#[unsafe(no_mangle)]
pub extern "C" fn EngineSetWholeRun(engine: usize, on: u8) {
    handles::with_engine(engine, |e| e.set_whole_run(on != 0));
}

/// Ends the word just committed but not the run Đoán dấu is guessing, for a front end that types
/// each word into the field itself and resets the engine after it.
#[unsafe(no_mangle)]
pub extern "C" fn EngineEndWord(engine: usize) {
    handles::with_engine(engine, BambooEngine::end_word);
}

/// The margin Đoán dấu fixes syllables nothing knows by in runs started from now: 4, 3 or 2 from
/// cautious to strong, or 0 for no fixing.
#[unsafe(no_mangle)]
pub extern "C" fn EngineSetFixing(engine: usize, margin: u8) {
    let margin = (margin != 0).then(|| f64::from(margin));
    handles::with_engine(engine, |e| e.set_fixing(margin));
}

/// For Backspace right after the key that committed a fixed word: the text the field shows from
/// that word to the cursor, for the front end to check and delete, the word now reopened as
/// typed; null when there is no fix to take back.
#[unsafe(no_mangle)]
pub extern "C" fn EngineUndoFix(engine: usize) -> *mut c_char {
    handles::with_engine(engine, |e| e.undo_fix().map(|w| c_string(&w)))
        .flatten()
        .unwrap_or(std::ptr::null_mut())
}

/// For Backspace right after the key that committed a word which made Đoán dấu fix the word
/// before it: the text the field shows from the fixed word to the cursor, for the front end to
/// check before `EngineUndoEarlierFix`; null when there is no such fix.
#[unsafe(no_mangle)]
pub extern "C" fn EngineEarlierFixShown(engine: usize) -> *mut c_char {
    handles::with_engine(engine, |e| e.earlier_fix_shown().map(|t| c_string(&t)))
        .flatten()
        .unwrap_or(std::ptr::null_mut())
}

/// Takes that fix back for good and returns the text to put in place of what
/// `EngineEarlierFixShown` returned; null when there is no such fix.
#[unsafe(no_mangle)]
pub extern "C" fn EngineUndoEarlierFix(engine: usize) -> *mut c_char {
    handles::with_engine(engine, |e| e.undo_earlier_fix().map(|t| c_string(&t)))
        .flatten()
        .unwrap_or(std::ptr::null_mut())
}

/// For Cứu chữ: `text`, typed without marks, with the marks Đoán dấu gives it, in the output
/// charset; null without Đoán dấu, in a private field, or when no word changes.
///
/// # Safety
/// `text` is null or a NUL-terminated string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn EngineRestoreText(engine: usize, text: *const c_char) -> *mut c_char {
    // SAFETY: the caller passes a valid C string or null.
    let text = unsafe { string_from_c(text) };
    handles::with_engine(engine, |e| e.restore_text(&text).map(|t| c_string(&t)))
        .flatten()
        .unwrap_or(std::ptr::null_mut())
}

/// The word as shown before the last key split it, dropped its tail, or expanded it as a chat
/// abbreviation; empty otherwise.
#[unsafe(no_mangle)]
pub extern "C" fn EnginePullDroppedWord(engine: usize) -> *mut c_char {
    handles::with_engine(engine, |e| c_string(&std::mem::take(&mut e.dropped_word)))
        .unwrap_or(std::ptr::null_mut())
}

#[unsafe(no_mangle)]
pub extern "C" fn EngineKeptAtLastKey(engine: usize) -> u8 {
    handles::with_engine(engine, |e| e.kept_at_last_key()).unwrap_or(false) as u8
}

/// 1 when the input method can compose a word with the key, as VNI does with digits.
#[unsafe(no_mangle)]
pub extern "C" fn EngineComposesWith(engine: usize, key_val: u32) -> u8 {
    handles::with_engine(engine, |e| e.composes_with(key_val)).unwrap_or(false) as u8
}

#[unsafe(no_mangle)]
pub extern "C" fn EngineShownAsTyped(engine: usize) -> u8 {
    handles::with_engine(engine, |e| e.shown_as_typed()).unwrap_or(false) as u8
}

/// 1 when `text` is one correctly spelled Vietnamese syllable exactly as written, so `cho` is one
/// and `cuoi`, which needs a mark, is not.
///
/// # Safety
/// `text` is null or a NUL-terminated string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn IsVietnameseSyllable(text: *const c_char) -> u8 {
    // SAFETY: the caller passes a valid C string or null.
    let text = unsafe { string_from_c(text) };
    let mut engine =
        bamboo_core::Engine::with_config(InputMethod::default(), crate::engine::default_config());
    engine.rebuild_from_text(&text);
    let rebuilt = engine.get_processed_str(bamboo_core::OutputOptions::NONE);
    u8::from(
        rebuilt == text
            && has_vowel(&text)
            && engine.is_vietnamese_word()
            && stop_final_has_tone(&text),
    )
}

/// The engine also accepts a lone onset such as `ngh`, a syllable still being typed.
fn has_vowel(text: &str) -> bool {
    crate::text::without_diacritics(text)
        .to_lowercase()
        .contains(['a', 'e', 'i', 'o', 'u', 'y'])
}

/// A syllable ending in c, ch, p or t is only ever spoken with the sắc or nặng tone.
fn stop_final_has_tone(text: &str) -> bool {
    let lower = text.to_lowercase();
    if !["c", "ch", "p", "t"].iter().any(|f| lower.ends_with(f)) {
        return true;
    }
    lower
        .chars()
        .any(|c| "áắấéếíóốớúứýạặậẹệịọộợụựỵ".contains(c))
}

#[cfg(test)]
mod tests;
