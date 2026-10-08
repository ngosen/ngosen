// SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

//! Objects handed to C++ as numbers. A handle is looked up rather than dereferenced, so a zero,
//! stale or wrong-kind handle from C++ is a miss instead of a crash of the whole fcitx5 process.

use std::collections::BTreeMap;
use std::sync::{Arc, Mutex, MutexGuard};

use crate::engine::{BambooEngine, Dictionary};
use crate::macro_table::MacroTable;

pub enum Object {
    Engine(Box<BambooEngine>),
    MacroTable(Arc<MacroTable>),
    Dictionary(Arc<Dictionary>),
}

struct Registry {
    next: usize,
    objects: BTreeMap<usize, Object>,
}

static REGISTRY: Mutex<Registry> = Mutex::new(Registry {
    next: 1,
    objects: BTreeMap::new(),
});

fn registry() -> MutexGuard<'static, Registry> {
    REGISTRY.lock().unwrap_or_else(|e| e.into_inner())
}

pub fn new_handle(object: Object) -> usize {
    let mut reg = registry();
    let handle = reg.next;
    reg.next += 1;
    reg.objects.insert(handle, object);
    handle
}

pub fn with_engine<R>(handle: usize, f: impl FnOnce(&mut BambooEngine) -> R) -> Option<R> {
    match registry().objects.get_mut(&handle) {
        Some(Object::Engine(engine)) => Some(f(engine)),
        _ => None,
    }
}

pub fn macro_table(handle: usize) -> Option<Arc<MacroTable>> {
    match registry().objects.get(&handle) {
        Some(Object::MacroTable(table)) => Some(Arc::clone(table)),
        _ => None,
    }
}

pub fn dictionary(handle: usize) -> Option<Arc<Dictionary>> {
    match registry().objects.get(&handle) {
        Some(Object::Dictionary(dict)) => Some(Arc::clone(dict)),
        _ => None,
    }
}

pub fn delete_handle(handle: usize) {
    registry().objects.remove(&handle);
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::engine::input_method_by_name;

    #[test]
    fn handle_helpers_reject_zero_handle() {
        assert!(with_engine(0, |_| ()).is_none());
        assert!(macro_table(0).is_none());
        assert!(dictionary(0).is_none());
        delete_handle(0);
    }

    #[test]
    fn handle_helpers_round_trip() {
        let engine = BambooEngine::new(
            input_method_by_name("Telex"),
            Arc::default(),
            Arc::default(),
        );
        let handle = new_handle(Object::Engine(Box::new(engine)));
        assert_eq!(with_engine(handle, |e| e.macro_enabled), Some(false));
        // A handle holding another kind of object is a miss, not that object.
        assert!(macro_table(handle).is_none());
        delete_handle(handle);
        assert!(with_engine(handle, |_| ()).is_none());
    }
}
