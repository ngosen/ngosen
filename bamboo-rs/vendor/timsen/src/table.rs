//! Reader for the `.timsen` table written by `research.table`; the layout is documented there.
//!
//! Every section is read in place: opening a table checks the header and the string sections, and
//! lookups decode little-endian numbers straight from the bytes.

use std::fmt;
use std::ops::Range;
use std::path::Path;

pub const BOS: u32 = 0;
pub const EOS: u32 = 1;
pub const UNK: u32 = 2;
/// Bits per word id in packed n-gram keys.
pub const SHIFT: u32 = 21;

const MAGIC: &[u8; 8] = b"TIMSEN\0\0";
const VERSION: u32 = 3;
/// N-gram sections store the last word id in 16 bits.
const MAX_WORDS: usize = 1 << 16;
const HEADER_LEN: usize = 64;
/// Counts and leftovers are stored as one of this many levels.
const LEVELS: usize = 256;
/// A trigram group at least this long has its length in the long list.
const LONG: usize = 255;
/// Trigram groups per stored base.
const BLOCK: usize = 32;
/// Most spellings one unmarked form may have, far above what real tables use. Typing keeps a score
/// for every pair of neighbouring spellings, so this bounds its memory per syllable.
pub(crate) const MAX_SPELLINGS: usize = 256;

#[derive(Debug)]
pub enum Error {
    Io(std::io::Error),
    Format(&'static str),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::Io(e) => write!(f, "cannot read file: {e}"),
            Error::Format(why) => write!(f, "not a valid file: {why}"),
        }
    }
}

impl std::error::Error for Error {}

impl From<std::io::Error> for Error {
    fn from(e: std::io::Error) -> Self {
        Error::Io(e)
    }
}

/// Byte positions of a string section: `n + 1` u32 offsets, then the UTF-8 blob.
struct Strings {
    offsets: usize,
    blob: usize,
    n: usize,
}

/// Byte positions of the bigram section: `n_words + 1` u32 starts, then per bigram a u16 last-word
/// id ascending within each first word, a count code and a leftover code.
struct Bigrams {
    start: usize,
    groups: usize,
    next: usize,
    code: usize,
    left: usize,
    n: usize,
}

/// Byte positions of the trigram section. Group `j` holds the trigrams after bigram `j`, and the
/// last group those after (`<s>`, `<s>`). A group starts at the base of its block plus the lengths
/// of the groups before it in the block; a length byte of `LONG` stands for an entry of the long list.
struct Trigrams {
    base: usize,
    length: usize,
    long_group: usize,
    long_length: usize,
    n_long: usize,
    next: usize,
    code: usize,
}

pub struct Table {
    bytes: Vec<u8>,
    total: f64,
    words: Strings,
    uni: usize,
    /// The count each code stands for.
    counts: [f64; LEVELS],
    /// The log unkept share each leftover code stands for.
    lefts: [f64; LEVELS],
    /// Log unkept share of the (`<s>`, `<s>`) context.
    left_bos: f64,
    /// Per word, the log unkept share of its count.
    left_uni: usize,
    bigrams: Bigrams,
    trigrams: Trigrams,
    plain: Strings,
    cand_start: usize,
    cand_ids: usize,
}

struct Cursor<'a> {
    bytes: &'a [u8],
    pos: usize,
}

