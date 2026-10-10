#!/bin/bash
# Fails when a change adds a file, or declares a type, constant, macro or function, whose name contains
# "lotus". The project is Ngó Sen; the lotus names left are legacy names users' machines still have.
#
#   misc/check-new-names.sh [base]   compare HEAD with base (default origin/main)
#   misc/check-new-names.sh --self-test
set -euo pipefail

# A declaration whose own name contains "lotus": the word after class/struct/enum/namespace/using/
# #define/constexpr, or a function or variable name preceded by a type.
decl='^\+[[:space:]]*((class|struct|enum([[:space:]]+class)?|namespace|using|#[[:space:]]*define)[[:space:]]+[[:alnum:]_]*lotus|(static[[:space:]]+|inline[[:space:]]+|const[[:space:]]+)*constexpr[[:space:]]+[^=(]*[[:space:]][[:alnum:]_]*lotus[[:alnum:]_]*[[:space:]]*=|([[:alpha:]_][[:alnum:]_:<>,*&]*[[:space:]]+)+[*&]?[[:alnum:]_]*lotus[[:alnum:]_]*[[:space:]]*\()'
# Statements that put a word before a call without declaring anything.
notDecl='^\+[[:space:]]*(return|else|throw|case|delete|new|co_return|co_await)[[:space:]]'

check() {
    local base=$1 status=0
    local files
    files=$(git diff --name-only --diff-filter=AR "$base"...HEAD | grep -i 'lotus' || true)
    if [ -n "$files" ]; then
        echo "New files must not be named lotus:"
        echo "$files" | sed 's/^/  /'
        status=1
    fi
    local diff decls removed line word kept
    diff=$(git diff -U0 "$base"...HEAD -- '*.c' '*.cpp' '*.h' '*.hpp')
    decls=$(echo "$diff" | grep -iE "$decl" | grep -vE "$notDecl" | grep -v '^+++' || true)
    # A declaration rewritten in place, such as a class gaining a base, keeps its old name: drop added
    # lines whose lotus names all come from a removed declaration.
    removed=$(echo "$diff" | sed -n 's/^-//p' | grep -v '^--' | sed 's/^/+/' | grep -iE "$decl" | grep -oiE '[[:alnum:]_]*lotus[[:alnum:]_]*' | sort -u || true)
    kept=""
    while IFS= read -r line; do
        [ -z "$line" ] && continue
        for word in $(echo "$line" | grep -oiE '[[:alnum:]_]*lotus[[:alnum:]_]*' | sort -u); do
            if ! grep -qxF "$word" <<< "$removed"; then
                kept+="$line"$'\n'
                break
            fi
        done
    done <<< "$decls"
    decls=${kept%$'\n'}
    if [ -n "$decls" ]; then
        echo "New declarations must not be named lotus (calling existing ones is fine):"
        echo "$decls" | sed 's/^/  /'
        status=1
    fi
    return $status
}

selfTest() {
    # A git hook may export GIT_DIR, which would point these commands at the real repository.
    unset GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE
    local dir
    dir=$(mktemp -d)
    trap 'rm -rf "$dir"' RETURN
    git -C "$dir" init -q
    git -C "$dir" config user.email t@t
    git -C "$dir" config user.name t
    printf 'void LotusState::reset();\nclass LotusEngine : public Base {};\n' > "$dir/old.cpp"
    git -C "$dir" add . && git -C "$dir" commit -qm base
    local base
    base=$(git -C "$dir" rev-parse HEAD)
    local failed=0
    expect() {
        local want=$1 label=$2
        if (cd "$dir" && check "$base" > /dev/null); then got=pass; else got=fail; fi
        if [ "$got" != "$want" ]; then
            echo "self-test: $label: expected $want, got $got"
            failed=1
        fi
        git -C "$dir" reset -q --hard "$base"
    }
    # Uses of existing names pass.
    printf 'void LotusState::selectAndOvertype(int n) {\n    LOTUS_INFO("x");\n    fcitx::LotusEngine engine(&i);\n    add_lotus_headless_test(t)\n    return lotusFoo(x);\n    if (lotusReady(ic)) {}\n}\n' > "$dir/new.cpp"
    git -C "$dir" add . && git -C "$dir" commit -qm uses
    expect pass "calling existing lotus names"
    printf 'bool helper();\n' > "$dir/lotus-new.cpp"
    git -C "$dir" add . && git -C "$dir" commit -qm file
    expect fail "new file named lotus"
    for line in 'class LotusHelper {};' 'struct lotus_data;' '#define LOTUS_NEW 1' 'constexpr int LotusDelay = 3;' 'bool lotusSendKeys(int count);' 'static void send_lotus(int n) {' 'namespace lotus {'; do
        printf '%s\n' "$line" >> "$dir/old.cpp"
        git -C "$dir" commit -qam decl
        expect fail "declaration: $line"
    done
    sed -i 's/public Base {}/public Base, public Other {}/' "$dir/old.cpp"
    git -C "$dir" commit -qam rewrite
    expect pass "an existing declaration rewritten in place"
    sed -i 's/class LotusEngine : public Base {};/class LotusHelper : public Base {};/' "$dir/old.cpp"
    git -C "$dir" commit -qam rename
    expect fail "an old declaration replaced by a new lotus name"
    if [ $failed -eq 0 ]; then
        echo "self-test: ok"
    fi
    return $failed
}

if [ "${1:-}" = "--self-test" ]; then
    selfTest
else
    check "${1:-origin/main}"
fi
