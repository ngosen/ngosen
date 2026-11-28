//! Fixing a typed syllable the table does not know: to a known form one edit away (`khogn`), as
//! `research.typo` does, or to a word the input method found near the keys typed (`mguoifw`), as
//! `research.telex_typo` does.

use super::{EOS, START, Typing, UNK};
use crate::tone;

/// How far the best fix must score above keeping the word, or the next fix, before it is made.
pub const FIX_MARGIN: f64 = 4.0;

/// How far a fix of a word the table knows must win, decided once the word after it is typed: such
/// a word is mostly typed as meant.
pub const MARKS_MARGIN: f64 = 12.0;

/// What one unit of a nearby word's cost takes off its score.
const NEARBY_SCALE: f64 = 2.0;

/// The least share of the table's words a nearby word must be: 1,000 occurrences in the released
/// small table. A table also holds the typos of its corpus, and a typo must not be a fix.
const NEARBY_SHARE: f64 = 1.87e-6;

/// The longest syllable that is a fix target, in letters.
const MAX_TARGET: usize = 32;

/// The word to show for an unknown syllable, and how far it scores above keeping the syllable or
/// the next word. [`Typing::fix`] gives a form without marks, [`Typing::fix_nearby`] a spelling.
#[derive(Clone, Debug, PartialEq)]
pub struct Fix {
    pub word: String,
    pub margin: f64,
}

/// A word that keys near the ones typed would have made, and how far those keys are from them: 1
/// for a slip made easily (the key beside, two keys in the wrong order), more for an unlikely one.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Nearby<'a> {
    pub word: &'a str,
    pub cost: f64,
}

/// A candidate fix: the word, the ids it may stand for, and what its score loses.
struct Candidate {
    word: String,
    ids: Vec<u32>,
    penalty: f64,
}

/// Every string one edit away: a letter replaced, removed or added, or two neighbours swapped.
fn edits(word: &str) -> Vec<String> {
    let chars: Vec<char> = word.chars().collect();
    let mut out = Vec::new();
    let join = |parts: &[&[char]]| parts.concat().into_iter().collect::<String>();
    for i in 0..=chars.len() {
        let (a, b) = chars.split_at(i);
        if let Some((_, rest)) = b.split_first() {
            out.push(join(&[a, rest]));
            out.extend(('a'..='z').map(|c| join(&[a, &[c], rest])));
        }
        if b.len() > 1 {
            out.push(join(&[a, &[b[1], b[0]], &b[2..]]));
        }
        out.extend(('a'..='z').map(|c| join(&[a, &[c], b])));
    }
    out.sort();
    out.dedup();
    out.retain(|e| e != word);
    out
}

