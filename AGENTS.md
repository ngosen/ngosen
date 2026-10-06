# AGENTS.md

Rules for anyone (human or coding agent) changing this fork or sending patches upstream to
[LotusInputMethod/fcitx5-lotus](https://github.com/LotusInputMethod/fcitx5-lotus). Upstream's
[CONTRIBUTING](CONTRIBUTING.en.md) still applies; this file adds what it leaves out.

These rules exist because upstream reviewers said our patches were hard to read: Vietnamese
identifiers, too many core changes at once, long AI-sounding PR text, and claims they could not
check (upstream #476, #492). Measured on 2026-09-26:

|                                                 | Upstream               | This fork          |
| ----------------------------------------------- | ---------------------- | ------------------ |
| Comment lines per code line (`src/`, `server/`) | 0.04                   | 0.29 (added lines) |
| Median comment block                            | 1 line                 | 3 lines            |
| Median issue/PR comment on upstream             | 140 chars (maintainer) | 2,174 chars        |

Most of this work is done with an AI coding agent. That is allowed, but the output has to read like
a careful human wrote it, and the person sending it must be able to explain every line.

## Code

- **English only** for identifiers, comments, log strings and commit messages. User-facing UI
  strings stay translatable (`_()`), with Vietnamese in `po/vi.po`.
- **Comments say why, not what or how.** If a comment describes what the code does, rename things
  instead. Most comments are one line; a block of more than 3 lines needs a real reason.
- **No measurements, dates or history in code.** "Measured 0/60 wrong at 70 ms", "the old path
  slept here", "since 20/09" belong in the commit message or PR. Refer to a real issue as `#123`.
  No internal labels (`v7`, `B33`, `AF-0012`, session names) anywhere in the repo.
- **Reuse what exists.** Log with `LOTUS_DEBUG/INFO/WARN/ERROR` (`src/lotus-utils.h`); use fcitx5 and
  libc facilities (event loop timers, `syslog()`) before writing a new mechanism.
- **Fix reported problems.** Do not add code for cases no user hits ("200 keys per second"). If only
  a test harness triggers it, file a low-priority issue instead of changing core code.
- **Match surrounding code.** clang-format (`.clang-format`), ruff for `settings-gui/`, existing
  naming (`camelCase` functions, `snake_case_` members). Functions under ~50 lines.
- **Run clang-format before pushing.** CI fails on any formatting diff, including alignment of
  neighbouring declarations.
- **Build as C++17.** The Ubuntu 22.04 package compiles in C++17, so no `std::string::starts_with`
  and the like; use `isStartsWith` (`src/lotus-utils.h`).
- **A bug fix comes with a test that fails without it.** Check this by undoing the fix (or breaking
  its guard) and watching that test, and only that test, fail.
- **Keep SDL games on the uinput server.** SDL takes only commits and preedit, so forwarded
  backspaces never reach it. When changing which frontends forward backspaces, keep the
  `sdlKeepsServer` checks in `test/ibus-dbus-forward-backspaces.cpp` passing.

## Commits

- Conventional Commits, imperative subject of at most 72 characters: `fix(uinput): select with
right Shift`.
- The body says what was wrong for the user and why this fixes it. Numbers go here, one line each,
  with how they were measured: `Tested: Writer, 60 words, 0 wrong (was 30).`
- One logical change per commit; every commit builds and passes `ctest`.
- Keep the `Co-Authored-By:` trailer the agent adds. It is how we disclose AI assistance.

## Pull requests

Inside this fork: every change goes through a PR into `main` (branches are deleted on merge).
Each such PR adds a line under `## [Chưa phát hành]` in [CHANGELOG.md](CHANGELOG.md): what
changed for the user, with the PR number. The changelog is written in Vietnamese, since most of its
readers are Vietnamese users; keep English tech terms as in "Writing issues and PR text" below.
Refer to upstream issues as `LotusInputMethod/fcitx5-lotus#123`, since a bare `#123` links to this
fork.

To upstream:

- **Open an issue first**, get the maintainer's agreement on the direction, then send code. PRs sent
  cold have been closed without comment (upstream #452).
- **One concern per PR**, based on current upstream `dev`. A bug fix, a refactor and a rename are
  three PRs. If the description needs headings, the PR is probably too big.
- **Keep core changes small.** The maintainer reviews alone in spare time; a PR worth sending costs
  less to review than it gives back. Prefer an off-by-default option over a behaviour change.
- **CI must pass** (clang-format, build, tests) before asking for review.
- **Say what was not tested** (other desktops, X11, other browsers). Never tick a checklist box that
  was not actually run.

## Writing issues and PR text

Upstream issues and PRs are written in Vietnamese; the rules below apply to that prose.

- **Short.** Aim for under 800 characters, excluding code blocks and logs. Longer detail goes in a
  linked file in this fork, not in the comment.
- **Problem first:** what the user sees, how to reproduce, environment (version, desktop, app,
  `fcitx5-diagnose`). Then the proposed fix in one or two sentences, pointing at the function.
- **Keep English tech terms**: focus, commit, preedit, surrounding text, backspace, event, timeout,
  test, log, app, window. Do not invent Vietnamese for them ("tiêu điểm", "đối chứng dương",
  "gốc rễ" read as machine translation to this community).
- **Only checkable claims.** Every number says how it was measured. Separate facts from guesses and
  label guesses. Drop trade-offs and theories you cannot demonstrate.
- **No AI filler**: no bold-heavy structure, no "root cause analysis" headings, no restating the
  question, no summary of the summary. Write the way the maintainer writes: plain and direct.
- **The human sends it.** Draft with the agent if needed, then cut it down and make sure you can
  answer questions about every sentence without asking the agent.

## Checklist before posting upstream

1. Would the maintainer understand the problem from the first two sentences?
2. Is it under 800 characters (not counting code)?
3. Is every number reproducible from what the text says?
4. Is there exactly one concern?
5. Are all identifiers and comments in the diff English, short, and free of history?
6. Does CI pass on the branch?

## Sources

- Linux kernel [coding style: commenting](https://www.kernel.org/doc/html/latest/process/coding-style.html)
  and [submitting patches](https://www.kernel.org/doc/html/latest/process/submitting-patches.html)
- Google [C++ style: comments](https://google.github.io/styleguide/cppguide.html#Comments) and
  [small CLs](https://google.github.io/eng-practices/review/developer/small-cls.html)
- [LLVM AI tool policy](https://llvm.org/docs/AIToolPolicy.html),
  [Ghostty AI policy](https://github.com/ghostty-org/ghostty/blob/main/AI_POLICY.md),
  [curl contribution guide](https://curl.se/dev/contribute.html)
- Simon Tatham, [How to Report Bugs Effectively](https://www.chiark.greenend.org.uk/~sgtatham/bugs.html)
