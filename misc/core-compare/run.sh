#!/bin/sh
# Build both runners, feed them the same cases and compare.
# Usage: run.sh OUT_DIR [cases.py options]
set -eu
here=$(cd "$(dirname "$0")" && pwd)
out=$1
shift
mkdir -p "$out"
dict="$here/../../data/dictionaries/vietnamese.cm.dict"

(cd "$here/go" && go build -o "$out/go-runner" .)
cargo build --quiet --release --manifest-path "$here/rust/Cargo.toml" --target-dir "$out/target"

python3 "$here/cases.py" "$@" >"$out/cases.tsv"
"$out/go-runner" "$dict" <"$out/cases.tsv" >"$out/go.tsv"
"$out/target/release/core-compare" "$dict" <"$out/cases.tsv" >"$out/rust.tsv"
python3 "$here/compare.py" "$out/cases.tsv" "$out/go.tsv" "$out/rust.tsv" --diffs "$out/diffs.tsv"
