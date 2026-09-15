# AGENTS.md

This file provides guidance to agents when working with code in this repository.

## Non-obvious coding rules

- **Never touch backend C++ unless explicitly asked.** Frontend-only edits are the norm.
- **Always deploy after editing frontend files** — copy all 3 files to `src\build\bin\frontend\` or changes won't appear.
- **`escapeAttr()` for onclick attrs, `escapeHtml()` for DOM text** — mixing them breaks inline handlers.
- **`state.decisions` is the source of truth for approve/reject** — it is never synced to the backend; patching `state.allShipments` locally is intentional.
- **`state.allShipments` is patched on approval** — don't re-fetch from backend after approve or the patch is lost.
- **Enum strings are defined as `static` methods on each model struct** in `src/backend/models/*.h` — check there before hardcoding string values.
- **`initDB()` must be called once before any `getDB()` call** — calling `getDB()` first throws; order matters in tests.
- **Tests: no external framework** — use `check(bool, string)` from `test_main.cpp`. To isolate one suite, comment out the other `testXxx()` calls in `main()` and rebuild.
- **`sqlite3.c` must be compiled with `gcc`, not `g++`** — do not change this line in build.bat.
- **Build says "Complete" even on linker failure** — check for `Permission denied` in stderr (running exe locks the file).
