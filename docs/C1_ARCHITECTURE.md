# C1 — архитектура реализации CLife

Статус: **принято / текущие правила реализации**

C1 фиксирует архитектурные правила реализации. C0 описывает первый клеточный мир, C2 отражает промежуточный этап разделения типов, а текущая модель универсального core зафиксирована в `C3_CALCULATOR_MODEL.md`. При конфликте абстракций core приоритет имеет C3.

## 1. Главный принцип

CLife строится как **переносимое C++-ядро симуляции**, которое не зависит от конкретного renderer/game engine.

```text
clife_core <- clife_world <- headless / Godot
                         <- clife_presets <- preset tests/examples
                         <- Unreal (separate deferred host)
```

`clife_core`:

- не содержит `main()`;
- не создаёт окно;
- не читает клавиатуру/мышь;
- не зависит от Godot/Unreal/Windows API;
- не владеет главным циклом процесса;
- содержит законы и состояние симуляции.

### 1.1. Компактный calculator core

`clife_core` не кодирует предметные категории первого мира как фундаментальные типы. Его минимальная ответственность:

```text
core code:
    numeric values addressed inside objects
    finite genome pipeline evaluation
    proportional competition for shared input
    deterministic fixed tick

world data / runtime:
    human names and semantics of values
    genome function definitions and formulas
    end-of-tick world rules
    external value bindings
    engine/visual bindings
    first-world cell templates and initial configuration
```

Понятия `Field`, `Resource`, `State`, `Matter`, `Measure`, `Property`, `Store` и подобные могут оставаться удобным языком первого world preset или UI, но не требуют отдельных фундаментальных C++-ролей.

Геном описывает способности объекта как конечный конвейер функций. Он не является imperative-программой с универсальными командами `SET/ADD` и не определяет порядок исполнения порядком записей. Совместный спрос на одно значение разрешается пропорционально.

Неизбежные последствия, не являющиеся способностью конкретного genome, задаются world rules. Например первый клеточный мир может в конце tick преобразовать неиспользованную Energy в изменение Temperature по формуле мира.

Host/engine может поставлять внешние числа и интерпретировать рассчитанные числа геометрически или визуально. Core не обязан повторно рассчитывать пространственную физику, которую engine уже предоставляет, но engine также не задаёт скрытым порядком вызовов законы геномного evaluator.

Конкретные правила текущей модели описаны в `C3_CALCULATOR_MODEL.md`.

## 2. Точка входа и жизненный цикл

Точка входа принадлежит **хост-приложению**.

На первом этапе это `clife_headless`:

```text
OS
 |
main()
 |
create Simulation
 |
step()
step()
...
 |
destroy Simulation
 |
return exit code
```

Позднее Godot/Unreal будут собственными хостами и вызывать то же ядро из своего lifecycle.

`Simulation` не получает бесконечный `run()` loop. Хост явно вызывает `step()`.

Владение ресурсами строится через RAII: валидный объект готов к использованию после конструктора, а освобождение принадлежащих ему ресурсов происходит при разрушении объекта. Отдельные `init()/shutdown()` вводятся только если для них появится реальная семантическая необходимость.

## 3. Время симуляции

Core использует **фиксированный simulation tick**.

Результат не должен зависеть от renderer FPS, скорости выполнения кадра или wall-clock времени.

Базовое требование детерминированности:

```text
same initial state
+ same inputs
+ same random seed
+ same tick count
= same simulation result
```

Случайность в будущем передаётся в симуляцию явно и воспроизводимо. Скрытые зависимости от `system_clock`, `random_device` или порядка обхода контейнера не должны менять физику модели.

## 4. Toolchain

Production baseline:

```text
C++20
CMake
CMakePresets.json
Visual Studio 2022 / MSVC v143
x64
```

C++23 preview/latest не является baseline, пока Visual Studio 2022 не предоставляет стабильный полностью поддержанный режим стандарта.

Visual Studio используется как IDE. Источник истины проекта — CMake; вручную поддерживаемые `.sln/.vcxproj` в репозитории не используются.

## 5. Правила современного C++

Используются возможности языка, когда они улучшают корректность, читаемость, владение или производительность.

Базовые правила:

- RAII и value semantics по умолчанию;
- явное владение ресурсами;
- `std::unique_ptr` для уникального динамического владения;
- `std::shared_ptr` только при реальной разделённой семантике владения;
- отсутствие ручных `new/delete` в обычном прикладном коде;
- `const` correctness;
- `enum class` вместо неограниченных enum;
- `[[nodiscard]]` для результатов, которые опасно игнорировать;
- `std::span`, `std::string_view`, `std::optional`, `std::variant`, ranges/concepts и другие средства — когда они соответствуют задаче;
- отсутствие шаблонной/ООП-сложности только ради использования возможностей языка.

## 6. Ошибки

Core не завершает процесс через `exit()` и не управляет UI ошибок.

Различаются:

- нарушение внутреннего invariant — ошибка программы;
- ожидаемая ошибка данных/конфигурации — типизированный результат;
- необработанная ошибка процесса — ответственность host boundary (`main()`/engine adapter), где она логируется и преобразуется в exit/error state.

## 7. Сборочные targets

Текущая структура targets:

```text
clife_core      static library
clife_world     static library
clife_presets   static library
clife_headless  executable host
clife_tests     test executable
```

Godot adapter подключается непосредственно через `clife_world`. `clife_presets` остаётся отдельной библиотекой test/example данных; Unreal остаётся отдельным отложенным host, а не текущим местом editor-разработки.

## 8. Структура каталогов

```text
CLife/
├── CMakeLists.txt
├── CMakePresets.json
├── cmake/
├── include/clife/
├── src/
├── apps/headless/
├── tests/
└── docs/
```

Публичный API ядра находится под `include/clife/`. Реализация — под `src/`.

## 9. Качество сборки

Для собственного C++-кода включаются повышенные предупреждения компилятора и стандартно-согласованный режим MSVC.

Тесты подключены через CTest с первого рабочего commit. Любое новое фундаментальное поведение ядра должно иметь воспроизводимый тест, когда это практически возможно.

## 10. Реализованные вертикальные срезы

Первый lifecycle-срез и компактный calculator из C3 реализованы. C4 добавляет engine-independent world/runtime-слой, не вводя renderer и многоклеточность.

Текущий архитектурный контракт:

```text
host edits/loads WorldDefinition
        ↓ compile
host instantiates RuntimeObject
        ↓
host stages inputs and advances fixed ticks
        ↓
host reads outputs
```

Специальные роли `Field / Resource / State / Matter` не входят в calculator API. Имена и семантика значений принадлежат `WorldDefinition`; скомпилированный runtime использует плотные `ValueId`. Подробный контракт зафиксирован в `C4_HOST_WORLD_EDITOR_MODEL.md`.
