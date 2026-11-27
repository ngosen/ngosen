//! A damaged table file must give an `Error` when opened, or else restore without panicking.

use proptest::prelude::*;
use timsen::{Table, ToneStyle, Typing};

fn fixture() -> Vec<u8> {
    std::fs::read(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/tests/fixtures/tiny.timsen"
    ))
    .unwrap()
}

/// A table where one trigram group is long enough to be in the long list.
fn long_fixture() -> Vec<u8> {
    std::fs::read(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/tests/fixtures/long.timsen"
    ))
    .unwrap()
}

fn u64_at(b: &[u8], pos: usize) -> usize {
    u64::from_le_bytes(b[pos..pos + 8].try_into().unwrap()) as usize
}

fn u32_at(b: &[u8], pos: usize) -> usize {
    u32::from_le_bytes(b[pos..pos + 4].try_into().unwrap()) as usize
}

fn put_u32(b: &mut [u8], pos: usize, v: u32) {
    b[pos..pos + 4].copy_from_slice(&v.to_le_bytes());
}

fn pad(x: usize) -> usize {
    x.next_multiple_of(8)
}

fn strings(b: &[u8], pos: usize, n: usize) -> usize {
    pad(pad(pos + 4 * (n + 1)) + u32_at(b, pos + 4 * n))
}

/// Where the levels are, following the layout in `research/table.py`.
fn levels(b: &[u8]) -> usize {
    let n_words = u64_at(b, 16);
    pad(strings(b, 64, n_words) + 4 * n_words)
}

/// Where the bigram starts are.
fn bigram_starts(b: &[u8]) -> usize {
    levels(b) + 8 * (2 * 256 + 1) + 8 * u64_at(b, 16)
}

/// Where the trigram bases, lengths, long groups and long lengths are.
fn trigram_groups(b: &[u8]) -> [usize; 4] {
    let (n_words, n_bi, n_long) = (u64_at(b, 16), u64_at(b, 24), u32_at(b, 12));
    let base = pad(pad(pad(pad(bigram_starts(b) + 4 * (n_words + 1)) + 2 * n_bi) + n_bi) + n_bi);
    let length = pad(base + 4 * ((n_bi + 1) / 32 + 1));
    let long_group = pad(length + n_bi + 1);
    [base, length, long_group, pad(long_group + 4 * n_long)]
}

/// Where the candidate ranges and ids start.
fn candidates(b: &[u8]) -> (usize, usize, usize) {
    let (n_tri, n_plain, n_long) = (u64_at(b, 32), u64_at(b, 40), u32_at(b, 12));
    let mut pos = pad(trigram_groups(b)[3] + 4 * n_long);
    pos = pad(pad(pos + 2 * n_tri) + n_tri);
    pos = strings(b, pos, n_plain);
    (pos, pad(pos + 4 * (n_plain + 1)), n_plain)
}

/// Restores and types a few runs, touching every lookup.
fn exercise(t: &Table) {
    let _ = t.restore(&["toi", "go", "phim", "rat", "nhanh"], ToneStyle::New);
    let mut typing = Typing::new(t);
    for s in ["nha", "ta", "co", "go", "quy", "xyz"] {
        typing.push(s);
        let _ = typing.guess(ToneStyle::Old);
        let _ = typing.choices(3, false, ToneStyle::New);
    }
}

#[test]
fn the_fixture_layout_is_understood() {
    let b = fixture();
    let (start, ids, n_plain) = candidates(&b);
    assert_eq!(u32_at(&b, start), 0);
    assert_eq!(u32_at(&b, start + 4 * n_plain), u64_at(&b, 48));
    assert_eq!((ids + 4 * u64_at(&b, 48)).next_multiple_of(8), b.len());
    let b = long_fixture();
    let (_, ids, _) = candidates(&b);
    assert_eq!((ids + 4 * u64_at(&b, 48)).next_multiple_of(8), b.len());
    let [_, length, long_group, long_length] = trigram_groups(&b);
    assert!(u32_at(&b, 12) >= 1);
    assert_eq!(b[length + u32_at(&b, long_group)], 255);
    assert!(u32_at(&b, long_length) >= 255);
}

#[test]
fn levels_that_are_not_numbers_are_errors() {
    let at = levels(&fixture());
    let n_words = u64_at(&fixture(), 16);
    // A count level, a leftover level, the leftover of the start and the leftover of a word.
    for i in [3, 256 + 3, 512, 513 + n_words - 1] {
        for v in [f64::NAN, f64::INFINITY, f64::NEG_INFINITY] {
            let mut b = fixture();
            b[at + 8 * i..at + 8 * i + 8].copy_from_slice(&v.to_le_bytes());
            assert!(Table::from_bytes(b).is_err(), "level {i} = {v}");
        }
    }
    // A count is above 0, and a leftover is the log of a share, so it is never above 0.
    for (i, v) in [(3, 0.0), (3, -1.0), (256 + 3, 0.5), (512, 0.5)] {
        let mut b = fixture();
        b[at + 8 * i..at + 8 * i + 8].copy_from_slice(&f64::to_le_bytes(v));
        assert!(Table::from_bytes(b).is_err(), "level {i} = {v}");
    }
}

