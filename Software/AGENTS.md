# AGENTS.md — DigiRadio Firmware

Rules for any coding agent (Claude Code, etc.) working on the DigiRadio
firmware. This file is authoritative. If a request conflicts with these
rules, stop and surface the conflict before writing code.

Target: ESP32-S3-WROOM-1. Framework: **ESP-IDF v5.5.x** (stable).
Language: **C++23** (`-std=gnu++23`), strongly typed, class-based.
C++ exceptions: **disabled** (ESP-IDF default; keep off).
Companion chips: Si4684 (DAB+/FM tuner), ADAU1701 (SigmaDSP audio),
FSC-BT1035 / QCC3056 (Bluetooth audio, AT-controlled over UART).

---

## 0. How this agent must behave

- **Blockers first.** Open every response with what will stop the build
  or the hardware from working. State the risk before the solution.
- **Zero tolerance for guessing at the hardware.** Never invent a
  register address, an opcode, a bit field, or a boot sequence. If it is
  not in the datasheet / programming guide, say so and stop. Cite the
  document and section for every register-level decision.
- **No silent failure.** Every fallible operation returns a typed error
  (see §6). Nothing is swallowed, nothing is logged-and-ignored.
- **Small steps.** One vertical slice at a time, compiling and testable
  at every commit. No big-bang subsystems.
- **Ask when the invariant is unclear.** A wrong assumption baked into a
  driver costs a re-flash and a debugging session. Confirm, don't assume.

---

## 1. Prime directive — Code That Fits in Your Head

Human working memory holds about seven things. Every unit of code must
fit in that budget at its own zoom level (methods, classes, modules —
fractal). Concretely:

- **Cyclomatic complexity <= 7 per method.** At 8, decompose. No
  exceptions for "it's just a switch over registers" — extract a table.
- **The 80x24 box.** A method fits in an old terminal screen: <= 80
  columns wide, <= 24 lines tall. If it doesn't fit, it's doing too much.
- **A method does one thing** at one level of abstraction. Mixing I2C
  byte-twiddling and business logic in the same method is a smell.
- **Name for intent, not mechanism.** `tuneTo(Frequency)` not
  `writeReg0x30()`. The datasheet detail lives *inside* the method.
- **Delete before you add.** The cheapest code to maintain is the code
  that isn't there. Prefer removing a branch to adding one.
- **Chunk.** A reader should grasp a class from its public interface
  without reading the bodies. If they can't, the interface leaks.

These are hard limits, enforced in CI where possible (clang-tidy
`readability-function-cognitive-complexity`, line-length lint).

---

## 2. Language and typing rules

C++ standard: **C++23**, pinned as `-std=gnu++23` in CMake (do not rely
on the toolchain default, which differs between ESP-IDF 5.x and 6.x).
This makes `std::expected` available natively (see §6).

### 2.1 Make illegal states unrepresentable

- **No primitive obsession.** Domain quantities get their own types.
  A frequency is not an `int`; a gain is not a `float`; a station id is
  not a `uint8_t`. Use a small strong-typedef template (a `NamedType`)
  or dedicated value classes:

  ```cpp
  class FrequencyKHz {          // 80x24, one invariant, immutable
  public:
      explicit constexpr FrequencyKHz(std::uint32_t khz);  // validates
      constexpr std::uint32_t value() const noexcept;
  private:
      std::uint32_t khz_;       // invariant: within band limits
  };
  ```

- **Parse at the boundary, then trust.** Validate untrusted input
  (network, UART, flash) once, at the edge, into a domain type. After
  that, the type *is* the guarantee — no re-checking downstream.
- **`enum class` always.** Never a bare `enum`. Opcodes, states, bands,
  and modes are enums, not magic numbers.
- **No booleans in public APIs for mode selection.** `setBand(Band::Dab)`
  not `setBand(true)`.

### 2.2 Const-correctness, ownership, RAII

- **`const` by default.** Mutable is the exception you justify.
- **`[[nodiscard]]`** on every function returning a status or a value
  that must not be dropped.
- **Rule of zero.** Wrap every OS/hardware handle (I2C bus, SPI device,
  NVS handle, task, mutex) in a RAII type. No raw `new`/`delete`, no
  manual `*_delete()` calls scattered in code — the destructor owns it.
- **Own with values and smart pointers**, borrow with references or
  `std::span`. Never pass `pointer + length`; pass `std::span<std::byte>`.
- **`noexcept`** on anything that genuinely cannot throw (hot paths,
  destructors, move ops).

### 2.3 Functions and purity

- **Command Query Separation.** A method either changes state (returns
  void / status) or answers a question (returns a value, no side
  effects). Never both.
