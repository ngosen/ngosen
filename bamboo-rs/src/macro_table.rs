// SPDX-FileCopyrightText: 2022 CSSlayer <wengxt@gmail.com>
// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

use std::collections::HashMap;

#[derive(Debug, Default)]
pub struct MacroTable {
    table: HashMap<String, String>,
}

impl MacroTable {
    pub fn new(table: HashMap<String, String>) -> Self {
        Self { table }
    }

    pub fn is_empty(&self) -> bool {
        self.table.is_empty()
    }

    /// Keys are stored as the user wrote them; only the typed text is lowercased.
    pub fn get(&self, key: &str) -> Option<&str> {
        if key.is_empty() || self.is_empty() {
            return None;
        }
        self.table.get(&key.to_lowercase()).map(String::as_str)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn table(pairs: &[(&str, &str)]) -> MacroTable {
        MacroTable::new(
            pairs
                .iter()
                .map(|&(k, v)| (k.to_string(), v.to_string()))
                .collect(),
        )
    }

    #[test]
    fn macro_table_empty() {
        assert!(MacroTable::default().is_empty());
        assert!(!table(&[("vn", "Việt Nam")]).is_empty());
    }

    #[test]
    fn macro_table_get() {
        let t = table(&[("vn", "Việt Nam")]);
        assert_eq!(t.get("VN"), Some("Việt Nam"), "case-insensitive");
        assert!(t.get("vn").is_some());
        assert!(t.get("").is_none());
        assert!(t.get("xyz").is_none());
        assert!(MacroTable::default().get("vn").is_none());
    }
}