#[test]
fn trigram_groups_that_do_not_add_up_are_errors() {
    let b = long_fixture();
    let [base, length, long_group, long_length] = trigram_groups(&b);
    let group = u32_at(&b, long_group);
    let (n_bi, n_tri) = (u64_at(&b, 24), u64_at(&b, 32));
    let mut cases: Vec<(&str, Vec<u8>)> = Vec::new();
    let mut put = |name, pos: usize, bytes: &[u8]| {
        let mut c = b.clone();
        c[pos..pos + bytes.len()].copy_from_slice(bytes);
        cases.push((name, c));
    };
    put(
        "a base that is off by one",
        base + 4,
        &(u32_at(&b, base + 4) as u32 + 1).to_le_bytes(),
    );
    put("a first base that is not 0", base, &1u32.to_le_bytes());
    put("a group one longer", length, &[b[length] + 1]);
    put("a long group that is not listed", length + group, &[254]);
    put(
        "a listed group that is not long",
        long_group,
        &(group as u32 + 1).to_le_bytes(),
    );
    put(
        "a long length that is short",
        long_length,
        &7u32.to_le_bytes(),
    );
    put(
        "a long length past the section",
        long_length,
        &u32::MAX.to_le_bytes(),
    );
    put("more long groups than listed", 12, &0u32.to_le_bytes());
    put(
        "fewer trigrams than the groups hold",
        32,
        &(n_tri as u64 - 1).to_le_bytes(),
    );
    let short = (0..n_bi + 1).find(|&j| b[length + j] < 254).unwrap();
    put("a short group marked long", length + short, &[255]);
    for (name, bytes) in cases {
        assert!(Table::from_bytes(bytes).is_err(), "{name}");
    }
}

#[test]
fn huge_section_counts_are_errors() {
    for field in [16, 24, 32, 40, 48] {
        for n in [u64::MAX, u64::MAX / 4, 1 << 62, 1 << 61, (1 << 62) + 1] {
            let mut b = fixture();
            b[field..field + 8].copy_from_slice(&n.to_le_bytes());
            assert!(Table::from_bytes(b).is_err(), "field {field} = {n}");
        }
    }
}

#[test]
fn n_gram_groups_out_of_order_are_errors() {
    let start = bigram_starts(&fixture());
    let n_words = u64_at(&fixture(), 16);
    // A group start beyond the section, a first start that is not 0, and a group that ends early.
    for (i, v) in [(5, u32::MAX), (0, 1), (n_words, 0)] {
        let mut b = fixture();
        put_u32(&mut b, start + 4 * i, v);
        assert!(Table::from_bytes(b).is_err(), "start {i} = {v}");
    }
}

#[test]
fn a_form_without_spellings_is_an_error() {
    let mut b = fixture();
    let (start, _, _) = candidates(&b);
    let third = u32_at(&b, start + 8);
    put_u32(&mut b, start + 4, 0);
    assert!(Table::from_bytes(b.clone()).is_err());
    // A range that runs backwards is as bad.
    put_u32(&mut b, start + 4, third as u32 + 1);
    assert!(Table::from_bytes(b).is_err());
}

#[test]
fn a_repeated_spelling_is_an_error() {
    let mut b = fixture();
    let (start, ids, n_plain) = candidates(&b);
    let form = (0..n_plain)
        .find(|&i| u32_at(&b, start + 4 * (i + 1)) - u32_at(&b, start + 4 * i) >= 2)
        .unwrap();
    let first = u32_at(&b, start + 4 * form);
    let id = u32_at(&b, ids + 4 * first) as u32;
    put_u32(&mut b, ids + 4 * (first + 1), id);
    assert!(Table::from_bytes(b).is_err());
}

proptest! {
    #[test]
    fn damaged_bytes_never_panic(flips in prop::collection::vec((any::<prop::sample::Index>(), any::<u8>()), 1..8)) {
        let mut b = fixture();
        for (at, byte) in flips {
            let i = at.index(b.len());
            b[i] = byte;
        }
        if let Ok(t) = Table::from_bytes(b) {
            exercise(&t);
        }
    }

    #[test]
    fn damaged_trigram_groups_never_panic(flips in prop::collection::vec((any::<prop::sample::Index>(), any::<u8>()), 1..8)) {
        let mut b = long_fixture();
        let [base, _, _, long_length] = trigram_groups(&b);
        let len = long_length + 8 - base;
        for (at, byte) in flips {
            b[base + at.index(len)] = byte;
        }
        if let Ok(t) = Table::from_bytes(b) {
            let _ = t.restore(&["ta", "co", "ba", "anh", "thay", "bin"], ToneStyle::New);
            exercise(&t);
        }
    }
}
