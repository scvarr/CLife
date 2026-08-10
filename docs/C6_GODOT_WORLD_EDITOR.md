# C6 — Godot world editor

Application startup is `scenes/main_menu.tscn`: it offers New World, Load World and Exit without creating or opening a world. `scenes/main.tscn` remains the legacy/development world-editor scene during the gradual UX transition and can still be opened manually in Godot.

New World now opens the separate `world_definition_editor.tscn` with a new empty definition. Its first user-facing authoring section is Units only; the legacy editor remains the development stand.

Статус: **CURRENT / NORMATIVE**.

Editor starts with an empty `WorldDefinition`; `first_world` remains an example/test preset. The world workspace contains Values, Units, Unit Conversions, Calculations, Templates, Function Types shortcuts, World Rules, Object Characteristics and Object Construction.

Function Types open the separate Function Library workspace. It keeps selection, has Construction, Process and Materials tabs, and edits genome parameters, Calculation bindings/sources, processes, material contributions and function characteristic contributions. Context deletion is used where lifecycle API exists.

Calculation remains an inspector editor in the world workspace: inputs, ordered outputs, expression editing, test evaluation and dependency-safe deletion.

Template authoring remains visible even if phenotype compilation fails. Templates hold raw genome, base characteristics and host bindings; a best-effort phenotype preview exposes function sums and final characteristics. Host input selectors offer runtime Values only. Output selectors can use a runtime Value or ObjectCharacteristic, including `geometry.volume`.

The editor persists one host JSON snapshot. It is a working file, not a project browser; runtime state, camera, UI selection and staged inputs are not saved.
