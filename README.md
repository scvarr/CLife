# CLife

CLife — проект искусственной жизни с переносимым C++-ядром симуляции и отделёнными от него приложениями/графическими движками.

Это не механический перенос старого Godot-прототипа. Старый проект используется как доказательство работоспособности отдельных идей, источник проверенных формул и будущий golden reference.

## Текущий этап

**C5 — общий first-world preset и эквивалентные Godot/Unreal adapters.**

Концептуальная база C0 зафиксирована как рабочий baseline: [`docs/C0_CONCEPT.md`](docs/C0_CONCEPT.md).
Архитектурные правила реализации: [`docs/C1_ARCHITECTURE.md`](docs/C1_ARCHITECTURE.md).
Компактный calculator core реализован: [`docs/C3_CALCULATOR_MODEL.md`](docs/C3_CALCULATOR_MODEL.md).
Текущая модель world/runtime и host-интеграции: [`docs/C4_HOST_WORLD_EDITOR_MODEL.md`](docs/C4_HOST_WORLD_EDITOR_MODEL.md).
Dual-engine integration: [`docs/C5_DUAL_ENGINE_ADAPTERS.md`](docs/C5_DUAL_ENGINE_ADAPTERS.md).

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
clife_core <- clife_world <- clife_presets <- headless / Godot / Unreal
```

`clife_core` не имеет `main()` и содержит только компактный числовой evaluator. `clife_world` содержит редактируемое определение мира, его компиляцию и runtime-объекты. Хост подаёт внешние значения, вызывает фиксированные simulation ticks и читает результаты; CLife не вызывает engine callbacks.

Host/engine может поставлять внешние числовые значения и интерпретировать рассчитанные значения геометрически/визуально. Законы геномного конвейера и world rules остаются явной частью модели мира, а не скрытым порядком вызовов engine.

## Engine demos

Обычная CMake/CI сборка не требует движков и остаётся основной проверкой проекта.

Godot 4.7.1 GDExtension (stable Godot 4.5 API baseline):

```powershell
.\scripts\build_godot.ps1 -Configuration Debug
godot --editor --path .\apps\godot
```

Unreal Engine 5.8 plugin/demo:

```powershell
.\scripts\build_unreal_demo.ps1 -UnrealEngineRoot 'C:\Program Files\Epic Games\UE_5.8'
```

Полные build/run инструкции и границы ответственности находятся в [`docs/C5_DUAL_ENGINE_ADAPTERS.md`](docs/C5_DUAL_ENGINE_ADAPTERS.md).

## Первый world preset

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
