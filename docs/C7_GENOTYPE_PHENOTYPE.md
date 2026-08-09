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

## 2. World function types

`FunctionTypeDefinition` имеет стабильный `FunctionTypeId`, display name, определения независимых и производных параметров и необязательное описание numeric process. Каждый параметр имеет стабильный `ParameterId`; display name не используется как runtime identity.

`GenomeFunctionInstance` ссылается на `FunctionTypeId` и хранит пары `ParameterId -> Amount` только для `genome_parameters`. Производные параметры отсутствуют в этой записи.

Тип функции может компилироваться либо в обычный conversion process, либо в универсальный buffer process. Conversion задаёт input/output `ValueKey`; buffer подключается к одному `ValueKey` как дополнительный источник и потребитель, но не создаёт ребро `Value -> Value` в DAG. Специальных типов `Energy Storage`, `Energy` или `Organic` в core нет.

World data также задаёт material contributions шаблона и типов функций. Contribution указывает материал через стабильный `ValueKey` и число либо phenotype expression. При компиляции contributions суммируются и добавляются к явным initial values объекта.

## 3. Expressions

Derived parameter expression компилируется при редактировании определения в детерминированную postfix-инструкцию. Поддерживаются:

- finite numeric literals;
- parameter references;
- `+`, `-`, `*`, `/` и parentheses;
- binary `min(a, b)` и `max(a, b)`.

Имя параметра разрешается в стабильный `ParameterId` один раз при компиляции expression. Последующее переименование function type или parameter metadata не меняет поведение формулы. Derived definitions вычисляются в declaration order, поэтому более поздняя формула может ссылаться на уже определённый derived parameter. Forward references и cycles этим минимальным C7 API не создаются.

Division by zero, unknown references, malformed syntax и любой non-finite result являются явными validation errors. Expression evaluator не имеет side effects, engine access, loops или scripting.

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

Godot facade возвращает structured function types, genome instances, compiled material totals, runtime function states и last end-buffer. Template inspector показывает редактируемые **Genome parameters**, read-only **Derived parameters** и материальную стоимость. RUN inspector показывает capacity/throughput/leakage, stored/received/supplied накопителя и END Heat.

Function types доступны отдельным разделом world tree. Все новые application captions проходят через существующие RU/EN PO resources. Function/parameter display names остаются world data и не переводятся автоматически.

## 8. Не входит в C7.1

C7.1 не реализует mutation, inheritance transport, biosynthesis, thermosynthesis, reproduction, division, topology, multi-cell, ненулевой закон leakage, save/load или runtime formula DSL. Structural operations по-прежнему зарезервированы для tick boundary.