impl Typing<'_> {
    /// The fix for syllable `i` when it was typed without marks and neither the table nor the
    /// personal dictionary knows it, or `None`. Candidates are the known forms one edit away, each
    /// scored by every trigram through `i` with its best marked spelling, around the best guess of
    /// the run; `ended` says whether the run is over, as for [`Typing::finish`]. The margin is over
    /// the next candidate or keeping the word as typed, whichever scores higher.
    pub fn fix(&self, i: usize, ended: bool) -> Option<Fix> {
        if self.opts.get(i)? != &[UNK] || self.kept[i][0].is_some() {
            return None;
        }
        // Edits are of plain letters only, and a letter beyond them, such as a mark typed as a
        // combining character, says the word was meant as typed. The length bound keeps the
        // edits of pasted text small; no plain form in a table comes near it.
        let typed = &self.typed[i];
        if typed.len() > MAX_TARGET || !typed.chars().all(|c| c.is_ascii_alphabetic()) {
            return None;
        }
        let typed = typed.to_ascii_lowercase();
        let cands = (edits(&typed).into_iter())
            .filter_map(|word| {
                let ids = self.src.options(&word)?;
                Some(Candidate {
                    word,
                    ids,
                    penalty: 0.0,
                })
            })
            .collect();
        self.best_fix(i, ended, cands, false, &[UNK])
    }

    /// The fix for syllable `i` among `nearby`, the words the input method would have shown for
    /// keys near the ones typed, when neither the table nor the personal dictionary knows the
    /// syllable; `None` otherwise, or when the table has seen none of the words often enough. Each
    /// word is scored like a candidate of [`Typing::fix`], less its cost, and the fix is its best
    /// spelling: the word itself when it has marks.
    pub fn fix_nearby(&self, i: usize, ended: bool, nearby: &[Nearby<'_>]) -> Option<Fix> {
        if self.opts.get(i)? != &[UNK] || self.typed[i].len() > MAX_TARGET {
            return None;
        }
        let cands = self.nearby_candidates(nearby);
        self.best_fix(i, ended, cands, true, &[UNK])
    }

    /// The fix for syllable `i`, which the table may know, among `nearby`, words that differ from
    /// it only in marks; `None` when none is common enough. Scored as [`Typing::fix_nearby`] does,
    /// except that keeping the syllable is scored as the best of its readings, not as an unknown
    /// word: English keys read as a word with marks must not lose to a word near that one.
    pub fn fix_marks(&self, i: usize, ended: bool, nearby: &[Nearby<'_>]) -> Option<Fix> {
        let (typed, readings) = (self.typed.get(i)?, self.opts.get(i)?);
        if typed.len() > MAX_TARGET {
            return None;
        }
        let itself = tone::to_new(&typed.to_lowercase());
        let mut cands = self.nearby_candidates(nearby);
        cands.retain(|c| c.word != itself);
        self.best_fix(i, ended, cands, true, readings)
    }

    fn nearby_candidates(&self, nearby: &[Nearby<'_>]) -> Vec<Candidate> {
        (nearby.iter())
            .filter_map(|near| {
                let word = tone::to_new(&near.word.to_lowercase());
                let ids = self.common(&word)?;
                Some(Candidate {
                    word,
                    ids,
                    penalty: NEARBY_SCALE * near.cost,
                })
            })
            .collect()
    }

    /// Whether the table or the personal dictionary knows a syllable, so that it is no fix target.
    pub(crate) fn knows(&self, syllable: &str) -> bool {
        let plain = syllable.to_lowercase();
        if tone::strip(&plain) == plain {
            self.src.options(&plain).is_some()
        } else {
            self.id_of(syllable) != UNK
        }
    }

    /// The table's ids for a nearby word, every spelling for one without marks, or `None` when
    /// the table does not know it or saw each of them too rarely.
    fn common(&self, word: &str) -> Option<Vec<u32>> {
        let table = self.src.table();
        let plain = tone::strip(word);
        let mut ids = table.options(&plain)?;
        let ids: Vec<u32> = if plain == word {
            ids.collect()
        } else {
            vec![ids.find(|&id| table.word(id) == word)?]
        };
        let seen = ids.iter().map(|&id| table.uni(id)).fold(0.0, f64::max);
        (seen >= NEARBY_SHARE * table.total()).then_some(ids)
    }

    /// The best of `cands` at syllable `i`, over keeping it as the best of `keep`. With `spelled`, a candidate
    /// counts as its best spelling, so two that come to the same spelling are one.
    fn best_fix(
        &self,
        i: usize,
        ended: bool,
        cands: Vec<Candidate>,
        spelled: bool,
        keep: &[u32],
    ) -> Option<Fix> {
        let picks = self.best_path(ended);
        let seq: Vec<u32> = (0..self.opts.len())
            .map(|k| self.opts[k][picks[k]])
            .collect();
        let mut rated: Vec<(String, f64)> = Vec::new();
        for cand in cands {
            // Strictly greater keeps the first best, as numpy's argmax does.
            let (mut best, mut score) = (cand.ids[0], f64::NEG_INFINITY);
            for &id in &cand.ids {
                let s = self.local(&seq, i, id, ended);
                if s > score {
                    (best, score) = (id, s);
                }
            }
            let word = if spelled {
                self.src.word(best).to_string()
            } else {
                cand.word
            };
            rated.push((word, score - cand.penalty));
        }
        // By word, its best score first: each word keeps that score, and the first of equal
        // scores wins in the sorted order of the words.
        rated.sort_by(|a, b| a.0.cmp(&b.0).then(b.1.total_cmp(&a.1)));
        rated.dedup_by(|later, first| later.0 == first.0);
        let best = (0..rated.len()).reduce(|b, k| if rated[k].1 > rated[b].1 { k } else { b })?;
        let keep = (keep.iter())
            .map(|&id| self.local(&seq, i, id, ended))
            .fold(f64::NEG_INFINITY, f64::max);
        let runner = (0..rated.len())
            .filter(|&k| k != best)
            .map(|k| rated[k].1)
            .fold(keep, f64::max);
        Some(Fix {
            margin: rated[best].1 - runner,
            word: rated.swap_remove(best).0,
        })
    }

    /// The sum of every trigram score through syllable `i`, with `id` in its place.
    fn local(&self, seq: &[u32], i: usize, id: u32, ended: bool) -> f64 {
        let mut pad: Vec<u32> = [START[0], START[0]]
            .into_iter()
            .chain(seq.iter().copied())
            .collect();
        if ended {
            pad.push(EOS);
        }
        let j = i + 2;
        pad[j] = id;
        let mut total = 0.0;
        for k in j..(j + 3).min(pad.len()) {
            total += self.src.logp(pad[k - 2], pad[k - 1], pad[k]);
        }
        total
    }
}