impl Cursor<'_> {
    /// Claims `len` bytes and returns where they start; sections are padded to 8 bytes.
    fn take(&mut self, len: usize) -> Result<usize, Error> {
        let start = self.pos;
        let end = start
            .checked_add(len)
            .ok_or(Error::Format("section too large"))?;
        if end > self.bytes.len() {
            return Err(Error::Format("file is truncated"));
        }
        self.pos = end.next_multiple_of(8).min(self.bytes.len());
        Ok(start)
    }

    /// Claims `n` items of `size` bytes each.
    fn take_n(&mut self, n: usize, size: usize) -> Result<usize, Error> {
        let len = n
            .checked_mul(size)
            .ok_or(Error::Format("section too large"))?;
        self.take(len)
    }

    fn strings(&mut self, n: usize) -> Result<Strings, Error> {
        let offsets = self.take_n(
            n.checked_add(1).ok_or(Error::Format("section too large"))?,
            4,
        )?;
        let blob_len = u32_at(self.bytes, offsets, n) as usize;
        let blob = self.take(blob_len)?;
        let mut last = 0;
        for i in 0..=n {
            let off = u32_at(self.bytes, offsets, i) as usize;
            if off < last || off > blob_len {
                return Err(Error::Format("string offsets out of order"));
            }
            last = off;
        }
        let s = Strings { offsets, blob, n };
        for i in 0..n {
            std::str::from_utf8(s.bytes(self.bytes, i))
                .map_err(|_| Error::Format("string is not UTF-8"))?;
        }
        Ok(s)
    }

    /// `LEVELS` values that all pass `ok`.
    fn levels(&mut self, ok: fn(f64) -> bool) -> Result<[f64; LEVELS], Error> {
        let pos = self.take_n(LEVELS, 8)?;
        let mut out = [0.0; LEVELS];
        for (i, v) in out.iter_mut().enumerate() {
            *v = f64_at(self.bytes, pos, i);
            if !ok(*v) {
                return Err(Error::Format("bad level"));
            }
        }
        Ok(out)
    }

    fn bigrams(&mut self, groups: usize, n: usize) -> Result<Bigrams, Error> {
        let start = self.take_n(
            groups
                .checked_add(1)
                .ok_or(Error::Format("section too large"))?,
            4,
        )?;
        let mut last = 0;
        for i in 0..=groups {
            let s = u32_at(self.bytes, start, i) as usize;
            if s < last || s > n || (i == 0 && s != 0) || (i == groups && s != n) {
                return Err(Error::Format("n-gram groups out of order"));
            }
            last = s;
        }
        Ok(Bigrams {
            start,
            groups,
            next: self.take_n(n, 2)?,
            code: self.take_n(n, 1)?,
            left: self.take_n(n, 1)?,
            n,
        })
    }

    fn trigrams(&mut self, groups: usize, n_long: usize, n: usize) -> Result<Trigrams, Error> {
        let t = Trigrams {
            base: self.take_n(groups / BLOCK + 1, 4)?,
            length: self.take_n(groups, 1)?,
            long_group: self.take_n(n_long, 4)?,
            long_length: self.take_n(n_long, 4)?,
            n_long,
            next: self.take_n(n, 2)?,
            code: self.take_n(n, 1)?,
        };
        // Lookups add lengths up from a base without checking, so every base and every long length
        // must be what the lengths before it add up to.
        let (mut at, mut long) = (0usize, 0usize);
        for j in 0..=groups {
            if j % BLOCK == 0 && u32_at(self.bytes, t.base, j / BLOCK) as usize != at {
                return Err(Error::Format("trigram groups do not add up"));
            }
            if j == groups {
                break;
            }
            let mut len = usize::from(self.bytes[t.length + j]);
            if len == LONG {
                if long == n_long || u32_at(self.bytes, t.long_group, long) as usize != j {
                    return Err(Error::Format("long trigram group is not listed"));
                }
                len = u32_at(self.bytes, t.long_length, long) as usize;
                if len < LONG {
                    return Err(Error::Format("long trigram group is short"));
                }
                long += 1;
            }
            // `at` never passes `n`, so the subtraction cannot overflow; adding first could.
            if len > n - at {
                return Err(Error::Format("trigram groups do not add up"));
            }
            at += len;
        }
        if at != n || long != n_long {
            return Err(Error::Format("trigram groups do not add up"));
        }
        Ok(t)
    }
}

impl Strings {
    fn bytes<'a>(&self, bytes: &'a [u8], i: usize) -> &'a [u8] {
        let a = u32_at(bytes, self.offsets, i) as usize;
        let b = u32_at(bytes, self.offsets, i + 1) as usize;
        &bytes[self.blob + a..self.blob + b]
    }
}

fn u32_at(bytes: &[u8], pos: usize, i: usize) -> u32 {
    let p = pos + 4 * i;
    u32::from_le_bytes(bytes[p..p + 4].try_into().unwrap())
}

fn u16_at(bytes: &[u8], pos: usize, i: usize) -> u16 {
    let p = pos + 2 * i;
    u16::from_le_bytes(bytes[p..p + 2].try_into().unwrap())
}

fn f64_at(bytes: &[u8], pos: usize, i: usize) -> f64 {
    let p = pos + 8 * i;
    f64::from_le_bytes(bytes[p..p + 8].try_into().unwrap())
}

fn u64_at(bytes: &[u8], pos: usize, i: usize) -> u64 {
    let p = pos + 8 * i;
    u64::from_le_bytes(bytes[p..p + 8].try_into().unwrap())
}

