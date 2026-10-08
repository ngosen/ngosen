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

## Releasing a version

The release is `X.Y.Z-N` (`N` is the packaging revision) with epoch 1, tagged `ngosen-X.Y.Z-N`.

1. On a `release/X.Y.Z` branch, set the version everywhere:
   - `CMakeLists.txt` (`project(... VERSION X.Y.Z)`)
   - `packaging/arch/PKGBUILD` (`pkgver`, `pkgrel`)
   - `packaging/rpm/fedora/fcitx5-lotus.spec` and `packaging/rpm/opensuse/fcitx5-lotus.spec` (`Version`,
     `Release`, a new `%changelog` entry)
   - `packaging/debian/changelog` (new entry `fcitx5-ngosen (1:X.Y.Z-N)`)
   - `org.fcitx.Fcitx5.Addon.Lotus.metainfo.xml.in.in` (release `date`)
   - `.github/ISSUE_TEMPLATE/bug_report.yml` (example version)

   `packaging/check-version.sh X.Y.Z-N` must print no `MISMATCH`.
2. `CHANGELOG.md`: turn `## [Chưa phát hành]` into `## [X.Y.Z-N] — DD/MM/YYYY` with a one-line summary and a
   link to the release, and start a new empty `## [Chưa phát hành]`.
3. `packaging/release-notes.md`: update the "Mức đã thử" column to what was actually typed on each system
   for this version. Untested systems stay "chỉ dựng".
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
7. Website, after the release is public: add the release entry, update the version and download links,
   and mirror any README changes to the intro, highlights and roadmap. Publishing the site needs the
   maintainer's approval.
