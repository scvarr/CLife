# Codex verification

Date: 2026-08-12
Starting HEAD: `60de5d02e67e0a7f55b02f05420e8f5d879990d7`
Scope: Separate static construction `MaterialId`/`MaterialDefinition` from mutable runtime `ValueKey`, including world compilation, snapshots, Godot native facade, and current editor material authoring.

## Commands

- `cmake --build --preset vs2022-debug` — PASS
- `ctest --preset vs2022-debug` — PASS (3/3 tests)
- `./scripts/build_godot.ps1 -Configuration Debug` — PASS
- `git diff --check` — PASS

## Result

The world and Godot native builds pass. Construction materials compile into the static phenotype only and no longer seed calculator runtime values.

## Notes

- Release build/test was not run: it is not required for this iteration by `reports/codex/README.md` and was not separately requested.
- A Godot executable was not available on `PATH`, so no project parsing or visual smoke test was run.
- Snapshot schema is now 9; schema-8 material bindings are intentionally incompatible because they used `ValueKey` rather than `MaterialId`.
