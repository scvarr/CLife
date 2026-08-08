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
- `get_initial_values(template_id)`;
- `get_genome(template_id)`;
- `get_world_rules()`;
- `get_bindings(template_id)`;
- `get_host_inputs()` and `get_runtime_values()`.

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

The preview scale remains the C5 view-only rule `1.0 + value`. Its value identity is the stable temperature key supplied by the first-world preset metadata. The name "Temperature" is not searched and the rule is not part of `WorldDefinition` or `Calculator`.

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
