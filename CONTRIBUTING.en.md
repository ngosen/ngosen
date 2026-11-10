[English](CONTRIBUTING.en.md) | [Tiếng Việt](CONTRIBUTING.md)

# Contributing to Ngó Sen

Thank you for your interest in contributing to Ngó Sen! This document guides you on how to participate in project development.

## 📋 Table of Contents

- [Getting Started](#getting-started)
- [Contribution Workflow](#contribution-workflow)
- [Code of Conduct](#code-of-conduct)
- [Code Style Rules](#code-style-rules)
- [Pull Request Process](#pull-request-process)
- [Important Notes on Branching](#important-notes-on-branching)
- [Bug Reporting](#bug-reporting)
- [Feature Requests](#feature-requests)

## Getting Started

### System Requirements

- GCC or Clang with C++17 support
- CMake >= 3.16
- Rust 1.88+ with cargo (for the typing core)
- Fcitx5 development headers
- Git

### Installation and Build

```bash
# Clone repository
git clone https://github.com/ngosen/ngosen.git
cd ngosen

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Contribution Workflow

### 1. Fork repository

Fork this repository on GitHub and clone your fork to your machine.

```bash
git clone https://github.com/yourusername/ngosen.git
cd ngosen
git remote add upstream https://github.com/ngosen/ngosen.git
```

### 2. Create a new branch

Always create a new branch for each feature or bug fix:

```bash
git checkout next
git pull upstream next
git checkout -b feature/feature-name
# or
git checkout -b fix/bug-name
```

### 3. Make changes

- Write clean, readable code
- Follow code style rules (see below)
- Update documentation if needed

### 4. Commit changes

Use clear and descriptive commit messages:

```
feat: add emoji support

- Add emoji_data.h
- Update generate_emoji_header.py
```

## Code of Conduct

All members contributing to this project must adhere to the [Contributor Code of Conduct](CODE_OF_CONDUCT.en.md) to build an open, friendly, and healthy community.

## Code Style Rules

- **C/C++**: Follow the [`.clang-format`](.clang-format) file in the repository.
- **Python (`settings-gui`)**: Follow `ruff` linting and formatting standards (`ruff check` and `ruff format`).
- Encouraged to create a `pre-commit` hook to automatically format code before commit by creating `.git/hooks/pre-commit` with the following content:

```bash
#!/bin/bash
# Format C/C++
CPP_FILES=$(git diff --cached --name-only --diff-filter=ACMR \vert{} grep -E '\.(cpp\vert{}h)$')
if [ -n "$CPP_FILES" ]; then
    for file in $CPP_FILES; do
        clang-format -i "$file"
        git add "$file"
    done
fi

# Lint & Format Python
PY_FILES=$(git diff --cached --name-only --diff-filter=ACMR \vert{} grep -E '^settings-gui/.*\.py$')
if [ -n "$PY_FILES" ]; then
    ruff check --fix settings-gui
    ruff format settings-gui
    for file in $PY_FILES; do
        git add "$file"
    done
fi
```

Then run the command: `chmod +x .git/hooks/pre-commit`

## Pull Request Process

### 1. Ensure clean code with `clang-format` and `ruff`

```bash
# C/C++
clang-format -i src/*.cpp src/*.h

# Python
ruff check --fix settings-gui
ruff format settings-gui
```

### 2. Run tests

```bash
# Build C++ code
cd build
cmake .. && make
```

### 3. Rebase with the next branch

```bash
git checkout next
git pull upstream next
git checkout feature/feature-name
git rebase next
```

### 4. Push and create PR

```bash
git push origin feature/feature-name
```

Create a Pull Request on GitHub with:

- Clear descriptive title
- Detailed description of changes
- Link to related issue (if any)
- Screenshots for UI changes

## Important Notes on Branching

### PRs go into `next` or `patch`, never `main`

- The `main` branch only holds releases and only the maintainer merges into it. The installer downloads
  `install.sh` from `main`, so whatever lands there runs on users' machines at once.
- New features and everything else: PR into `next`.
- A fix for a bug in the current release: PR into `patch`. The maintainer ships bug-fix releases (e.g.
  1.4.2 → 1.4.3) from `patch`, then merges `patch` into `next`.
- Not sure? Send it to `next`; the maintainer will move it if needed.

### Branch Structure

```
main    ← Releases (only the maintainer merges)
  ↑            ↑
next          patch
(features)    (fixes for the current release)
  ↑            ↑
feature/*, fix/*  ← Personal branch for each PR
```

### Merge Process

1. Developer creates a PR to `next` (feature) or `patch` (fix)
2. Code review by maintainer. To have the people who reported a problem test the fix first, a maintainer
   adds the `test-build` label: CI publishes the packages as the `pr-<number>-build` pre-release and
   links it from the PR. It is deleted when the PR closes
3. Merge, then test with the `next-build` pre-release
4. Bug-fix release: `patch` → `main`, then `patch` → `next`. Feature release: `next` → `main`, then reset
   `patch` to `main`

## Bug Reporting

When reporting a bug, please provide:

- Ngó Sen version
- Operating system and version
- Steps to reproduce the bug
- Log or screenshot (if any) (Log via `fcitx5-diagnose` command)
- Environment (desktop environment, terminal, etc.)

## Feature Requests

Before proposing a new feature:

- Check if the feature already exists
- Search for similar issues
- Clearly describe the feature and use case
- Explain why this feature is necessary

## License

By contributing, you agree that your code will be licensed under the same license as the project (GPL-3.0-or-later).

---

Thank you for contributing!
