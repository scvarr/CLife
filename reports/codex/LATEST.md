# Codex verification

Date: 2026-08-12
Starting HEAD: `ad7af636f0d2ae3a57838e11562aadde6d33f657`
Scope: Replace the current Godot World Rules panel's legacy authoring UI with `CalculationWorldRule` authoring.

## Changed files

- `apps/godot/scripts/world_editor_world_rules_panel.gd`
- `apps/godot/translations/clife_editor.en.po`
- `apps/godot/translations/clife_editor.ru.po`
- `reports/codex/LATEST.md`

## Facade API used

- `get_calculation_world_rules()`
- `add_calculation_world_rule(source_key, calculation_id, input_bindings, output_bindings)`
- `change_calculation_world_rule(index, source_key, calculation_id, input_bindings, output_bindings)`
- `remove_calculation_world_rule(index)`

## Commands

- `cmake --build --preset vs2022-debug` — PASS
- `ctest --preset vs2022-debug` — PASS (3/3 tests)
- `./scripts/build_godot.ps1 -Configuration Debug` — PASS
- `git diff --check` — PASS
- `Get-Command godot` — PASS: no executable available on `PATH`

## Result

The panel reads, creates, changes, and removes calculation world rules. It binds every selected Calculation input to source residual, a runtime Value, or an ObjectCharacteristic, and every output to a runtime Value.

## Notes

- Legacy `WorldRule` backend remains unchanged but is no longer used by the current World Rules panel.
- No Godot project parsing or interactive UI smoke test ran because a Godot executable is not available on `PATH`.
- Release build/test was not run; it is not required for this focused UI change.