- **Functional core, imperative shell.** Pure logic — station-list
  operations, EQ coefficient math, config parsing/serialisation, boot
  blob framing — lives in a hardware-free core that compiles and tests
  on the host. All I2C/SPI/UART/flash lives in a thin shell that calls
  the core. The core has zero `#include` of ESP-IDF headers.

---

## 3. File headers and code documentation

These are mandatory and checked in the Definition of Done. A file
without its licence header, or a class/method without its documentation
block, is not done.

### 3.1 File header (every source and header file)

Every `.hpp` / `.cpp` starts with this block, filled in for the file.
Use the SPDX identifier plus the short Apache notice — firmware is
Apache-2.0.

```cpp
/**
 * @file    Si4684Driver.hpp
 * @brief   Si4684 DAB+/FM tuner driver (intent-level interface).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * @author  Michele Bigi
 * @date    <YYYY-MM-DD of creation>
 */
```

Rules:
- The `@date` is the file's creation date and is not rewritten on later
  edits (history lives in version control).
- The copyright year matches the creation year.
- Never place a secret, token, or path to a private resource in a header.

### 3.2 Documentation block — every class and every method

Every class and every method carries a Doxygen block with the fields
below, in this order. Native Doxygen tags are used for the standard
fields; two project aliases (`@dname`, `@pubstate`, defined in the
Doxyfile, §3.3) render the non-standard fields as titled sections in the
generated documentation. This means the required format *is* the tool's
format — one source of truth, no drift.

**Method block:**

```cpp
/**
 * @brief    tuneTo — set the tuner to a validated frequency.
 *
 * @dname    tuneTo
 * @param    freq  Target frequency, already validated to the active band.
 * @return   Ok on success, or Error::TunerTimeout / Error::NotBooted.
 * @pubstate reads band_ (range-check); writes lastRsq_ (refreshed after
 *           a successful tune); uses spi_ (injected SPI dependency).
 *
 * States intent and the contract upheld — why this method exists and
 * what it guarantees. Do NOT restate the code line by line.
 *
 * @author   Michele Bigi
 * @date     <YYYY-MM-DD>
 */
```

**Class block:**

```cpp
/**
 * @brief    Si4684Driver — owns one Si4684, exposes intent-level tuning.
 *
 * @dname    Si4684Driver
 * @param    spi  Injected SPI device, borrowed for the driver's life.
 * @param    fw   Injected firmware source for boot images.
 * @return   n/a (type)
 * @pubstate Public interface: powerUp(), loadImage(Band),
 *           tuneTo(FrequencyKHz), readRsq(). Owns one SPI handle (RAII).
 *           No public data members.
 *
 * Single responsibility of the class in one or two sentences, plus its
 * key invariants (e.g. tuneTo is valid only after a successful
 * loadImage).
 *
 * @author   Michele Bigi
 * @date     <YYYY-MM-DD>
 */
```

The mapping: **Name** → `@dname`, **Parameters** → `@param` (per param;
constructor/template params for a class), **Return** → `@return`,
**Public variables used** → `@pubstate`, **Description** → the free
text, **Author/Date** → `@author` / `@date`.

Field rules:
- **Name** — the class or method name, verbatim.
- **Parameters** — one `@param` per parameter; for a class, the
  constructor / template parameters. Write "none" if there are none.
- **Return** — `@return` with success value and each error cause it can
  return; "void" or "n/a" where applicable.
- **Public state used** — member state read/written and injected
  dependencies touched. With proper encapsulation there are normally no
  public data members, so this documents the shared/member state and
  collaborators the method relies on. Write "none" for a pure function.
- **Description** — intent, contract, and invariants. Explains *why*,
  never a restatement of the implementation.
- **Author / Date** — `@author Michele Bigi` and the `@date`.

Keep the block honest: if a method's "Public state used" list grows
long, that is a design signal to split the method (§1), not to write a
longer comment.

### 3.3 Documentation tooling — Doxygen

The documentation format above is backed by **Doxygen** (the standard
C++ documentation generator), configured by the `Doxyfile` at the repo
root. The tool does two jobs:

1. **Renders** the doc blocks into browsable HTML under `docs/api/`. The
   `@dname` and `@pubstate` aliases turn the project-specific fields into
   proper titled sections, so the generated docs match this spec exactly.
2. **Enforces** the rule. The Doxyfile sets `EXTRACT_ALL = NO`,
   `WARN_IF_UNDOCUMENTED = YES`, `WARN_NO_PARAMDOC = YES`, and
   `WARN_AS_ERROR = FAIL_ON_WARNINGS`. Any class, method, or parameter
   without its documentation block makes `doxygen` exit non-zero.

