# Codex verification

Date: 2026-08-12
Starting HEAD: `73593b9497801d8347694e50f3c844dad269d869`
Scope: Optional `UnitConversion` bindings for CalculationWorldRule Value inputs and Calculation rename through the Godot facade/UI.

## Changed files

- `include/clife/world/definition.hpp`
- `include/clife/world/runtime_rules.hpp`
- `src/world/definition.cpp`
- `src/world/runtime.cpp`
- `src/world/runtime_rules.cpp`
- `apps/godot/native/src/clife_world_editor.cpp`
- `apps/godot/native/src/clife_world_editor.hpp`
- `apps/godot/native/src/clife_world_editor_authoring.cpp`
- `apps/godot/native/src/clife_world_editor_marshalling.cpp`
- `apps/godot/native/src/clife_world_editor_snapshot.cpp`
- `apps/godot/scripts/world_editor_calculations_panel.gd`
- `apps/godot/scripts/world_editor_world_rules_panel.gd`
- `apps/godot/translations/clife_editor.en.po`
- `apps/godot/translations/clife_editor.ru.po`
- `tests/world_tests.cpp`
- `reports/codex/LATEST.md`

## Commands

- `cmake --build --preset vs2022-debug` — PASS
- `ctest --preset vs2022-debug` — PASS (3/3 tests)
- `.\scripts\build_godot.ps1 -Configuration Debug` — PASS
- `git diff --check` — PASS
- `Get-Command godot -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source` — FAIL: no Godot executable on `PATH`.

## Result

Calculation-world-rule value inputs now optionally reference a UnitConversion. RuntimeWorld resolves its numeric ratio during compilation; RuntimeRuleExecutor receives only a multiplier. Schema 10 preserves bindings, while schema 8 and 9 restore them as no conversion. Calculation names can be edited without changing CalculationId or existing references.

## Notes

- Native snapshot import accepts schemas 8, 9, and 10. Missing input `conversion_id` is interpreted as no conversion.
- Tests cover residual and runtime conversions, no-conversion behavior, characteristic rejection, conversion-reference deletion protection, schema migration/round-trip, and CalculationId-preserving rename.
- No Godot project parsing or interactive UI smoke test ran because no Godot executable is available on `PATH`.
- Release build/test was not run; it was not required for this slice.
