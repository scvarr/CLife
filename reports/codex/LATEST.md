# Codex verification

Date: 2026-08-12
Starting HEAD: `8481791a69ba10c33d80fe814a9efe249c0103e0`
Scope: Move `UnitConversionId` from `FunctionProcessDefinition` to each `FunctionProcessOutputDefinition`, including local schema-8 snapshot migration.

## Changed files

- `include/clife/world/definition.hpp`
- `src/world/definition.cpp`
- `src/world/phenotype.cpp`
- `src/presets/first_world.cpp`
- `apps/godot/native/src/clife_world_editor.cpp`
- `apps/godot/native/src/clife_world_editor.hpp`
- `apps/godot/native/src/clife_world_editor_authoring.cpp`
- `apps/godot/native/src/clife_world_editor_definition.cpp`
- `apps/godot/native/src/clife_world_editor_snapshot.cpp`
- `apps/godot/scripts/world_editor_functions_panel.gd`
- `tests/world_tests.cpp`
- `docs/C7_GENOTYPE_PHENOTYPE.md`
- `reports/codex/LATEST.md`

## Commands

- `cmake --build --preset vs2022-debug` — PASS
- `ctest --preset vs2022-debug` — PASS (3/3 tests)
- `./scripts/build_godot.ps1 -Configuration Debug` — PASS
- `git diff --check` — PASS
- `Get-Command godot` — PASS: no executable available on `PATH`

## Result

Each process output now owns its conversion. Phenotype compilation calculates each output's `result_per_input` from that output's allocation and conversion ratio; Calculator input remains unchanged.

## Notes

- Snapshot schema is 9. The Godot import path accepts schema 8 and copies its legacy process conversion to every process output before restore; re-export uses schema 9.
- The world tests cover independent output ratios, allocation validation independent of ratios, schema-8 migration, schema-9 round-trip, and conversion-reference removal.
- No Godot project parsing or interactive UI smoke test ran because a Godot executable is not available on `PATH`.
- Release build/test was not run; it is not required for this slice.
