# C7 — Genotype → phenotype foundation

Статус: **реализовано**

## 1. Три разных вида данных

```text
genotype                phenotype                         runtime state
independent values  ->  deterministic derived values  ->  tick-changing amounts
```

- **Genotype** — экземпляры типов функций и только независимые потенциально наследуемые параметры.
- **Phenotype** — неизменяемый результат компиляции genotype по законам текущего `WorldDefinition`.
- **Runtime state** — изменяемое между тиками состояние: обычные `Value` объекта и внутренние amounts runtime-функций, например накопителя.

Если значение однозначно выводится из genome parameters и world laws, оно не принадлежит genome. Поэтому `organic_size` не хранится рядом с `capacity`: оно вычисляется при phenotype compilation.

Сейчас уже существует **семантический genotype**: `GenomeFunctionInstance` содержит `FunctionTypeId` и независимые `ParameterId -> Amount`. Именно его собирает authoring UI, а затем `compile_phenotype()` строит phenotype. Физического genome как byte/string sequence, encoding, decoding, folding или mutation representation пока нет.

```text
сейчас:   UI/world authoring -> semantic genotype -> phenotype compilation
будущее:  encoded genome -> decoding -> semantic genotype -> phenotype compilation
```

Способ будущей кодировки не определён.

## 2. World function types

`FunctionTypeDefinition` имеет стабильный `FunctionTypeId`, display name, определения независимых и производных параметров и необязательное описание numeric process. Каждый параметр имеет стабильный `ParameterId`; display name не используется как runtime identity.

`GenomeFunctionInstance` ссылается на `FunctionTypeId` и хранит пары `ParameterId -> Amount` только для `genome_parameters`. Производные параметры отсутствуют в этой записи.

Тип функции может компилироваться либо в обычный conversion process, либо в универсальный buffer process. Conversion задаёт input/output `ValueKey`; buffer подключается к одному `ValueKey` как дополнительный источник и потребитель, но не создаёт ребро `Value -> Value` в DAG. Специальных типов `Energy Storage`, `Energy` или `Organic` в core нет.

World data также задаёт material contributions шаблона и типов функций. Contribution указывает материал через стабильный `ValueKey` и число либо phenotype expression. При компиляции contributions суммируются и добавляются к явным initial values объекта.

## 2.1 Нормированные параметры и количественная семантика

Принцип authoring-направления: независимый genome parameter по возможности описывает относительное свойство конкретной конструкции, а не несёт абсолютный масштаб мира. Например при world base processing rate `1 unit / tick` параметр `ThroughputFactor = 1.5` означает 150% базовой производительности:

```text
actual throughput = ThroughputFactor * world base processing rate
```

То есть это не само по себе `1.5 units / tick`. Текущий C++ API всё ещё хранит обычные `Amount`; системы units/dimensions в коде пока нет. До её появления при ручной сборке первого мира допустима convention: `ThroughputFactor = 1.0` означает базовую производительность мира, а `1.5` — 150%.

Следует различать три уровня количественной семантики:

1. **Единица конкретного Value.** Концептуально Value должен определять, что считается и в какой единице выражен amount: `Свет = 5` мог бы означать `5 L`, а `Энергия = 5` — `5 E`. Это ещё не реализовано в `ValueDefinition`.
2. **Мировой масштаб или закон преобразования.** Отношение `10 L Света -> 1 E Энергии` принадлежит миру. Это не conversion одной физической величины наподобие `1000 J = 1 kJ`, а отдельный закон между разными Value.
3. **Параметр конкретной конструкции.** `ThroughputFactor = 1.5` и `Efficiency = 0.8` принадлежат функции клетки; они не должны повторять мировое отношение `10 L -> 1 E` в каждом экземпляре.

Иллюстрация, а не универсальная формула CLife:

```text
Свет: unit = L
Энергия: unit = E
World: base Light processing rate = 1 L / tick
World: ideal Light -> Energy ratio = 10 L -> 1 E
Cell function: ThroughputFactor = 1.5, Efficiency = 0.8
```

При достаточном входном Свете maximum processed Light равен `1.5 L / tick`; идеальный энергетический эквивалент — `1.5 / 10 = 0.15 E / tick`. Последующее разделение на полезный результат и потери может учитывать `Efficiency`, но конкретная формула КПД здесь не закрепляется.

Если два представления описывают одну величину (`EnergyJ`, `EnergyKJ`), `1000 J = 1 kJ` — обычное масштабирование единиц. `Свет -> Энергия` — закон преобразования мира. Текущий CLife не обязан моделировать это различие отдельными типами, но документация различает его концептуально.

## 3. Expressions

Derived parameter expression компилируется при редактировании определения в детерминированную postfix-инструкцию. Поддерживаются:

- finite numeric literals;
- parameter references;
- `+`, `-`, `*`, `/` и parentheses;
- binary `min(a, b)` и `max(a, b)`.

Имя параметра разрешается в стабильный `ParameterId` один раз при компиляции expression. Последующее переименование function type или parameter metadata не меняет поведение формулы. Derived definitions вычисляются в declaration order, поэтому более поздняя формула может ссылаться на уже определённый derived parameter. Forward references и cycles этим минимальным C7 API не создаются.