Rules for the agent:

- **The docs build is part of Done.** Run `doxygen Doxyfile` and it must
  exit 0 with an empty `docs/api/doxygen-warnings.log`. A non-zero exit
  means something is undocumented or malformed — fix it, don't suppress
  the warning.
- **Wire it into CI** as a required job, so an undocumented symbol blocks
  the merge exactly like a failing test does.
- **Do not use `EXTRACT_ALL = YES` to silence warnings.** That flag hides
  missing documentation instead of reporting it, defeating the purpose.
- Generated output (`docs/api/`) is a build artifact — git-ignore it,
  don't commit it.
- Graphviz (`dot`) is optional but enabled: it produces class and
  collaboration diagrams, which help keep the structure "in your head".
  If `dot` is unavailable in an environment, set `HAVE_DOT = NO` there.

The `WARN_AS_ERROR = FAIL_ON_WARNINGS` value requires a recent Doxygen
(1.9.x+); on an older version use `WARN_AS_ERROR = YES`. Verify the
version rather than assuming.

---

### 3.4 Manual synchronisation

The LaTeX manual documents the firmware at the **design level** and must
never fall behind the code. The split of duties is strict:

- **Doxygen** documents the API — exact signatures, parameters, returns,
  per §3.2. Generated from the code.
- **The manual** documents design — what a component is for, its
  responsibility, its collaborators and invariants, and how it fits the
  system. Written prose plus diagrams. It does **not** repeat the
  per-method API.

Rule: **every public, architecturally-significant class has a matching
manual section.** These are the classes with a public interface —
drivers, application services, and public domain-core types (the ones
declared under an `include/` directory). Each gets a `\subsection` (or
`\subsubsection`) in the manual, tagged with a stable label
`\label{cls:ClassName}`, describing its responsibility, collaborators,
key invariants, and how it is used.

- When a public class is **added**, its manual section is written in the
  same change.
- When its **public interface changes**, the manual section is updated in
  the same change.
- When it is **removed**, its manual section is removed.
- Internal / private helper classes (declared only in `src/`, not
  exposed through `include/`) do **not** require a manual section.

This is enforceable, not aspirational: `tools/check-manual-sync.py`
lists the public classes and fails if any lacks a `\label{cls:...}` in
the manual sources. Wire it into CI alongside the Doxygen check.

---

## 4. Architecture

Layered, dependencies point inward only:

```
  Shell (imperative): drivers, web server, NVS, tasks, ISRs
      — thin, no business logic                         [ESP-IDF, HW]
  Application services: TunerService, AudioService, StationService,
      BluetoothService, IntegrationService          [orchestration]
  Domain core (pure, host-testable): Station, Frequency,
      EqProfile, MixerState, Credential, boot-blob
      framing, validation                               [no HW headers]
```

- **Depend on abstractions.** Services take driver *interfaces*
  (e.g. `ITuner`, `IDsp`, `IBtModule`, `ISecureStore`), injected via the
  constructor. This is what makes the core testable without hardware.
- **No god object.** No single `DigiRadio` class that knows everything.
  Compose small services.
- **One class = one responsibility.** If a class name needs "and", split
  it.

---

## 5. Embedded constraints (ESP32-S3)

- **Heap discipline.** Prefer static / stack / pool allocation. No
  dynamic allocation in audio or ISR paths, ever. Allocate at init,
  reuse buffers. Watch fragmentation — long-running device.
- **ISR rules.** ISRs do the minimum: read/clear flag, signal a task.
  No logging, no allocation, no blocking, no C++ exceptions in an ISR.
  No virtual function calls in IRAM-safe ISRs — vtables live in flash and
  are inaccessible when the flash cache is disabled.
- **Tasks and concurrency.** Each subsystem that needs its own timeline
  gets a FreeRTOS task with an explicit stack size and priority,
  documented. Shared state crosses task boundaries only through queues
  or mutex-guarded types — never raw shared globals.
- **Exception policy.** C++ exceptions are disabled by default in
  ESP-IDF and stay disabled here. All recoverable errors use the typed
  result (§6). Destructors and hot paths are `noexcept`.
- **Blocking.** No busy-wait spin loops. Use event groups / notifications
  with timeouts. Every wait has a timeout and a defined failure path.

---

## 6. Error handling

- **Typed results, not error codes floating in `int`.** Use
  `std::expected<T, Error>` — available natively under C++23, so no
  vendored library is needed. `Error` is an `enum class` with a stable
  set of causes plus optional context.
