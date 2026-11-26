//! Restores Vietnamese diacritics from a syllable n-gram table: `toi da go phim` → `tôi đã gõ phím`.
//!
//! The table is built by `research.table`. Restoration matches `research.model` exactly on the same
//! table: 3-grams with stupid backoff, costlier from contexts the table kept nearly whole, and a
//! Viterbi search over the whole run. [`Typing`] restores a run while it is typed; [`Personal`] adds
//! counts learned from the user's own text, and [`Dictionary`] keeps one for every field, read
//! through snapshots. [`Run`] says what to change in the field when a word ends.

mod dictionary;
mod personal;
mod run;
mod shared;
mod table;
mod tone;
mod typing;

pub use dictionary::{Dictionary, SaveJob, SyncJob};
pub use personal::{MAX_WEIGHT, Personal};
pub use run::{Change, MAX_RUN, Reach, Run};
pub use table::{Error, Table};
pub use tone::ToneStyle;
pub use typing::{Choices, FIX_MARGIN, Fix, SENSITIVE_CONFIDENCE, Typing};

use table::BOS;

/// Log weight of each step down to a shorter n-gram (stupid backoff, 0.4).
fn backoff() -> f64 {
    0.4f64.ln()
}

/// Extra backoff cost per unit of the log share of a context's count that the table's longer n-grams
/// leave uncovered: backing off from a context that kept almost everything costs more.
const LEFTOVER: f64 = 0.5;

impl Table {
    /// Restores a run of syllables typed without marks, such as the words between two punctuation
    /// marks. Each output word keeps the capitalization of its input; words the table does not know
    /// come back as typed.
    pub fn restore(&self, run: &[&str], style: ToneStyle) -> Vec<String> {
        let mut typing = Typing::new(self);
        for syllable in run {
            typing.push(syllable);
        }
        typing.finish(style)
    }
}

impl Personal<'_> {
    /// Restores a run like [`Table::restore`], with the learned counts on top of the table's.
    pub fn restore(&self, run: &[&str], style: ToneStyle) -> Vec<String> {
        let mut typing = Typing::with_personal(self);
        for syllable in run {
            typing.push(syllable);
        }
        typing.finish(style)
    }
}

/// N-gram counts a run is scored with: a table, or a table with a personal dictionary.
pub(crate) trait Counts {
    fn trigram(&self, u: u32, v: u32, w: u32) -> f64;
    fn bigram(&self, v: u32, w: u32) -> f64;
    fn uni(&self, id: u32) -> f64;
    fn segments(&self) -> f64;
    fn total(&self) -> f64;
    /// Log unkept share of context `u v` and of word `v`; see `Table::left`.
    fn left(&self, u: u32, v: u32) -> f64;
    fn left_uni(&self, v: u32) -> f64;
    /// The id a word is looked up by when it is a context.
    fn context(&self, id: u32) -> u32;
    /// The id a word is looked up by when it is the word being scored, and the log share it gets of
    /// that id's score.
    fn predicted(&self, id: u32) -> (u32, f64);

    /// Log score of `w` after `u v`, as `Model.logp` computes it.
    fn logp(&self, u: u32, v: u32, w: u32) -> f64 {
        self.logp_left(u, v, w, self.context_left(u, v))
    }

    /// The part of the backoff cost that depends only on `u v`, so a search can compute it once per
    /// pair instead of once per candidate `w`.
    fn context_left(&self, u: u32, v: u32) -> f64 {
        self.left(self.context(u), self.context(v))
    }

    /// `logp` with `context_left(u, v)` given.
    fn logp_left(&self, u: u32, v: u32, w: u32, left: f64) -> f64 {
        let (w, share) = self.predicted(w);
        share + self.logp_of(self.context(u), self.context(v), w, left)
    }

    fn logp_of(&self, u: u32, v: u32, w: u32, left: f64) -> f64 {
        let tri = self.trigram(u, v, w);
        if tri > 0.0 {
            let ctx = if u == BOS && v == BOS {
                self.segments()
            } else {
                self.bigram(u, v)
            };
            return (tri.max(1e-9) / ctx.max(1.0)).ln();
        }
        let bi = self.bigram(v, w);
        if bi > 0.0 {
            return (bi.max(1e-9) / self.uni(v).max(1.0)).ln() + backoff() + LEFTOVER * left;
        }
        (self.uni(w).max(0.5) / self.total()).ln()
            + 2.0 * backoff()
            + LEFTOVER * left
            + LEFTOVER * self.left_uni(v)
    }
}

macro_rules! counts {
    ($t:ty) => {
        impl Counts for $t {
            fn trigram(&self, u: u32, v: u32, w: u32) -> f64 {
                <$t>::trigram(self, u, v, w)
            }
            fn bigram(&self, v: u32, w: u32) -> f64 {
                <$t>::bigram(self, v, w)
            }
            fn uni(&self, id: u32) -> f64 {
                <$t>::uni(self, id)
            }
            fn segments(&self) -> f64 {
                <$t>::segments(self)
            }
            fn total(&self) -> f64 {
                <$t>::total(self)
            }
            fn left(&self, u: u32, v: u32) -> f64 {
                <$t>::left(self, u, v)
            }
            fn left_uni(&self, v: u32) -> f64 {
                <$t>::left_uni(self, v)
            }
            fn context(&self, id: u32) -> u32 {
                <$t>::context(self, id)
            }
            fn predicted(&self, id: u32) -> (u32, f64) {
                <$t>::predicted(self, id)
            }
        }
    };
}

counts!(Table);
counts!(Personal<'_>);
