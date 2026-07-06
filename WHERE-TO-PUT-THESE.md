# DigiRadio — repository documentation package

Copy the contents into your repo keeping this structure.

```
DigiRadio/                          <- repo root
├── LICENSE                         CERN-OHL-S v2 (hardware)
├── CONTRIBUTING.md                 dev conventions (human-facing)
├── .gitignore
└── Software/                       firmware project root (open THIS in Cursor)
    ├── LICENSE                     Apache-2.0 (firmware)
    ├── AGENTS.md                   authoritative coding rules
    ├── instructions.md             agent kickoff briefing (Slice 1)
    ├── Doxyfile                    API docs generation + enforcement
    ├── apache-header.txt           header to paste in each source file
    ├── .cursor/rules/*.mdc         Cursor scoped rules (6 files)
    ├── tools/
    │   └── check-manual-sync.py    enforces "a section per public class"
    └── docs/
        └── manual/                 the technical manual (LaTeX) — canonical
            ├── manual.tex          main file
            ├── digiradio-manual.sty style (Optima-like, boxes, listings)
            ├── ch-*.tex            chapters
            └── manual.pdf          compiled preview

At the **repository root**, `docs/` is a **symbolic link** to
`Software/docs/manual/` (one source of truth; do not duplicate .tex here).
```

## Build the manual
    cd docs                              # symlink → Software/docs/manual
    latexmk -lualatex manual.tex         # real Optima on macOS
    # or: cd Software/docs/manual && latexmk -lualatex manual.tex

## Enforcement in CI (run from Software/)
    doxygen Doxyfile                              # API docs must pass
    python3 tools/check-manual-sync.py            # manual must be in sync

## Notes
- Two LICENSE files: CERN-OHL-S at root (hardware), Apache-2.0 in
  Software/ (firmware). GitHub auto-detects both.
- docs/api/ (Doxygen output) is git-ignored; the manual PDF is optional
  to commit (source .tex is the master).
- Open Software/ as the Cursor project so rules and AGENTS.md load.