/// Every unmarked form has 1 to `MAX_SPELLINGS` distinct spellings, in ranges that tile the ids.
fn check_candidates(
    bytes: &[u8],
    start: usize,
    ids: usize,
    n_plain: usize,
    n_cand: usize,
    n_words: usize,
) -> Result<(), Error> {
    if u32_at(bytes, start, 0) != 0 || u32_at(bytes, start, n_plain) as usize != n_cand {
        return Err(Error::Format("candidate ranges do not cover the ids"));
    }
    let mut seen = Vec::with_capacity(MAX_SPELLINGS);
    for i in 0..n_plain {
        let (a, b) = (
            u32_at(bytes, start, i) as usize,
            u32_at(bytes, start, i + 1) as usize,
        );
        if b <= a || b - a > MAX_SPELLINGS || b > n_cand {
            return Err(Error::Format("bad number of spellings"));
        }
        seen.clear();
        for j in a..b {
            let id = u32_at(bytes, ids, j);
            if id as usize >= n_words {
                return Err(Error::Format("candidate id out of bounds"));
            }
            if seen.contains(&id) {
                return Err(Error::Format("repeated spelling"));
            }
            seen.push(id);
        }
    }
    Ok(())
}

fn header_u64(bytes: &[u8], i: usize) -> Result<usize, Error> {
    usize::try_from(u64_at(bytes, 16, i)).map_err(|_| Error::Format("count too large"))
}

impl Table {
    /// Reads a table file into memory, so a file rewritten during an update cannot change it.
    pub fn open(path: impl AsRef<Path>) -> Result<Table, Error> {
        Table::from_bytes(std::fs::read(path)?)
    }

    /// Reads a table already in memory.
    pub fn from_bytes(bytes: Vec<u8>) -> Result<Table, Error> {
        if bytes.len() < HEADER_LEN || &bytes[..8] != MAGIC {
            return Err(Error::Format("missing magic number"));
        }
        if u32_at(&bytes, 8, 0) != VERSION {
            return Err(Error::Format("unsupported version"));
        }
        let [n_words, n_bi, n_tri, n_plain, n_cand] =
            [0, 1, 2, 3, 4].map(|i| header_u64(&bytes, i));
        let (n_words, n_bi, n_tri, n_plain, n_cand) = (n_words?, n_bi?, n_tri?, n_plain?, n_cand?);
        if !(3..=MAX_WORDS).contains(&n_words) {
            return Err(Error::Format("bad vocabulary size"));
        }
        let total = f64::from_le_bytes(bytes[56..64].try_into().unwrap());
        let n_long = u32_at(&bytes, 12, 0) as usize;
        let mut c = Cursor {
            bytes: &bytes,
            pos: HEADER_LEN,
        };
        let words = c.strings(n_words)?;
        let uni = c.take_n(n_words, 4)?;
        let counts = c.levels(|v| v.is_finite() && v > 0.0)?;
        let lefts = c.levels(|v| v.is_finite() && v <= 0.0)?;
        let left_bos = f64_at(&bytes, c.take(8)?, 0);
        let left_uni = c.take_n(n_words, 8)?;
        let shares = (0..n_words).map(|i| f64_at(&bytes, left_uni, i));
        if !std::iter::once(left_bos)
            .chain(shares)
            .all(|v| v.is_finite() && v <= 0.0)
        {
            return Err(Error::Format("bad leftover"));
        }
        let bigrams = c.bigrams(n_words, n_bi)?;
        let groups = n_bi
            .checked_add(1)
            .ok_or(Error::Format("section too large"))?;
        let trigrams = c.trigrams(groups, n_long, n_tri)?;
        let plain = c.strings(n_plain)?;
        // n_plain + 1 cannot overflow: the plain strings section above already needed it.
        let cand_start = c.take_n(n_plain + 1, 4)?;
        let cand_ids = c.take_n(n_cand, 4)?;
        check_candidates(&bytes, cand_start, cand_ids, n_plain, n_cand, n_words)?;
        Ok(Table {
            bytes,
            total,
            words,
            uni,
            counts,
            lefts,
            left_bos,
            left_uni,
            bigrams,
            trigrams,
            plain,
            cand_start,
            cand_ids,
        })
    }

    /// Log unkept share of context `u v`; a context the table dropped says nothing, so it is 0.
    pub(crate) fn left(&self, u: u32, v: u32) -> f64 {
        if u == BOS && v == BOS {
            return self.left_bos;
        }
        self.bigram_index(u, v).map_or(0.0, |j| {
            self.lefts[usize::from(self.bytes[self.bigrams.left + j])]
        })
    }

    /// Log unkept share of word `v`'s count, as the context of a bigram.
    pub(crate) fn left_uni(&self, v: u32) -> f64 {
        if (v as usize) < self.len() {
            f64_at(&self.bytes, self.left_uni, v as usize)
        } else {
            0.0
        }
    }

    /// Number of words, counting `<s>`, `</s>` and `<unk>`.
    pub(crate) fn len(&self) -> usize {
        self.words.n
    }

    /// Every id of the table is its own context; see `Personal::context`.
    pub(crate) fn context(&self, id: u32) -> u32 {
        id
    }

