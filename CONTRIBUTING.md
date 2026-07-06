# Contributing to DigiRadio

Thanks for your interest in DigiRadio. This document covers the firmware
development conventions. The full, authoritative rules live in
[`Software/AGENTS.md`](Software/AGENTS.md); this is the human-facing
summary.

DigiRadio is dual-licensed: hardware under **CERN-OHL-S v2**, firmware
under **Apache-2.0**. By contributing, you agree your contributions are
licensed under the same terms as the part of the project they touch.

## Toolchain

| Item      | Choice                                   |
|-----------|------------------------------------------|
| Framework | ESP-IDF v5.5.x (native, not Arduino)     |
| Language  | C++23, pinned `-std=gnu++23`             |
| Errors    | `std::expected<T, Error>`; exceptions off|
| Docs      | Doxygen (build must pass, see below)     |

On macOS, the host unit tests need a C++23 standard library: use
Homebrew `llvm` (>= 18) or `gcc-14`, not the system Apple Clang.

## Build, test, docs

Device build / flash / monitor:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p <port> flash monitor
```

Host unit tests (pure core, no hardware):

```bash
cmake -S components/core/test -B build-host \
      -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++"
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

Documentation (must exit 0; run from `Software/`):

```bash
doxygen Doxyfile
python3 tools/check-manual-sync.py
```

The LaTeX manual lives in `Software/docs/manual/` (canonical). The repository
root `docs/` is a symbolic link to that folder for convenience and Overleaf
/GitHub browsing. Design and HTTP JSON API: `ch-api.tex`; rebuild the PDF with
`latexmk -lualatex manual.tex` inside `docs/` or `Software/docs/manual/`.

## Coding conventions

The guiding idea is *Code That Fits in Your Head*: code must fit in
human working memory at every zoom level.

- **Complexity `<= 7`** per method; **methods fit an 80x24 box** (<= 80
  columns, <= 24 lines). One method does one thing.
- **Strong typing.** No primitive obsession: domain quantities are their
  own types. `enum class` always; no booleans for mode selection.
  Validate untrusted input once at the boundary.
- **RAII and `const` by default.** Wrap every hardware/OS handle; no raw
  `new`/`delete`; borrow with `std::span`, not pointer + length.
- **Functional core, imperative shell.** Pure logic in
  `components/core` (no ESP-IDF headers, host-tested); hardware access in
  the shell.
- **No silent failure.** Fallible operations return `std::expected`;
  every timeout is an explicit error.
- **Embedded discipline.** No dynamic allocation in audio or ISR paths;
  no virtual calls in IRAM-safe ISRs; every wait has a timeout.
- **Never invent** a register address, opcode, or boot sequence — cite
  the datasheet section in a comment, or stop and ask.

## File headers and documentation

Every source file starts with the Apache-2.0 header (see
[`Software/apache-header.txt`](Software/apache-header.txt)), with
`@file`, `@author`, and `@date` filled in.

Every class and method carries a Doxygen block with, in order: name
(`@dname`), parameters (`@param`), return (`@return`), public state used
(`@pubstate`), a description of intent (not a restatement of the code),
and `@author` / `@date`. The `doxygen Doxyfile` build enforces this —
an undocumented symbol fails the build.

## Commits and pull requests

- **Small, focused commits.** Each commit compiles and keeps host tests
  green. One logical change per commit.
- **Commit messages: 50/72.** Summary line <= 50 characters, imperative
  mood; blank line; body wrapped at 72 explaining *why*.
- Keep `main` always building. Use feature branches for work in
  progress; no commented-out code in commits.

## Definition of Done

Before opening a PR, confirm:

- [ ] Compiles with warnings-as-errors; clang-tidy clean.
- [ ] Every file has the Apache-2.0 header.
- [ ] Every class and method has its documentation block.
- [ ] `doxygen Doxyfile` exits 0 with an empty warnings log.
- [ ] `python3 tools/check-manual-sync.py` passes.
- [ ] Manual updated: `ch-classes.tex` for new/changed public classes;
      `ch-api.tex` for new/changed HTTP endpoints.
- [ ] Every method fits 80x24 and complexity <= 7.
- [ ] Fallible paths return typed results; no silent failure.
- [ ] Pure-core logic has passing host unit tests.
- [ ] No secret is loggable or stored in plaintext.
- [ ] Register-level decisions cite the datasheet section.
- [ ] No dynamic allocation in audio/ISR paths.

## Editor setup (optional)

The repository ships Cursor rules under `Software/.cursor/rules/`. If you
use Cursor, open the `Software/` directory as the project so the rules
and `AGENTS.md` are picked up automatically.