Division by zero, unknown references, malformed syntax и любой non-finite result являются явными validation errors. Expression evaluator не имеет side effects, engine access, loops или scripting.

Phenotype expressions вычисляются при genotype → phenotype compilation. Отдельно существует `CalculationDefinition`: библиотека чистых функций с именованными inputs/outputs. Binding calculation к runtime/tick пока не реализован и не должен смешиваться с phenotype formulas.

## 4. Phenotype compilation and runtime

`compile_phenotype(WorldDefinition, TemplateId)` проверяет полный набор независимых параметров, вычисляет derived values ровно один раз и возвращает `CompiledPhenotype` с read-only public API. Изменение genome parameter требует новой компиляции phenotype.

`RuntimeWorld` компилирует phenotype при создании и строит calculator `Program` из conversion и buffer processes. Формулы genotype → phenotype не вычисляются на каждом tick. `RuntimeWorld::phenotype(ObjectId)` предоставляет const-доступ к скомпилированному phenotype объекта без раскрытия mutable `Calculator`; `function_states(ObjectId)` отдельно возвращает изменяемое состояние runtime-функций.

## 5. Flow values и buffer surplus/deficit semantics

Обычный `Value` во время тика сначала разрешается как normal bus. `normal_supply` — текущий amount value от normal sources; `normal_demand` — сумма throughput-запросов обычных conversion `Function` consumers. Buffers — отдельные stateful primitives, а не обычные source или consumer этой arbitration.

При `normal_supply >= normal_demand` все обычные consumers получают полный throughput. Только оставшийся `surplus` может зарядить buffers, пропорционально их текущим запросам:

```text
charge_demand = min(Capacity - stored, Throughput)
```

При `normal_supply < normal_demand` buffers могут разрядиться только для покрытия `deficit`, пропорционально доступным offers:

```text
offer = min(stored, Throughput)
```

После actual discharge доступный объём пропорционально распределяется между обычными `Function` consumers по их throughput demands. У обычных consumers нет приоритетов; declaration order не задаёт приоритет и для нескольких buffers. В одном разрешении value buffer может либо receive surplus, либо supply deficit, либо не менять state; одновременно receive и supply он не может и не заряжается из собственного offer. Leakage остаётся отдельным существующим шагом state update.

Непринятый buffers surplus остаётся на `Value` и переносится в отдельный `end_buffer`. Pipeline текущего тика не может читать его. После заполнения end-buffer применяются end rules, затем на следующем тике snapshot заменяется новым. Первый мир использует `remaining Energy -> END Heat` и `END Heat -> Temperature * 0.1`.

## 6. First-world proof

Первый мир определяет `Energy Storage` как phenotype-only function type:

```text
Genome parameter:
    Capacity = 5

World-derived parameter:
    OrganicSize = Capacity / 5
    Throughput = Capacity * 0.3
    Leakage = 0

Compiled phenotype:
    Capacity = 5
    OrganicSize = 1
    Throughput = 1.5
```

При `Capacity = 5` base contribution клетки и трёх функций вместе с дополнительным размером накопителя дают `Organic = 5`. После изменения только `Capacity` на `10` новая компиляция даёт `OrganicSize = 2`, `Throughput = 3` и total `Organic = 6`. `geometry.volume` уже связан с этим `ValueKey`, поэтому Godot меняет объём без поиска имени Organic.

`Energy Storage` хранит runtime amount, изначально равный нулю. При `Light = 1/tick`, `Energy Use throughput = 0.5`, `Capacity = 5` и `Throughput = 1.5` normal bus сначала полностью обслуживает Energy Use, а затем storage получает surplus:

```text
Tick 1: UsedEnergy = 0.5, Storage received = 0.5, Storage stored = 0.5
Tick 2: UsedEnergy = 0.5, Storage received = 0.5, Storage stored = 1.0
```

На следующем tick при `Light = 0` storage покрывает deficit обычного consumer:

```text
Storage supplies = 0.5, UsedEnergy = 0.5, Storage stored = 0.5
```

Пока storage может принять этот surplus, он не попадает в `END Heat`; только не принятый buffers остаток остаётся на `Energy` для существующего end-buffer transfer.

## 7. Godot editor

Godot facade возвращает structured function types, calculations, genome instances, compiled material totals, runtime function states и last end-buffer. Template inspector показывает редактируемые Genome parameters, derived parameters и материальную стоимость. Calculation library остаётся отдельной от phenotype/runtime.

Function types доступны отдельным разделом world tree. Все новые application captions проходят через существующие RU/EN PO resources. Function/parameter display names остаются world data и не переводятся автоматически.

## 8. Границы текущей реализации

Не реализованы mutation, inheritance transport, biosynthesis, thermosynthesis, reproduction, division, topology, multi-cell, физическое genome encoding и runtime formula binding/DSL. Structural operations по-прежнему зарезервированы для tick boundary.

Сохранение authoring `WorldDefinition` уже реализовано через `WorldDefinitionSnapshot` и host serialization. Не реализованы сохранение `RuntimeWorld`, эволюционной популяции и биологическая сериализация физического genome.