    pub(crate) fn predicted(&self, id: u32) -> (u32, f64) {
        (id, 0.0)
    }

    /// The spelling of a word id; ids come from the table itself, so they are in range.
    pub(crate) fn word(&self, id: u32) -> &str {
        std::str::from_utf8(self.words.bytes(&self.bytes, id as usize)).unwrap()
    }

    /// Unigram count; `<s>` counts as often as `</s>`, one per run.
    pub(crate) fn uni(&self, id: u32) -> f64 {
        let id = if id == BOS { EOS } else { id };
        f64::from(u32_at(&self.bytes, self.uni, id as usize))
    }

    pub(crate) fn total(&self) -> f64 {
        self.total
    }

    pub(crate) fn segments(&self) -> f64 {
        self.uni(EOS)
    }

    pub(crate) fn bigram(&self, v: u32, w: u32) -> f64 {
        self.bigram_index(v, w).map_or(0.0, |i| {
            self.counts[usize::from(self.bytes[self.bigrams.code + i])]
        })
    }

    pub(crate) fn trigram(&self, u: u32, v: u32, w: u32) -> f64 {
        let ctx = if u == BOS && v == BOS {
            Some(self.bigrams.n)
        } else {
            self.bigram_index(u, v)
        };
        let t = &self.trigrams;
        ctx.and_then(|j| self.find(t.next, self.trigram_range(j), w))
            .map_or(0.0, |i| self.counts[usize::from(self.bytes[t.code + i])])
    }

    fn bigram_index(&self, v: u32, w: u32) -> Option<usize> {
        let b = &self.bigrams;
        let group = v as usize;
        if group >= b.groups {
            return None;
        }
        // The starts were checked to be in order and within the section.
        let range = (
            u32_at(&self.bytes, b.start, group) as usize,
            u32_at(&self.bytes, b.start, group + 1) as usize,
        );
        self.find(b.next, range, w)
    }

    /// Length of trigram group `j`; every long group was checked to be in the long list.
    fn trigram_len(&self, j: usize) -> usize {
        let t = &self.trigrams;
        let len = usize::from(self.bytes[t.length + j]);
        if len < LONG {
            return len;
        }
        let (mut lo, mut hi) = (0, t.n_long);
        while lo < hi {
            let mid = lo + (hi - lo) / 2;
            match (u32_at(&self.bytes, t.long_group, mid) as usize).cmp(&j) {
                std::cmp::Ordering::Less => lo = mid + 1,
                std::cmp::Ordering::Greater => hi = mid,
                std::cmp::Ordering::Equal => {
                    return u32_at(&self.bytes, t.long_length, mid) as usize;
                }
            }
        }
        0
    }

    /// Positions of trigram group `j`, which is below `groups`.
    fn trigram_range(&self, j: usize) -> (usize, usize) {
        let first = j - j % BLOCK;
        let base = u32_at(&self.bytes, self.trigrams.base, j / BLOCK) as usize;
        let start = base + (first..j).map(|i| self.trigram_len(i)).sum::<usize>();
        (start, start + self.trigram_len(j))
    }

    /// Where `w` is among the ascending ids at `next`, between the two positions of `range`.
    fn find(&self, next: usize, range: (usize, usize), w: u32) -> Option<usize> {
        let w = u16::try_from(w).ok()?;
        let (mut lo, mut hi) = range;
        while lo < hi {
            let mid = lo + (hi - lo) / 2;
            match u16_at(&self.bytes, next, mid).cmp(&w) {
                std::cmp::Ordering::Less => lo = mid + 1,
                std::cmp::Ordering::Greater => hi = mid,
                std::cmp::Ordering::Equal => return Some(mid),
            }
        }
        None
    }

    /// Ids of the spellings of an unmarked syllable, or `None` if the table does not know it.
    pub(crate) fn options(&self, plain: &str) -> Option<impl ExactSizeIterator<Item = u32> + '_> {
        let (mut lo, mut hi) = (0, self.plain.n);
        while lo < hi {
            let mid = lo + (hi - lo) / 2;
            match self.plain.bytes(&self.bytes, mid).cmp(plain.as_bytes()) {
                std::cmp::Ordering::Less => lo = mid + 1,
                std::cmp::Ordering::Greater => hi = mid,
                std::cmp::Ordering::Equal => {
                    let range: Range<usize> = u32_at(&self.bytes, self.cand_start, mid) as usize
                        ..u32_at(&self.bytes, self.cand_start, mid + 1) as usize;
                    return Some(range.map(|i| u32_at(&self.bytes, self.cand_ids, i)));
                }
            }
        }
        None
    }
}
