# C6 — Godot world editor

Статус: **CURRENT / NORMATIVE**.

Приложение стартует с `scenes/main_menu.tscn`. Главное меню предлагает **Новый мир**, **Загрузить мир** и **Выход**; до выбора пользователь не создаёт и не открывает мир. Новый мир открывает пустой `scenes/world_definition_editor.tscn`.

Новый user-facing editor имеет sidebar с разделами:

```text
Единицы
Величины мира
Преобразования
Формулы
Функции
Характеристики объектов
Конструкция
Объекты
Внешние входы

Сохранить
```

## Реализованные authoring sections

- **Единицы** — user-authored UnitDefinition с обозначением и комментарием.
- **Величины мира** — текущие backend `ValueDefinition`; это не genome parameters.
- **Преобразования** — `UnitConversionDefinition` между единицами.
- **Формулы** — `CalculationDefinition`: inputs, ordered outputs и expressions. Поздний output может ссылаться на предыдущий.
- **Функции** — текущий semantic scaffold `FunctionTypeDefinition`: genome parameters, Calculation binding, process, multiple outputs и material construction contributions. Preview вида `02 | 1.0` — семантическая запись, не physical hex genome.
- **Характеристики объектов** — `ObjectCharacteristicDefinition`.
- **Конструкция** — singleton `ObjectConstructionDefinition`; её inputs могут использовать base/function contributions и `material_amount`.
- **Объекты** — `ObjectTemplate` с ordered `GenomeFunctionInstance`, semantic genome preview, one-step runtime proof, read-only material/characteristic preview и 3D ShapePhenotype preview.
- **Внешние входы** — Godot host configuration: host channel → World Quantity (`ValueKey`) + test value. Она не записывается в ObjectTemplate HostBinding.

Objects editor provides a minimal stateful one-object runtime proof: `Start`, manual single-tick `Step`, `Reset`, and `Stop`. It displays the current tick, runtime Values, and every compiled buffer's stored amount plus received/supplied amounts from the last tick. Before every `Step`, the current Godot external-input test configuration is staged again through direct runtime input. This remains an editor proof, not a simulation screen, real-time Play mode, physics, or multi-object simulation.

## Shape and construction-volume debug preview

Objects editor uses a batched native facade to sample the engine-neutral current `ShapePhenotype` on Godot-owned fixed directional tessellation. Godot creates the closed preview mesh and corrects its discrete mesh volume uniformly to the explicitly selected final `ObjectCharacteristic` volume. That selected characteristic id remains session-only editor/debug configuration; it does not add a builtin Volume semantic or persist in the world. Therefore the semantic genome controls morphology, while final `ObjectConstruction` still supplies physical preview size.

## Persistence and lifecycle

**Сохранить** записывает committed `WorldDefinitionSnapshot` в `user://current_world.clife.json` и Godot-specific external-input config в `user://current_world.godot.json`. **Загрузить мир** загружает единственный current world и, если существует, его host config. **Новый мир** всегда получает пустую definition и пустое host-config state; он не загружает сохранённый мир автоматически.

Пока нет multi-world browser, Save As, autosave и dirty tracking.

## Legacy development stand

`scenes/main.tscn`, `scripts/main.gd` и его Function Library остаются legacy/development stand и всё ещё могут быть открыты вручную в Godot. Они не являются основным current UX.
