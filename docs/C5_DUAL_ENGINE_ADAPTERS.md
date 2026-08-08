# C5 — dual-engine Godot + Unreal adapters

Статус: **реализованный первый визуальный integration slice**

C5 одновременно проводит один и тот же клеточный preset через Godot и Unreal. Параллельная реализация нужна не ради общего renderer, а как проверка того, что численная модель и host contract действительно не зависят от lifecycle, UI и rendering API одного движка.

## 1. Зависимости

```text
clife_core <- clife_world <- clife_presets
                                  |
                       +----------+----------+
                       |                     |
                Godot GDExtension      Unreal plugin
                       |                     |
                 Godot scene/UI        Actor + Slate UI
```

Engine headers отсутствуют в `clife_core`, `clife_world` и `clife_presets`. CLife не вызывает движок. Каждый adapter собирает input, вызывает `DemoSession`, читает output и обновляет собственное представление.

## 2. Общий preset и session

`clife_presets` содержит единственное определение первого мира и стабильные identities `Light`, `Energy`, `UsedEnergy`, `Temperature`, `Cell`. Там же находятся channel names и read-only UI summaries.

`DemoSession` владеет `RuntimeWorld`, создаёт Cell, реализует deterministic reset и общий host lifecycle:

- `10 Hz`, то есть один tick каждые `0.1` секунды;
- `advance_time(frame_delta)` накапливает engine time и выполняет целое число ticks;
- `step()` всегда выполняет ровно один ручной tick;
- `reset()` реконструирует runtime, возвращает tick `0`, Light `1.0`, Temperature `0.2` и paused state.

Engine delta не передаётся в `Calculator`.

## 3. Host channels

Оба adapter используют одинаковые bindings:

```text
world.light       -> Light
UsedEnergy        -> cell.used_energy
Temperature       -> cell.temperature
```

Light slider задаёт staging input `world.light`. UI и движок не вычисляют Energy или Temperature самостоятельно.

## 4. Object mapping и rendering

Каждый adapter содержит явную коллекцию `ObjectId -> engine representation` и перестраивает её при reset:

- Godot: `Dictionary<ObjectId, Node3D>`;
- Unreal: `std::map<ObjectId, TWeakObjectPtr<USceneComponent>>`.

Текущий Cell отображается sphere mesh. Одинаковое view-only правило в обоих движках:

```text
uniform_scale = 1.0 + Temperature
```

Это не закон CLife и не находится в canonical world data.

## 5. Godot

Baseline проекта: Godot 4.7.1, Windows x64. Native extension собирается на стабильном совместимом API Godot 4.5, поскольку demo не использует API 4.7:

```text
godot-cpp tag: godot-4.5-stable
commit: e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77
```

Dependency загружается только при opt-in build и закреплена exact commit. Обычный CMake configure её не ищет и не загружает.

Сборка:

```powershell
.\scripts\build_godot.ps1 -Configuration Debug
.\scripts\build_godot.ps1 -Configuration Release
```

Запуск после установки Godot 4.7.1:

```powershell
godot --editor --path .\apps\godot
# либо
godot --path .\apps\godot
```

`CLifeDemoRuntime` — минимальный `RefCounted` wrapper: reset, running, Light, fixed/manual step, tick и output getters. Scene создаёт Camera3D, lights, sphere и обычный Control UI с World/Values/Genome/Rules/Bindings, Inspector и нижними controls.

## 6. Unreal Engine

Baseline: Unreal Engine 5.8, Visual Studio 2022, Win64. `CLifeRuntimeExternal` имеет `ModuleType.External` и линкует CMake-built static libraries. UBT не компилирует исходники calculator/world.

Сборка и staging:

```powershell
.\scripts\build_unreal_clife.ps1 -Configuration Release
.\scripts\build_unreal_demo.ps1 -UnrealEngineRoot 'C:\Program Files\Epic Games\UE_5.8'
```

Запуск editor:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
    .\apps\unreal\CLifeDemo.uproject
```

Game smoke без committed map asset можно запустить на engine Entry map:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
    .\apps\unreal\CLifeDemo.uproject /Engine/Maps/Entry -game -windowed
```

Runtime plugin автоматически создаёт `ACLifeDemoHost` в game world. Actor владеет session, sphere/camera/light components и object map. Code-defined Slate UI предоставляет Play, Pause, Step, Reset, tick, Light slider, текущие значения и semantic summaries без `.uasset`.

## 7. Semantic parity и ownership

Общее:

- определения values/genome/rules/bindings;
- stable identities и channel names;
- numerical runtime;
- fixed tick, play/pause/manual step/reset semantics;
- UI information semantics.

Engine-specific:

- frame callback и input plumbing;
- Node3D/Actor/component mapping;
- camera, lights, meshes and viewport;
- Control widgets или Slate widgets;
- visual scale application.

Общий widget toolkit или renderer interface не вводится.

## 8. Future structural operations

Adapters уже используют object maps, а не permanent global Cell pointer. Будущий runtime будет собирать structural intents после calculator phase, применять mutations только на tick boundary и выдавать host явные deltas/events. Новый объект начнёт genome execution со следующего tick. Engine adapter создаст/удалит representation после получения delta; `Calculator` по-прежнему не будет знать об engine или напрямую создавать объекты.
