// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

//! Per-key typing logic of Ngó Sen on top of the `bamboo-core` crate, exported with the C
//! interface the addon used to get from the Go core (`include/bamboo-core.h`).

mod engine;
mod ffi;
mod handles;
mod macro_table;
mod text;
mod time_format;

pub use ffi::*;
