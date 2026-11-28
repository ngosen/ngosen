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
    /// Keys are stored lowercased, as `get` looks them up. A key written in lower case wins over
    /// the same key written with capitals; among those, the first in `entries` wins.
    pub fn new(entries: impl IntoIterator<Item = (String, String)>) -> Self {
        let entries = entries.into_iter();
        let mut table = HashMap::with_capacity(entries.size_hint().0);
        for (key, value) in entries {
            let lower = key.to_lowercase();
            if key == lower || !table.contains_key(&lower) {
                table.insert(lower, value);
            }
        }
        Self { table }
    }

    pub fn is_empty(&self) -> bool {
        self.table.is_empty()
    }

    pub fn entries(&self) -> impl Iterator<Item = (&str, &str)> {
        self.table.iter().map(|(k, v)| (k.as_str(), v.as_str()))
    }

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
        MacroTable::new(pairs.iter().map(|&(k, v)| (k.to_string(), v.to_string())))
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

    #[test]
    fn macro_table_key_written_with_capitals() {
        let t = table(&[
            ("VN", "Việt Nam"),
            ("KO", "Kansas"),
            ("ko", "không"),
            ("Ko", "Korea"),
        ]);
        for key in ["vn", "Vn", "VN"] {
            assert_eq!(t.get(key), Some("Việt Nam"), "{key}");
        }
        assert_eq!(t.get("KO"), Some("không"), "the lower-case key wins");
    }

    #[test]
    fn macro_table_first_capitalized_key_wins() {
        let t = table(&[("KO", "Kansas"), ("Ko", "Korea")]);
        assert_eq!(t.get("ko"), Some("Kansas"));
        let t = table(&[("Ko", "Korea"), ("KO", "Kansas")]);
        assert_eq!(t.get("ko"), Some("Korea"));
    }
}
