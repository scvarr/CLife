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

## 5. Flow values и пропорциональное разрешение

Обычный `Value` во время тика является общим потоком со множеством источников и потребителей. Для каждого value calculator вычисляет суммы предложений и запросов, разрешает `min(total_supply, total_demand)` и одним коэффициентом пропорционально масштабирует все источники, а другим — всех потребителей. Приоритетов между свежим потоком, накопителем и conversion-функциями нет. Результат не зависит от declaration order.

Buffer рассчитывает offer и demand из одного состояния в начале разрешения value:

```text
offer  = min(stored, Throughput)
demand = min(Capacity - stored, Throughput)
stored = stored - actual_supplied + actual_received - Leakage
```

Один buffer может одновременно получить и отдать поток. Неиспользованная часть offer остаётся внутри него.

Остаток обычного временного потока переносится в отдельный `end_buffer`. Pipeline текущего тика не может читать его. После заполнения end-buffer применяются end rules, затем на следующем тике snapshot заменяется новым. Первый мир использует `remaining Energy -> END Heat` и `END Heat -> Temperature * 0.1`.

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

`Energy Storage` хранит runtime amount, изначально равный нулю. При Light `1` первый tick даёт `UsedEnergy = 0.25`, `Stored = 0.75`; второй — `UsedEnergy = 0.4375`, `Stored = 1.3125`.

## 7. Godot editor

Godot facade возвращает structured function types, genome instances, compiled material totals, runtime function states и last end-buffer. Template inspector показывает редактируемые **Genome parameters**, read-only **Derived parameters** и материальную стоимость. RUN inspector показывает capacity/throughput/leakage, stored/received/supplied накопителя и END Heat.

Function types доступны отдельным разделом world tree. Все новые application captions проходят через существующие RU/EN PO resources. Function/parameter display names остаются world data и не переводятся автоматически.

## 8. Не входит в C7.1

C7.1 не реализует mutation, inheritance transport, biosynthesis, thermosynthesis, reproduction, division, topology, multi-cell, ненулевой закон leakage, save/load или runtime formula DSL. Structural operations по-прежнему зарезервированы для tick boundary.
