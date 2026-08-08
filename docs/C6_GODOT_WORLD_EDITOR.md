# C6 — Godot World Editor foundation

Status: **implemented editor foundation**

C6 makes Godot 4.7.1 the primary CLife development and world-editing host. The C5 Unreal Engine 5.8 adapter remains a validated alternative rendering host; it is neither removed nor required to adopt the C6 editor facade.

## 1. Dependency and ownership boundary

```text
clife_core <- clife_world <- clife_presets <- Godot adapter/editor
```

`WorldDefinition` is the canonical mutable document. It contains names, stable identities, object templates, initial values, genome functions, world rules and semantic host bindings. `RuntimeWorld` is compiled from a document snapshot and owns calculator-backed runtime objects. Godot owns widgets, picking, the preview sphere, frame accumulation and the `ObjectId -> Node3D` view map.

No Godot type appears in core, world or presets. CLife does not call Godot and does not expose a mutable `Calculator` through the extension.

## 2. EDIT / RUN lifecycle

```text
EDIT: mutable WorldDefinition, no RuntimeWorld
  |
  | Run: copy document, compile snapshot, instantiate selected TemplateId
  v
RUN: immutable run snapshot + RuntimeWorld + preview ObjectId
  |
  | Stop: destroy runtime and snapshot
  v
EDIT: the edited WorldDefinition is preserved
```

Definition mutations are rejected while RUN is active. `Reset` remains in RUN, reconstructs the runtime from the snapshot captured by Run, resets the tick to zero and pauses automatic ticking. It does not discard edits made before Run. There is no live model mutation or hot reload.

## 3. Structured Godot facade

`CLifeWorldEditor` is a small GDExtension `RefCounted` facade. It owns the editable definition, selected template, optional run snapshot/runtime/object, fixed-tick accumulator, host input values and last validation error.

Queries return Godot `Array` values containing `Dictionary` records:

- `get_values()` and `get_templates()`;
- `get_function_types()`;
- `get_initial_values(template_id)`;
- `get_genome(template_id)`;
- `get_world_rules()`;
- `get_bindings(template_id)` and `get_host_capabilities()`;
- `get_host_inputs()`, `get_host_outputs()` and `get_runtime_values()`.

`ValueKey`, `TemplateId`, indices and `ObjectId` cross the boundary as integers. Names are display data and list order is not identity. Mutation methods map directly to explicit `WorldDefinition` operations. Every exposed mutation/run operation catches domain/runtime exceptions and reports a message through `get_last_error()`.

## 4. Editor UI

The Godot scene builds an editor layout with ordinary Controls:

- a World tree containing Values, Templates and World Rules;
- add, rename and delete operations for values/templates;
- initial-value, genome-function and host-binding forms in the template inspector;
- world-rule forms using stable value keys;
- a central 3D viewport and a right Inspector;
- Run/Stop and Play/Pause/Step/Reset controls;
- a visible validation/status line.

The startup document is `presets::make_first_world_preset()`. GDScript does not contain special cases for Light, Energy, UsedEnergy or Temperature and does not consume C5 summary strings. Adding or renaming values changes the UI through structured queries alone.

## 5. Runtime preview and host inputs

Run compiles the current definition snapshot, instantiates the selected template and registers the result in a Godot `Dictionary<ObjectId, Node3D>`. Clicking the sphere uses Godot `Area3D` ray picking and selects the runtime object. The runtime Inspector then enumerates every value in the run definition and reads its current amount by `ObjectId + ValueKey`.

Input controls are generated from the selected template's `Input` host bindings. Each channel keeps a host-side numeric value, defaults to zero, and is staged before every runtime tick. The first-world `world.light` channel is seeded to `1.0` in C++ using preset metadata, not by GDScript.

`HostBinding` remains engine-independent world data: it connects a stable `ValueKey` to a semantic channel string. The Godot Host Capability Registry is separate adapter data describing which of those channels this host can execute. C6.1 intentionally registers only:

- Input `world.light` (`World Light`);
- Output `geometry.volume` (`Cell Volume`).

The binding editor offers registered capabilities instead of arbitrary new channel strings. Existing bindings that are absent from the registry remain visible as unsupported/legacy entries and can be removed; their presence does not prevent the document from loading. Capability validation is not part of `WorldDefinition`.

`get_host_outputs()` enumerates every active preview-object Output binding as structured `{object_id, channel, value_key, amount}` data. It reads the amount from `RuntimeWorld` through the binding's `ValueKey`; there are no specialized Organic, volume or temperature getters.

For `geometry.volume`, the Godot view interprets the bound scalar as sphere volume. Nominal volume `1` has uniform scale `1`, so presentation computes `scale = cbrt(max(volume, 0))`. Thus volume `8` produces scale `2` and volume `27` produces scale `3`. This geometry conversion belongs only to Godot. CLife value names have no geometry semantics: renaming `Organic` to `Biomass`, or rebinding another arbitrary value to `geometry.volume`, leaves the execution mechanism unchanged.

The first-world document now includes ordinary value `Organic`, initial amount `10`, and `Organic -> geometry.volume`. Temperature no longer controls preview scale. Light continues to affect numerical simulation without changing cell volume.

## 5.1 Godot UI localization

The Godot editor UI uses `TranslationServer` and standard PO resources under `apps/godot/translations/`. Russian is the default for a clean user profile; the header selector switches between Russian and English immediately. The selected locale is the only setting persisted in `user://settings.cfg` and never enters `WorldDefinition`.

Translation keys identify application captions, help and status messages. User-owned world names and stable semantic channel strings are formatted into translated UI without being translated or mutated. The Godot capability registry similarly exposes stable `display_key` values such as `capability.world_light`; GDScript resolves them through `tr()`, while `world.light` and `geometry.volume` remain unchanged.

Changing locale rebuilds only the Godot `Control` hierarchy. The editor facade, editable definition, active runtime snapshot, tick counter, selected template and runtime-object selection remain alive.

## 6. Fixed tick

Automatic simulation is 10 Hz. Godot frame delta only advances an accumulator. Every accumulated `0.1` seconds stages all declared inputs and calls `RuntimeWorld::step()` once. Manual Step performs exactly one CLife tick. Pause leaves the runtime alive; Play resumes accumulator-driven stepping.

## 7. Persistence and future structural changes

C6 documents live only for the application session. There is no JSON, YAML, resource-based canonical storage, undo/redo or node graph.

`CreateNode` and division remain reserved for a later stage. Calculator output may eventually create structural intent; runtime mutations will be collected after calculator execution, applied only at a tick boundary, and exposed to hosts through explicit deltas/events. Newly created objects will first participate in the following tick. The Godot object map is already a collection and does not assume that one permanent Cell is the complete object set.

## 8. Build and run

```powershell
.\scripts\build_godot.ps1 -Configuration Debug
.\scripts\build_godot.ps1 -Configuration Release
godot --editor --path .\apps\godot
```

Normal core/world builds remain independent of Godot and Unreal installations.
