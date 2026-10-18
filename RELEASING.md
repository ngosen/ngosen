# Releasing

What to update when a pull request is merged and when a version is released. The website
([ngosen/ngosen.github.io](https://github.com/ngosen/ngosen.github.io)) copies its claims from this
repository, so it comes last in both lists; its own rules are in that repository's `AGENTS.md`.

## After a pull request is merged

- `CHANGELOG.md`: one line under `## [Chưa phát hành]` saying what changed for the user, with the PR number.
  Part of every PR, not a follow-up.
- `README.md`: when a feature, the list of highlights or the roadmap changes.
- Website: only when users can see the change or the roadmap moved. Unreleased changes are not announced as
  a new version.

## Branches and pace

- `next` holds features. A feature release (`X.Y.0`) carries one main feature, so a finished feature
  whose turn has not come is turned off on the release branch. Few people run the `next-build`
  pre-release, so instead of waiting on it, the packages from the release PR are typed on an X11 and a
  Wayland test machine before the release.
- `patch` holds fixes for the current release. A bug-fix release (`X.Y.Z+1`) is cut from `patch` when a
  serious bug (lost text, hangs, no typing) is fixed; minor fixes wait for the next release.
- After a bug-fix release, merge `main` into `next` through a PR. After a feature release, fast-forward
  `patch` to `main`:
  `gh api -X PATCH repos/ngosen/ngosen/git/refs/heads/patch -f sha=<merge commit on main>`.

## Releasing a version

The release is `X.Y.Z-N` (`N` is the packaging revision) with epoch 1, tagged `ngosen-X.Y.Z-N`.

1. On a `release/X.Y.Z` branch made from `next` (feature release) or `patch` (bug-fix release), set the
   version everywhere:
   - `CMakeLists.txt` (`project(... VERSION X.Y.Z)`)
   - `packaging/arch/PKGBUILD` (`pkgver`, `pkgrel`)
   - `packaging/rpm/fedora/fcitx5-ngosen.spec` and `packaging/rpm/opensuse/fcitx5-ngosen.spec` (`Version`,
     `Release`, a new `%changelog` entry)
   - `packaging/debian/changelog` (new entry `fcitx5-ngosen (1:X.Y.Z-N)`)
   - `io.github.ngosen.NgoSen.metainfo.xml.in.in` (release `date`)
   - `.github/ISSUE_TEMPLATE/bug_report.yml` (example version)

   `packaging/check-version.sh X.Y.Z-N` must print no `MISMATCH`.

2. `CHANGELOG.md`: turn `## [Chưa phát hành]` into `## [X.Y.Z-N] — DD/MM/YYYY` with a one-line summary and a
   link to the release, and start a new empty `## [Chưa phát hành]`.
3. `packaging/release-notes.md`: the first paragraph says what this version fixes or adds (the CHANGELOG
   summary), and "Đã test những gì" lists only what was typed on this version. Systems not tested this time
   stay under "đã được gõ thử ở các version trước" or "chỉ được build".
   Keep each paragraph on one line: GitHub breaks lines on the release page where the file does.
4. Merge the PR once CI passes, then tag the merge commit on `main` through the API, not from a local
   checkout, where a stale local tag can point at the wrong commit:

   ```
   gh api repos/ngosen/ngosen/git/refs -f ref=refs/tags/ngosen-X.Y.Z-N -f sha=<merge commit>
   ```

   The release workflow refuses a tag that is not on `main` or that disagrees with the packaging files.

5. Check the draft release before publishing:
   - all packages and `SHA256SUMS` are attached, and the sums match;
   - every package reports `1:X.Y.Z-N`;
   - each pattern `install.sh` looks for matches exactly one line of `SHA256SUMS`.
6. The maintainer publishes it:
   `gh release edit ngosen-X.Y.Z-N --repo ngosen/ngosen --draft=false --latest`.
   Publishing starts the `APT repository` workflow, which rebuilds the repository at
   `https://ngosen.github.io/ngosen/apt` from the two newest releases; check that it passes.
7. Website, after the release is public: add the release entry, update the version and download links,
   and mirror any README changes to the intro, highlights and roadmap. Publishing the site needs the
   maintainer's approval.