- **Errors propagate to a place that can act.** A driver reports; a
  service decides (retry, degrade, surface to UI); the top level logs.
  Do not decide policy deep in a driver.
- **Every timeout is an error value**, handled explicitly — never a
  silent return.
- **No `assert` for runtime-reachable conditions.** `assert` is only for
  programmer-invariant violations that are bugs by definition. Hardware
  can fail; that's a result, not an assertion.

---

## 7. Subsystem rules

### 7.1 Si4684 tuner driver

- The boot flow (POWER_UP → load patch/bootloader → load firmware image
  → BOOT) must follow the AN649 / programming-guide sequence exactly.
  **Cite the section** for each step in a comment.
- Firmware images (FM, DAB) are large blobs. The driver **streams** them
  in bounded chunks from flash — never loads a whole image into a heap
  buffer. Blob source is an injected interface (`IFirmwareSource`) so it
  can be faked in host tests.
- Command opcodes and property IDs are `enum class`. A `Command` builder
  frames bytes; a `Response` parser validates the CTS/STATUS byte before
  any payload is trusted.
- The public interface is intent-level: `powerUp()`, `loadImage(Band)`,
  `tuneTo(FrequencyKHz)`, `readRsq()`. Register access is private.
- One driver instance owns one SPI (or I2C) device handle via RAII.

### 7.2 ADAU1701 DSP driver (RAM boot, no EEPROM)

- The ESP32 writes the SigmaStudio-exported program **to DSP RAM at every
  boot** (self-boot EEPROM removed by design). Model the export as a
  domain type — an ordered list of `RegisterWrite{ address, bytes }` —
  parsed in the pure core, replayed by the shell over I2C.
- **Safeload for live updates.** EQ and mixer parameter changes at
  runtime use the ADAU1701 safeload mechanism (write to the safeload
  registers + IST) so audio updates are click-free. A raw param write
  while audio runs is a bug.
- Provide typed control surfaces, not raw cell addresses to callers:
  - `setEqBand(EqBandIndex, GainDb, FrequencyHz, Q)` → computes
    biquad coefficients in the pure core, then safeloads them.
  - `setInputMix(MixSource, GainDb)` where
    `enum class MixSource { Si4684, Esp32 }` — this is the input mixer
    between the tuner and the ESP32 audio path.
- Coefficient math (biquad design, gain-to-linear) lives in the pure
  core with host unit tests against known reference values. No DSP math
  hidden inside an I2C method.

### 7.3 FSC-BT1035 (QCC3056) driver

- Controlled by AT commands over UART. Build commands with a typed
  builder; parse responses with explicit `OK`/`ERROR`/timeout handling.
- **Line-In mode is mandatory:** the `AT+AUXCFG=1` step must be part of
  the documented init sequence and covered by a test on the command
  string. Losing it silently breaks the audio path.
- The AT subset in use is enumerated and documented; unknown responses
  are an error value, not ignored.

### 7.4 Network configuration + Web UI

- **Provisioning:** captive portal / SoftAP for first setup, then STA.
  State machine is explicit (`enum class NetState`), no ad-hoc flags.
- **UI: elegant and essential.** A minimal single-page app served
  gzipped from flash. No heavy frameworks; small, fast, legible. Design
  tokens (spacing, type scale, one accent colour) defined once and
  reused — consistency over decoration. The UI is a thin client over a
  typed JSON API; it holds no business logic.
- **API:** REST/JSON with typed DTOs on the firmware side. Every request
  body is parsed into a domain type at the boundary (§2.1) before use.
  Reject malformed input with a clear status; never partially apply.
- Serve UI assets read-only; never expose a raw filesystem or debug
  endpoint in a shipping build (guard behind a build flag).

### 7.5 Secure storage

- Stores: Wi-Fi SSID + password, station/frequency list, audio profiles,
  last-preset index. **Encrypted at rest** — NVS encryption with flash
  encryption enabled in `sdkconfig.defaults` (development mode); keys in
  `nvs_keys` partition; init via `secure_store::initEncryptedStorage()`.
  Production release mode: `sdkconfig.defaults.production`. See
  `docs/security-flash-nvs.md`.
- **Secrets never leave their type.** A `Secret` wrapper: no `operator<<`,
  no implicit conversion to a loggable string, buffer zeroised on
  destruction. Secrets are never logged, never placed in URLs, never
  serialised to plaintext.
- Access is behind `ISecureStore` so the core and tests never touch real
  flash or real keys.

### 7.6 Station / frequency list

- A `Station` is a value type: name, band, frequency (or DAB service id),
  optional preset slot. The list is a domain collection with CRUD in the
  pure core; persistence goes through `ISecureStore`.
