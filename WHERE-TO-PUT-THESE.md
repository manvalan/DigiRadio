# DigiRadio — repository documentation package

Canonical layout for this repository:

```
DigiRadio/                          <- repo root (README, Hardware, LICENSE)
├── CONTRIBUTING.md                 dev conventions (human-facing)
├── README.md                       project overview (fw 0.8.3)
└── Software/                       firmware project root (open THIS in Cursor)
    ├── LICENSE                     Apache-2.0 (firmware)
    ├── AGENTS.md                   authoritative coding rules
    ├── instructions.md             agent kickoff + roadmap status
    ├── sdkconfig.defaults          C++23, NVS + flash encryption (dev mode)
    ├── sdkconfig.defaults.production  release-mode overlay (irreversible)
    ├── partitions.csv              nvs + nvs_keys partitions
    ├── Doxyfile                    API docs generation + enforcement
    ├── apache-header.txt           header to paste in each source file
    ├── .cursor/rules/*.mdc         Cursor scoped rules (6 files)
    ├── tools/
    │   ├── check-manual-sync.py    one LaTeX section per public class
    │   ├── check_si4684_blobs.py   no proprietary .bin in git
    │   ├── fetch_si4684_firmware.py  local blob procurement
    │   └── gzip-www.sh             regenerate embedded web UI gzip
    └── docs/
        ├── TODO.md                 agent + HIL backlog
        ├── security-flash-nvs.md   encryption + device checklist
        └── manual/                 LaTeX technical manual (canonical)
            ├── manual.tex
            ├── ch-*.tex
            └── manual.pdf          optional compiled preview
```

LaTeX sources live in `Software/docs/manual/`. Build the PDF from there:

```bash
cd Software/docs/manual && latexmk -lualatex manual.tex
```

## CI enforcement (from `Software/`)

```bash
doxygen Doxyfile
python3 tools/check-manual-sync.py
python3 tools/check_si4684_blobs.py
```

Host tests: `cmake -S components/core/test -B build-host && ctest --test-dir build-host`.

## Notes

- Two LICENSE files: CERN-OHL-S at repo root (hardware), Apache-2.0 in
  `Software/` (firmware).
- Si4684 `.bin` blobs are gitignored — never commit them.
- Open `Software/` as the Cursor project so rules and `AGENTS.md` load.
