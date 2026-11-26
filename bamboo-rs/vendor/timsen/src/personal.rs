//! The personal dictionary: n-gram counts learned from the user's own text, added on top of a table.

use std::collections::HashMap;
use std::io::{BufRead, Write};
use std::sync::Arc;

use crate::shared::Shared;
use crate::table::{BOS, EOS, MAX_SPELLINGS, SHIFT, UNK};
use crate::{Error, Table, tone};

/// The most one learned n-gram weighs against one seen in the corpora.
pub const MAX_WEIGHT: f64 = 3000.0;

/// How much one learned n-gram weighs: at most `MAX_WEIGHT`, and less once the learned text would
/// outweigh the whole table, so a large dictionary does not drown it.
pub fn weight(table_total: f64, tokens: u64) -> f64 {
    if tokens == 0 {
        MAX_WEIGHT
    } else {
        (table_total / tokens as f64).min(MAX_WEIGHT)
    }
}

const HEADER: &str = "timsen-personal 1";
const MAX_ID: u32 = (1 << SHIFT) - 1;
/// Largest count a saved file may give one n-gram; far more than anyone types, and small enough
/// that sums of counts stay exact in an `f64`.
const MAX_COUNT: u64 = 1 << 40;

/// Counts learned from text known to be right, for one table. Words the table lacks get ids after
/// the table's own, so they can be guessed too; the saved file names words by their spelling, so it
/// survives a new table.
#[derive(Clone)]
pub struct Personal<'t> {
    table: Shared<'t, Table>,
    /// Word `table.len() + i` is `new_words[i]`, its unmarked form `new_plain[i]`. `new_share[i]` is
    /// its log share of the learned spellings of that form when the table does not offer the form at
    /// all, and `None` when it does.
    new_words: Vec<String>,
    new_plain: Vec<String>,
    new_share: Vec<Option<f64>>,
    new_ids: HashMap<String, u32>,
    /// Learned words an unmarked form does not already offer in the table.
    extra: HashMap<String, Vec<u32>>,
    uni: HashMap<u32, u64>,
    bi: HashMap<u64, u64>,
    tri: HashMap<u64, u64>,
    /// The same n-grams with each learned word the table lacks read as `<unk>` where it is a context,
    /// as the table counts such words. Kept apart so a saved file names only real words; every key
    /// holds `<unk>`, so other lookups skip these maps.
    unk_uni: u64,
    unk_bi: HashMap<u64, u64>,
    unk_tri: HashMap<u64, u64>,
    /// Learned words the table knows, and run ends: the table's total leaves `<unk>` out.
    tokens: u64,
    /// Lines of the loaded file this table could not take, written back unchanged by `save`.
    kept: Vec<String>,
}

fn key2(v: u32, w: u32) -> u64 {
    (u64::from(v) << SHIFT) | u64::from(w)
}

fn key3(u: u32, v: u32, w: u32) -> u64 {
    (u64::from(u) << (2 * SHIFT)) | key2(v, w)
}

fn get(map: &HashMap<u64, u64>, k: u64) -> u64 {
    map.get(&k).copied().unwrap_or(0)
}

fn bump(count: &mut u64, by: u64) {
    *count = count.saturating_add(by);
}

/// Whether a word can be learned and saved: one line of the file holds words between tabs, and
/// `<s>` and `</s>` stand for run boundaries there.
fn savable(word: &str) -> bool {
    !word.is_empty()
        && word != "<s>"
        && word != "</s>"
        && !word.chars().any(|c| c.is_whitespace() || c.is_control())
}

impl Personal<'static> {
    /// A dictionary that shares the table instead of borrowing it, so it can be kept anywhere.
    pub fn shared(table: Arc<Table>) -> Personal<'static> {
        Personal::empty(Shared::Owned(table))
    }

    /// Like [`Personal::load`], sharing the table.
    pub fn load_shared(table: Arc<Table>, input: impl BufRead) -> Result<Personal<'static>, Error> {
        Personal::read(Shared::Owned(table), input)
    }
}

