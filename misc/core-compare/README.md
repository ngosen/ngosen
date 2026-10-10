# core-compare

Types the same key sequences into the Go bamboo-core in `bamboo/` and the Rust bamboo-core that
`bamboo-rs/` uses ([nguyen10t2/bamboo_core](https://github.com/nguyen10t2/bamboo_core), same revision),
and reports where the results differ. It is a check for replacing the Go core; nothing here is
built or installed with the addon.

```sh
misc/core-compare/run.sh /tmp/core-compare
```

Needs Go, Cargo with network access (to fetch the crate from GitHub) and Python 3.

- `cases.py` writes the cases: every syllable in `data/dictionaries/vietnamese.cm.dict` typed in
  Telex and VNI (marks inline or last, tone last or right after the vowels, capitalised, all caps,
  one backspace and retype, restore), the inputs of `bamboo/bamboo-core/bamboo_test.go`, a few
  English words and random key sequences. Each runs with old and new tone placement and with free
  marking on and off.
- `go/` and `rust/` run the cases and print the engine state after every key. Both choose
  Vietnamese or English mode per key and compute the preedit and the committed word the way
  `bamboo/fcitxbambooengine.go` does with default settings.
- `compare.py` splits the differing cases into three levels: the committed word differs, only the
  preedit while typing differs, or only engine state the user does not see differs.

The Rust runner keeps the crate's own spelling correction off, as `bamboo-rs/` does. After
`run.sh OUT`, running `OUT/target/release/core-compare DICT --auto-correct <OUT/cases.tsv` turns it
on.

The Rust runner leaves "w types ư" off, so the cases turn it off for Go too.
