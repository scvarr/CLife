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
- **Объекты** — `ObjectTemplate` с ordered `GenomeFunctionInstance`, semantic genome preview, one-step runtime proof и read-only material/characteristic preview.
- **Внешние входы** — Godot host configuration: host channel → World Quantity (`ValueKey`) + test value. Она не записывается в ObjectTemplate HostBinding.

One-step proof временно запускает выбранный template, подаёт внешние inputs для preview object через facade/direct runtime input, выполняет один tick, читает runtime values и останавливает runtime. Он не создаёт постоянный runtime object или simulation screen.

## Persistence and lifecycle

**Сохранить** записывает committed `WorldDefinitionSnapshot` в `user://current_world.clife.json` и Godot-specific external-input config в `user://current_world.godot.json`. **Загрузить мир** загружает единственный current world и, если существует, его host config. **Новый мир** всегда получает пустую definition и пустое host-config state; он не загружает сохранённый мир автоматически.

Пока нет multi-world browser, Save As, autosave и dirty tracking.

## Legacy development stand

`scenes/main.tscn`, `scripts/main.gd` и его Function Library остаются legacy/development stand и всё ещё могут быть открыты вручную в Godot. Они не являются основным current UX.
