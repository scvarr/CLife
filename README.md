# CLife

## Documentation map

**Current / normative:** [C0](docs/C0_CONCEPT.md), [C1](docs/C1_ARCHITECTURE.md), [C3](docs/C3_CALCULATOR_MODEL.md), [C4](docs/C4_HOST_WORLD_EDITOR_MODEL.md), [C6](docs/C6_GODOT_WORLD_EDITOR.md), [C7](docs/C7_GENOTYPE_PHENOTYPE.md).

**Historical:** [C2](docs/C2_WORLD_DEFINITION.md) is the former Field/Resource/State/Matter/Measure model; [C5](docs/C5_DUAL_ENGINE_ADAPTERS.md) is a historical dual-engine integration slice.

**Non-normative:** [FUTURE_IDEAS](docs/FUTURE_IDEAS.md).

Historical documents must not override current/normative documents. The current practical stage is manual construction of a world in the empty Godot editor: Calculations, Function Library and processes, Units/UnitConversions, Templates, ObjectCharacteristics/ObjectConstruction and the first cell.

CLife — проект искусственной жизни с переносимым C++-ядром симуляции и отделёнными от него приложениями/графическими движками.

Это не механический перенос старого Godot-прототипа. Старый проект используется как доказательство работоспособности отдельных идей, источник проверенных формул и будущий golden reference.

## Текущий этап

Практический этап — пошаговое построение мира через пустой Godot editor: authoring `Value`, самостоятельных `CalculationDefinition`, `ObjectTemplate`, типов функций, связей и правил мира. Архитектура проверяется ручной сборкой клетки, а не загрузкой заранее заданного демонстрационного мира.

Концептуальная база C0 зафиксирована как рабочий baseline: [`docs/C0_CONCEPT.md`](docs/C0_CONCEPT.md).
Архитектурные правила реализации: [`docs/C1_ARCHITECTURE.md`](docs/C1_ARCHITECTURE.md).
Компактный calculator core реализован: [`docs/C3_CALCULATOR_MODEL.md`](docs/C3_CALCULATOR_MODEL.md).
Текущая модель world/runtime и host-интеграции: [`docs/C4_HOST_WORLD_EDITOR_MODEL.md`](docs/C4_HOST_WORLD_EDITOR_MODEL.md).
Dual-engine integration: [`docs/C5_DUAL_ENGINE_ADAPTERS.md`](docs/C5_DUAL_ENGINE_ADAPTERS.md).
Current Godot editor model: [`docs/C6_GODOT_WORLD_EDITOR.md`](docs/C6_GODOT_WORLD_EDITOR.md).
Genotype / phenotype model: [`docs/C7_GENOTYPE_PHENOTYPE.md`](docs/C7_GENOTYPE_PHENOTYPE.md).
Журнал ненормативных будущих идей: [`docs/FUTURE_IDEAS.md`](docs/FUTURE_IDEAS.md).

Первый клеточный мир остаётся основным тестовым preset и предметным языком разработки, но универсальный core больше не строится вокруг специальных категорий `Field / Resource / State / Matter`.

## Сборка

Источник истины сборки — **CMake**, а не `.sln/.vcxproj`.

Для Visual Studio 2022:

1. Открыть корень репозитория как папку/CMake project.
2. Выбрать configure preset `vs2022`.
3. Собирать targets `clife_core`, `clife_world`, `clife_presets`, `clife_headless` или preset `vs2022-debug` / `vs2022-release`.
4. Тесты запускаются через CTest preset `vs2022-debug` / `vs2022-release`.

Сгенерированные Visual Studio solution/project-файлы находятся только в `out/` и в Git не коммитятся.

## Архитектурная граница

```text
clife_core <- clife_world <- headless / Godot
                         <- clife_presets <- preset tests/examples
                         <- Unreal (separate deferred host)
```

`clife_core` не имеет `main()` и содержит универсальный числовой runtime. `clife_world` содержит authoring-модель мира, expressions, calculations, phenotype и runtime-объекты. Godot adapter зависит непосредственно от `clife_world`; preset не является его обязательным источником. Хост подаёт внешние значения, вызывает fixed ticks и читает результаты; CLife не вызывает engine callbacks.

Host/engine может поставлять внешние числовые значения и интерпретировать рассчитанные значения геометрически/визуально. Законы геномного конвейера и world rules остаются явной частью модели мира, а не скрытым порядком вызовов engine.

## Engine hosts

Обычная CMake/CI сборка не требует движков и остаётся основной проверкой проекта.

Godot 4.7.1 GDExtension world editor (stable Godot 4.5 API baseline):

```powershell
.\scripts\build_godot.ps1 -Configuration Debug
godot --editor --path .\apps\godot
```

Unreal Engine 5.8 plugin/demo:

```powershell
.\scripts\build_unreal_demo.ps1 -UnrealEngineRoot 'C:\Program Files\Epic Games\UE_5.8'
```

Godot is the primary editor/development host in C6. Unreal remains the validated C5 alternative renderer host and is not abandoned. Full editor lifecycle details are in [`docs/C6_GODOT_WORLD_EDITOR.md`](docs/C6_GODOT_WORLD_EDITOR.md); dual-engine build/run boundaries remain in [`docs/C5_DUAL_ENGINE_ADAPTERS.md`](docs/C5_DUAL_ENGINE_ADAPTERS.md).

## Первый world preset

`first_world` сохранён как отдельный example/test preset. Godot editor его автоматически не загружает: новый editor начинает с пустого `WorldDefinition`, а пользователь создаёт данные мира через UI.

Первый мир CLife использует **клетку как атомарный активный объект**. Это удобная предметная аналогия, а не фундаментальное ограничение универсального calculator core.

Клетка содержит числовые значения и конечный геномный конвейер функций. Человеческие понятия вроде света, энергии, температуры, материала и пропорции являются именами и правилами первого мира.

## Ключевое ограничение

**Взрослая клетка не растёт.**

Геном определяет законченный phenotype при формировании клетки. Рост существует только на стадии зачатка/зародыша.

## Historical reference

Старый исследовательский прототип:

- repository: `https://github.com/scvarr/lab3d.git`
- accepted stage: `R7`
- commit: `71a95bbc12b72102460e21045840d6dbd2f734ac`

Он не является архитектурной основой CLife и не должен переноситься механически.