- All list operations (add, remove, reorder, find, validate duplicates)
  are host-tested with zero hardware.

---

## 8. Testing

- **TDD where it pays:** the pure core is developed test-first
  (red → green → refactor). Coefficient math, blob framing, config
  parsing, station-list logic — all covered on the host.
- **Arrange–Act–Assert**, one behaviour per test, names that state the
  behaviour: `tuneTo_rejectsFrequencyOutsideFmBand`.
- **Fakes over mocks** for the driver interfaces; assert on observable
  behaviour, not on internal call sequences.
- **Hardware-in-the-loop** tests are separate, explicitly marked, and
  never block the host test suite.
- A change without a test for its logic is not done (hardware-only glue
  excepted, and that glue must be trivially thin).

---

## 9. Version control and workflow

- **Small, frequent commits.** Each commit compiles and keeps tests
  green. One logical change per commit.
- **Commit messages: the 50/72 rule.** Summary line <= 50 chars,
  imperative mood; blank line; body wrapped at 72 explaining *why*.
- **Feature flags / branches by abstraction** for anything half-built —
  `main` always builds and runs.
- No commented-out code committed. Version control is the history.

---

## 10. Definition of Done (checklist)

Before a slice is considered complete:

- [ ] Compiles with warnings-as-errors; clang-tidy clean.
- [ ] Every file has the Apache-2.0 header block (§3.1).
- [ ] Every class and method has its documentation block (§3.2).
- [ ] `doxygen Doxyfile` exits 0 with an empty warnings log (§3.3).
- [ ] Manual section exists/updated for any added or changed public
      class; `tools/check-manual-sync.py` passes (§3.4).
- [ ] Every method <= 80x24, complexity <= 7.
- [ ] No primitive obsession in public interfaces.
- [ ] Fallible paths return typed results; no silent failure.
- [ ] Pure-core logic has host unit tests, all green.
- [ ] No secret is loggable or stored in plaintext.
- [ ] Register-level decisions cite datasheet section in comments.
- [ ] No dynamic allocation in audio/ISR paths.
- [ ] Public interface is understandable without reading bodies.

---

## 11. The agent must NOT

- Ship a file without the Apache-2.0 licence header.
- Ship a class or method without its documentation block.
- Add or change a public class without updating its manual section.
- Invent register addresses, opcodes, bit fields, or boot sequences.
- Put business logic in a driver or in an ISR.
- Return a bare error code or swallow a failure.
- Introduce a class whose name needs "and".
- Exceed the complexity / size limits "just this once".
- Store or log a credential in plaintext.
- Ship a slice that doesn't compile or breaks the host tests.
- Proceed past an unclear hardware invariant without asking.

---

## 12. Repo layout, build and test

### Layout (ESP-IDF project; core is host-testable)

```
Software/
├── AGENTS.md  Doxyfile  instructions.md
├── CMakeLists.txt            top-level ESP-IDF project
├── sdkconfig.defaults        C++23, exceptions off, flash/NVS encryption
├── partitions.csv            nvs, otadata, ota_0/ota_1, dsp blob, nvs_keys
├── .cursor/rules/*.mdc
├── main/                     imperative shell entry (app_main)
├── components/
│   ├── core/                 PURE domain core — no ESP-IDF headers
│   │   ├── include/core/     public headers
│   │   ├── src/
│   │   └── test/             host unit tests (plain CMake + ctest)
│   ├── drivers/{si4684,adau1701,bt1035}/
│   ├── net/                  provisioning + web server
│   ├── secure_store/
│   └── services/             TunerService, AudioService, ...
└── docs/api/                 Doxygen output (git-ignored)
```

Rule: `components/core` compiles two ways — as an ESP-IDF component AND
standalone on the host for unit tests. It must never `#include` an
ESP-IDF header, so the host build stays hardware-free.

### Commands

Device build / flash / monitor (first encrypted flash: erase once):
```
idf.py set-target esp32s3
idf.py build
idf.py erase-flash flash monitor
```

Host unit tests (pure core; needs a C++23 stdlib compiler):
```
cmake -S components/core/test -B build-host \
      -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++"
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

Docs and policy (must exit 0, from `Software/`):
```
doxygen Doxyfile
python3 tools/check-manual-sync.py
python3 tools/check_si4684_blobs.py
```

### Host toolchain note (macOS)

On the M4 Mac, `std::expected` needs a recent C++23 stdlib. Use Homebrew
`llvm` (>= 18) or `gcc-14` for the host test build — the system Apple
Clang may be too old. This affects only host tests, not the firmware.
