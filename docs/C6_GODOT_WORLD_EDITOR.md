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
Правила мира
Внешние входы

Сохранить
```

## Реализованные authoring sections

- **Единицы** — user-authored UnitDefinition с обозначением и комментарием.
- **Величины мира** — текущие backend `ValueDefinition`; это не genome parameters.
- **Преобразования** — `UnitConversionDefinition` между единицами.
- **Формулы** — `CalculationDefinition`: inputs, ordered outputs и expressions. Поздний output может ссылаться на предыдущий.
- **Функции** — текущий semantic scaffold `FunctionTypeDefinition`: genome parameters, Calculation binding, process, `BufferProcess` (Value, capacity/throughput/leakage sources), multiple outputs, function characteristic contributions и material construction contributions. Источники параметров BufferProcess и contributions выбираются из genome parameter или Calculation output. Preview вида `02 | 1.0` — семантическая запись, не physical hex genome.
- **Характеристики объектов** — `ObjectCharacteristicDefinition`.
- **Конструкция** — singleton `ObjectConstructionDefinition`; её inputs могут использовать base/function contributions и `material_amount`.
- **Объекты** — `ObjectTemplate` с initial Values, ordered `GenomeFunctionInstance`, semantic genome preview, stateful runtime proof, read-only material/characteristic preview и 3D ShapePhenotype preview.
- **Правила мира** — `WorldRuleDefinition`: source Value, end-buffer Value, target Value и target-per-source; их можно создавать, изменять и удалять.
- **Внешние входы** — Godot host configuration: host channel → World Quantity (`ValueKey`) + test value. Она не записывается в ObjectTemplate HostBinding.

Objects editor provides a minimal stateful one-object runtime proof: `Start`, manual single-tick `Step`, `Reset`, and `Stop`. It displays the current tick, runtime Values, current end-buffer values, and every compiled buffer's stored amount plus received/supplied amounts from the last tick. Before every `Step`, the current Godot external-input test configuration is staged again through direct runtime input. This remains an editor proof, not a simulation screen, real-time Play mode, physics, or multi-object simulation.

## Shape and construction-volume debug preview

Objects editor uses a batched native facade to sample the engine-neutral current `ShapePhenotype` on Godot-owned fixed directional tessellation. Godot creates the closed preview mesh and corrects its discrete mesh volume uniformly to the explicitly selected final `ObjectCharacteristic` volume. That selected characteristic id remains session-only editor/debug configuration; it does not add a builtin Volume semantic or persist in the world. Therefore the semantic genome controls morphology, while final `ObjectConstruction` still supplies physical preview size.

## Persistence and lifecycle

**Сохранить** записывает committed `WorldDefinitionSnapshot` в `user://current_world.clife.json` и Godot-specific external-input config в `user://current_world.godot.json`. **Загрузить мир** загружает единственный current world и, если существует, его host config. **Новый мир** всегда получает пустую definition и пустое host-config state; он не загружает сохранённый мир автоматически.

Пока нет multi-world browser, Save As, autosave и dirty tracking.