impl<'t> Personal<'t> {
    pub fn new(table: &'t Table) -> Personal<'t> {
        Personal::empty(Shared::Borrowed(table))
    }

    fn empty(table: Shared<'t, Table>) -> Personal<'t> {
        Personal {
            table,
            new_words: Vec::new(),
            new_plain: Vec::new(),
            new_share: Vec::new(),
            new_ids: HashMap::new(),
            extra: HashMap::new(),
            uni: HashMap::new(),
            bi: HashMap::new(),
            tri: HashMap::new(),
            unk_uni: 0,
            unk_bi: HashMap::new(),
            unk_tri: HashMap::new(),
            tokens: 0,
            kept: Vec::new(),
        }
    }

    pub fn table(&self) -> &Table {
        &self.table
    }

    pub fn is_empty(&self) -> bool {
        self.tokens == 0
    }

    /// Forgets everything learned.
    pub fn clear(&mut self) {
        *self = Personal::empty(self.table.clone());
    }

    /// Learns a run of words known to be right: typed with marks, or a guess the user corrected.
    /// `None` stands for a word that is not known to be right; no n-gram is learned across it.
    pub fn learn(&mut self, run: &[Option<&str>]) {
        let ids: Vec<Option<u32>> = run.iter().map(|w| w.and_then(|w| self.intern(w))).collect();
        if ids.iter().all(Option::is_none) {
            return;
        }
        let seq: Vec<Option<u32>> = [Some(BOS), Some(BOS)]
            .into_iter()
            .chain(ids)
            .chain([Some(EOS)])
            .collect();
        for &id in seq[2..].iter().flatten() {
            self.add(&[id], 1);
        }
        for i in 2..seq.len() {
            if let (Some(v), Some(w)) = (seq[i - 1], seq[i]) {
                self.add(&[v, w], 1);
                if let Some(u) = seq[i - 2] {
                    self.add(&[u, v, w], 1);
                }
            }
        }
    }

    /// Counts an n-gram `c` times, and again with words the table lacks read as `<unk>`: in context
    /// position always, and as the last word for the n-grams that serve as a longer one's context.
    fn add(&mut self, ids: &[u32], c: u64) {
        match *ids {
            [w] => {
                bump(self.uni.entry(w).or_default(), c);
                if self.own(w) {
                    bump(&mut self.tokens, c);
                } else {
                    bump(&mut self.unk_uni, c);
                    self.share(w);
                }
            }
            [v, w] => {
                bump(self.bi.entry(key2(v, w)).or_default(), c);
                let mut keys = Vec::new();
                if !self.own(v) {
                    keys.push(key2(UNK, w));
                }
                if !self.own(w) {
                    keys.push(key2(self.context(v), UNK));
                }
                for k in keys {
                    bump(self.unk_bi.entry(k).or_default(), c);
                }
            }
            [u, v, w] => {
                bump(self.tri.entry(key3(u, v, w)).or_default(), c);
                let mut keys = Vec::new();
                if !self.own(u) || !self.own(v) {
                    keys.push(key3(self.context(u), self.context(v), w));
                }
                if !self.own(w) {
                    keys.push(key3(self.context(u), self.context(v), UNK));
                }
                for k in keys {
                    bump(self.unk_tri.entry(k).or_default(), c);
                }
            }
            _ => unreachable!("n-grams are one to three words"),
        }
    }

    /// The id a word stands for as a context: words the table lacks are `<unk>` there, so they share
    /// what the table and the other learned words say about unknown words.
    pub(crate) fn context(&self, id: u32) -> u32 {
        if self.own(id) { id } else { UNK }
    }

    /// A new word whose unmarked form the table does not offer never competes with a table spelling,
    /// so it is scored as `<unk>`, as the table scores such words, split among the learned spellings
    /// of that form by their counts. Other words keep their own counts.
    pub(crate) fn predicted(&self, id: u32) -> (u32, f64) {
        let Some(i) = (id as usize).checked_sub(self.table.len()) else {
            return (id, 0.0);
        };
        match self.new_share[i] {
            Some(share) => (UNK, share),
            None => (id, 0.0),
        }
    }

    /// Refreshes the shares of the spellings of new word `id`'s unmarked form after its count moved.
    fn share(&mut self, id: u32) {
        let i = id as usize - self.table.len();
        if self.new_share[i].is_none() {
            return;
        }
        let ids = &self.extra[&self.new_plain[i]];
        let count = |id: &u32| self.uni.get(id).copied().unwrap_or(0) as f64;
        let sum: f64 = ids.iter().map(count).sum();
        let shares: Vec<(usize, f64)> = ids
            .iter()
            .map(|id| {
                let share = if ids.len() == 1 {
                    0.0
                } else {
                    (count(id).max(0.5) / sum.max(1.0)).ln()
                };
                (*id as usize - self.table.len(), share)
            })
            .collect();
        for (j, share) in shares {
            self.new_share[j] = Some(share);
        }
    }

    /// The id of a word as typed, adding it if the table lacks it; `None` for a word that cannot be
    /// saved, once ids run out, or once its unmarked form has `MAX_SPELLINGS` spellings.
    fn intern(&mut self, word: &str) -> Option<u32> {
        let word = tone::to_new(&word.to_lowercase());
        if !savable(&word) {
            return None;
        }
        let plain = tone::strip(&word);
        let offered: Vec<u32> = self.table.options(&plain).into_iter().flatten().collect();
        if let Some(&id) = offered.iter().find(|&&id| self.table.word(id) == word) {
            return Some(id);
        }
        if let Some(&id) = self.new_ids.get(&word) {
            return Some(id);
        }
        let id = u32::try_from(self.table.len() + self.new_words.len()).ok()?;
        let learned = self.extra.get(&plain).map_or(0, Vec::len);
        if id > MAX_ID || offered.len() + learned >= MAX_SPELLINGS {
            return None;
        }
        self.new_ids.insert(word.clone(), id);
        self.new_words.push(word);
        self.new_share.push(offered.is_empty().then_some(0.0));
        self.new_plain.push(plain.clone());
        // Ordered by spelling, not by when they were learned, so a reloaded file breaks ties alike.
        let ids = self.extra.entry(plain).or_default();
        let at = ids.partition_point(|&i| {
            self.new_words[i as usize - self.table.len()]
                < self.new_words[id as usize - self.table.len()]
        });
        ids.insert(at, id);
        Some(id)
    }

    /// Spellings of an unmarked syllable: the table's, then learned words it lacks.
    pub(crate) fn options(&self, plain: &str) -> Option<Vec<u32>> {
        let mut ids: Vec<u32> = self.table.options(plain).into_iter().flatten().collect();
        ids.extend(self.extra.get(plain).into_iter().flatten());
        (!ids.is_empty()).then_some(ids)
    }

    /// The id of a learned word the table lacks, if it was learned.
    pub(crate) fn new_id(&self, word: &str) -> Option<u32> {
        self.new_ids.get(word).copied()
    }

    pub(crate) fn word(&self, id: u32) -> &str {
        match (id as usize).checked_sub(self.table.len()) {
            Some(i) => &self.new_words[i],
            None => self.table.word(id),
        }
    }

    fn own(&self, id: u32) -> bool {
        (id as usize) < self.table.len()
    }

    pub(crate) fn uni(&self, id: u32) -> f64 {
        let id = if id == BOS { EOS } else { id };
        let base = if self.own(id) {
            self.table.uni(id)
        } else {
            0.0
        };
        let unk = if id == UNK { self.unk_uni } else { 0 };
        base + self.weight() * self.uni.get(&id).copied().unwrap_or(0).saturating_add(unk) as f64
    }

    pub(crate) fn bigram(&self, v: u32, w: u32) -> f64 {
        let base = if self.own(v) && self.own(w) {
            self.table.bigram(v, w)
        } else {
            0.0
        };
        let k = key2(v, w);
        let unk = if v == UNK || w == UNK {
            get(&self.unk_bi, k)
        } else {
            0
        };
        base + self.weight() * get(&self.bi, k).saturating_add(unk) as f64
    }

    pub(crate) fn trigram(&self, u: u32, v: u32, w: u32) -> f64 {
        let own = self.own(u) && self.own(v) && self.own(w);
        let base = if own {
            self.table.trigram(u, v, w)
        } else {
            0.0
        };
        let k = key3(u, v, w);
        let unk = if u == UNK || v == UNK || w == UNK {
            get(&self.unk_tri, k)
        } else {
            0
        };
        base + self.weight() * get(&self.tri, k).saturating_add(unk) as f64
    }

    pub(crate) fn total(&self) -> f64 {
        self.table.total() + self.weight() * self.tokens as f64
    }

    pub fn weight(&self) -> f64 {
        weight(self.table.total(), self.tokens)
    }

    /// The table's shares: learned counts do not change them.
    pub(crate) fn left(&self, u: u32, v: u32) -> f64 {
        if self.own(u) && self.own(v) {
            self.table.left(u, v)
        } else {
            0.0
        }
    }

    pub(crate) fn left_uni(&self, v: u32) -> f64 {
        if self.own(v) {
            self.table.left_uni(v)
        } else {
            0.0
        }
    }

    pub(crate) fn segments(&self) -> f64 {
        self.uni(EOS)
    }

    /// Writes what was learned, one n-gram per line with words by spelling.
    pub fn save(&self, out: &mut impl Write) -> std::io::Result<()> {
        let name = |id: u32| match id {
            BOS => "<s>",
            EOS => "</s>",
            _ => self.word(id),
        };
        let mut lines: Vec<String> = Vec::new();
        for (&id, &c) in &self.uni {
            lines.push(format!("1\t{c}\t{}", name(id)));
        }
        let mask = u64::from(MAX_ID);
        for (&k, &c) in &self.bi {
            let (v, w) = ((k >> SHIFT) as u32, (k & mask) as u32);
            lines.push(format!("2\t{c}\t{}\t{}", name(v), name(w)));
        }
        for (&k, &c) in &self.tri {
            let (u, v, w) = (
                (k >> (2 * SHIFT)) as u32,
                ((k >> SHIFT) & mask) as u32,
                (k & mask) as u32,
            );
            lines.push(format!("3\t{c}\t{}\t{}\t{}", name(u), name(v), name(w)));
        }
        lines.extend(self.kept.iter().cloned());
        lines.sort();
        writeln!(out, "{HEADER}")?;
        for line in lines {
            writeln!(out, "{line}")?;
        }
        Ok(())
    }

    /// Reads a file written by [`Personal::save`], for this table.
    pub fn load(table: &'t Table, input: impl BufRead) -> Result<Personal<'t>, Error> {
        Personal::read(Shared::Borrowed(table), input)
    }

    fn read(table: Shared<'t, Table>, input: impl BufRead) -> Result<Personal<'t>, Error> {
        let mut p = Personal::empty(table);
        let mut lines = input.lines();
        if lines.next().transpose()?.as_deref() != Some(HEADER) {
            return Err(Error::Format("not a personal dictionary"));
        }
        'lines: for line in lines {
            let line = line?;
            let fields: Vec<&str> = line.split('\t').collect();
            let (Some(n), Some(c)) = (fields.first(), fields.get(1)) else {
                return Err(Error::Format("bad personal dictionary line"));
            };
            let c: u64 = c
                .parse()
                .ok()
                .filter(|&c| c <= MAX_COUNT)
                .ok_or(Error::Format("bad count in personal dictionary"))?;
            let words = &fields[2..];
            if n.parse::<usize>().ok() != Some(words.len()) || !(1..=3).contains(&words.len()) {
                return Err(Error::Format("bad personal dictionary line"));
            }
            let mut ids = Vec::with_capacity(words.len());
            for w in words {
                ids.push(match *w {
                    "<s>" => BOS,
                    "</s>" => EOS,
                    // A word this table cannot take any more, as `learn` would have dropped it;
                    // the line is kept so a table that can take it finds it again.
                    w => match p.intern(w) {
                        Some(id) => id,
                        None => {
                            p.kept.push(line);
                            continue 'lines;
                        }
                    },
                });
            }
            p.add(&ids, c);
        }
        Ok(p)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ToneStyle;

    fn table() -> Table {
        let path = concat!(env!("CARGO_MANIFEST_DIR"), "/tests/fixtures/tiny.timsen");
        Table::open(path).unwrap()
    }

    fn learn(p: &mut Personal, text: &str) {
        let run: Vec<Option<&str>> = text.split(' ').map(Some).collect();
        p.learn(&run);
    }

    #[test]
    fn a_word_the_table_lacks_can_be_guessed() {
        let t = table();
        let mut p = Personal::new(&t);
        assert_eq!(p.restore(&["ngo", "sen"], ToneStyle::New), ["ngo", "sen"]);
        learn(&mut p, "ngó sen");
        assert_eq!(p.restore(&["Ngo", "sen"], ToneStyle::New), ["Ngó", "sen"]);
    }

    #[test]
    fn a_learned_unmarked_word_competes_with_the_tables_spellings() {
        let t = table();
        let mut p = Personal::new(&t);
        assert_ne!(p.restore(&["let", "go"], ToneStyle::New)[1], "go");
        learn(&mut p, "let go");
        assert_eq!(p.restore(&["let", "go"], ToneStyle::New)[1], "go");
        assert_eq!(p.restore(&["toi", "go", "phim"], ToneStyle::New)[1], "gõ");
    }

    #[test]
    fn a_new_context_word_shares_what_other_new_words_were_followed_by() {
        let t = table();
        let mut p = Personal::new(&t);
        for _ in 0..3 {
            learn(&mut p, "gõ");
        }
        learn(&mut p, "abc gỗ");
        learn(&mut p, "def gỗ");
        learn(&mut p, "ghi");
        assert_eq!(p.restore(&["ghi", "go"], ToneStyle::New), ["ghi", "gỗ"]);
    }

    #[test]
    fn a_new_word_alone_in_its_form_is_scored_as_unknown_after_its_context() {
        let t = table();
        let mut p = Personal::new(&t);
        for _ in 0..3 {
            learn(&mut p, "gõ");
        }
        learn(&mut p, "gỗ abc nhà");
        learn(&mut p, "gỗ def nhà");
        learn(&mut p, "ghi");
        assert_eq!(p.restore(&["go", "ghi"], ToneStyle::New), ["gỗ", "ghi"]);
    }

    #[test]
    fn new_spellings_of_one_form_split_by_how_often_they_were_learned() {
        let t = table();
        let mut p = Personal::new(&t);
        learn(&mut p, "ngộ");
        learn(&mut p, "ngó");
        learn(&mut p, "ngó");
        assert_eq!(p.restore(&["ngo"], ToneStyle::New), ["ngó"]);
    }

    #[test]
    fn nothing_is_learned_across_an_unknown_word() {
        let t = table();
        let mut p = Personal::new(&t);
        p.learn(&[Some("nhà"), None, Some("gõ")]);
        let (nha, go) = (p.intern("nhà").unwrap(), p.intern("gõ").unwrap());
        assert_eq!(p.uni[&nha], 1);
        assert_eq!(p.uni[&go], 1);
        assert!(p.bi.keys().all(|&k| k != key2(nha, go)));
        p.learn(&[None, None]);
        assert_eq!(p.uni[&EOS], 1);
    }

    #[test]
    fn counts_that_overflow_are_an_error() {
        let t = table();
        let text = "timsen-personal 1\n1\t18446744073709551615\t</s>\n1\t1\t</s>\n";
        assert!(Personal::load(&t, text.as_bytes()).is_err());
    }

    #[test]
    fn words_a_saved_file_cannot_hold_are_not_learned() {
        let t = table();
        let mut p = Personal::new(&t);
        p.learn(&[
            Some("nhà"),
            Some("a\tb"),
            Some("ta"),
            Some("x\ny"),
            Some("<s>"),
        ]);
        p.learn(&[Some("</s>"), Some("có"), Some("<S>")]);
        let mut file = Vec::new();
        p.save(&mut file).unwrap();
        let q = Personal::load(&t, file.as_slice()).unwrap();
        let mut again = Vec::new();
        q.save(&mut again).unwrap();
        assert_eq!(file, again);
        let text = String::from_utf8(file).unwrap();
        assert!(text.contains("\tnhà\n") && text.contains("\tcó\n"));
        // A learned "<s>" would end a line; as a run boundary it never does.
        assert!(!text.contains("a\tb") && !text.contains("x\n") && !text.contains("\t<s>\n"));
    }

    #[test]
    fn lines_with_a_word_the_table_cannot_take_are_written_back() {
        let t = table();
        let a: Vec<char> = "aàáảãạăằắẳẵặâầấẩẫậ".chars().collect();
        let mut text = format!("{HEADER}\n");
        let mut words: Vec<String> = (a.iter())
            .flat_map(|x| a.iter().map(move |y| format!("{x}{y}")))
            .collect();
        words.sort();
        for w in &words {
            text.push_str(&format!("1\t1\t{w}\n"));
        }
        let p = Personal::load(&t, text.as_bytes()).unwrap();
        assert!(
            p.options("aa").unwrap().len() < words.len(),
            "some words must be refused"
        );
        let mut file = Vec::new();
        p.save(&mut file).unwrap();
        assert_eq!(String::from_utf8(file).unwrap(), text);
    }

    #[test]
    fn clear_drops_the_lines_kept_from_the_file() {
        let t = table();
        let text = format!("{HEADER}\n1\t1\tx\ry\n");
        let mut p = Personal::load(&t, text.as_bytes()).unwrap();
        p.clear();
        let mut file = Vec::new();
        p.save(&mut file).unwrap();
        assert_eq!(String::from_utf8(file).unwrap(), format!("{HEADER}\n"));
    }

    #[test]
    fn saving_and_loading_keeps_ties_between_new_spellings() {
        let t = table();
        for words in [["ngó", "ngò"], ["ngò", "ngó"]] {
            let mut p = Personal::new(&t);
            for w in words {
                learn(&mut p, w);
            }
            let mut file = Vec::new();
            p.save(&mut file).unwrap();
            let q = Personal::load(&t, file.as_slice()).unwrap();
            let run = ["ngo"];
            assert_eq!(
                q.restore(&run, ToneStyle::New),
                p.restore(&run, ToneStyle::New)
            );
        }
    }

    #[test]
    fn a_form_keeps_at_most_max_spellings() {
        let t = table();
        let mut p = Personal::new(&t);
        let a: Vec<char> = "aàáảãạăằắẳẵặâầấẩẫậ".chars().collect();
        for x in &a {
            for y in &a {
                learn(&mut p, &format!("{x}{y}"));
            }
        }
        assert!(p.options("aa").unwrap().len() <= crate::table::MAX_SPELLINGS);
    }

    #[test]
    fn saving_and_loading_keeps_what_was_learned() {
        let t = table();
        let mut p = Personal::new(&t);
        learn(&mut p, "nhà ta có gõ quý");
        learn(&mut p, "ngó sen");
        let mut file = Vec::new();
        p.save(&mut file).unwrap();
        let text = String::from_utf8(file.clone()).unwrap();
        assert!(text.contains("\tngó\tsen") && text.starts_with(HEADER));
        let q = Personal::load(&t, file.as_slice()).unwrap();
        for run in [&["nha", "ta", "co", "go", "quy"][..], &["ngo", "sen"]] {
            assert_eq!(
                q.restore(run, ToneStyle::New),
                p.restore(run, ToneStyle::New)
            );
        }
        assert_eq!(q.total(), p.total());
    }

    #[test]
    fn the_total_adds_learned_known_words_and_run_ends() {
        let t = table();
        let mut p = Personal::new(&t);
        learn(&mut p, "nhà ta");
        learn(&mut p, "ngó");
        // The tiny table's total is small, so four learned tokens already lower the weight.
        assert_eq!(p.weight(), t.total() / 4.0);
        assert_eq!(p.total(), t.total() + p.weight() * (3.0 + 1.0));
    }

    #[test]
    fn the_weight_falls_once_learned_text_outweighs_the_table() {
        assert_eq!(weight(1e9, 0), MAX_WEIGHT);
        assert_eq!(weight(1e9, 10), MAX_WEIGHT);
        assert_eq!(weight(6e8, 2_000_000), 300.0);
    }

    #[test]
    fn clear_forgets_everything() {
        let t = table();
        let mut p = Personal::new(&t);
        learn(&mut p, "ngó sen");
        p.clear();
        assert!(p.is_empty());
        assert_eq!(p.restore(&["ngo"], ToneStyle::New), ["ngo"]);
    }

    #[test]
    fn a_broken_file_is_an_error() {
        let t = table();
        for text in [
            "",
            "something else\n",
            "timsen-personal 1\n2\t1\tgõ\n",
            "timsen-personal 1\n1\tx\tgõ\n",
        ] {
            assert!(Personal::load(&t, text.as_bytes()).is_err(), "{text:?}");
        }
    }
}
