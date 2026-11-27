//! Fixing a typed syllable the table does not know (`khogn`) to a known form one edit away, as
//! `research.typo` does.

use super::{EOS, START, Typing, UNK};

/// How far the best fix must score above keeping the word, or the next fix, before it is made.
pub const FIX_MARGIN: f64 = 4.0;

/// The longest syllable that is a fix target, in letters.
const MAX_TARGET: usize = 32;

/// A known form one edit away from an unknown syllable, without marks, and how far it scores above
/// keeping the word or the next form.
#[derive(Clone, Debug, PartialEq)]
pub struct Fix {
    pub plain: String,
    pub margin: f64,
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
        let cands: Vec<(String, Vec<u32>)> = (edits(&typed).into_iter())
            .filter_map(|c| self.src.options(&c).map(|o| (c, o)))
            .collect();
        if cands.is_empty() {
            return None;
        }
        let picks = self.best_path(ended);
        let seq: Vec<u32> = (0..self.opts.len())
            .map(|k| self.opts[k][picks[k]])
            .collect();
        let keep = self.local(&seq, i, UNK, ended);
        let scores: Vec<f64> = (cands.iter())
            .map(|(_, ids)| {
                (ids.iter().map(|&id| self.local(&seq, i, id, ended)))
                    .fold(f64::NEG_INFINITY, f64::max)
            })
            .collect();
        // The first of equal scores wins, in the sorted order of the candidates.
        let best = (0..scores.len()).fold(0, |b, k| if scores[k] > scores[b] { k } else { b });
        let runner = (0..scores.len())
            .filter(|&k| k != best)
            .map(|k| scores[k])
            .fold(keep, f64::max);
        Some(Fix {
            plain: cands[best].0.clone(),
            margin: scores[best] - runner,
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
