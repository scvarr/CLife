# C7 — Genotype → phenotype foundation

Статус: **реализовано**

## 1. Три разных вида данных

```text
genotype                phenotype                         runtime state
independent values  ->  deterministic derived values  ->  tick-changing amounts
```

- **Genotype** — экземпляры типов функций и только независимые потенциально наследуемые параметры.
- **Phenotype** — неизменяемый результат компиляции genotype по законам текущего `WorldDefinition`.
- **Runtime state** — значения объекта в `Calculator`, которые изменяются каждый simulation tick.

Если значение однозначно выводится из genome parameters и world laws, оно не принадлежит genome. Поэтому `organic_size` не хранится рядом с `capacity`: оно вычисляется при phenotype compilation.

## 2. World function types

`FunctionTypeDefinition` имеет стабильный `FunctionTypeId`, display name, определения независимых и производных параметров и необязательное описание numeric process. Каждый параметр имеет стабильный `ParameterId`; display name не используется как runtime identity.

`GenomeFunctionInstance` ссылается на `FunctionTypeId` и хранит пары `ParameterId -> Amount` только для `genome_parameters`. Производные параметры отсутствуют в этой записи.

Некоторые типы функций компилируются в calculator process. Их world definition задаёт input/output `ValueKey` и указывает, какие compiled phenotype parameters становятся `throughput` и `result_per_input`. Phenotype-only функции могут не иметь calculator process.

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

`RuntimeWorld` компилирует phenotype при создании и строит calculator `Program` из process-enabled phenotype functions. Формулы genotype → phenotype не вычисляются на каждом tick. `RuntimeWorld::phenotype(ObjectId)` предоставляет const-доступ к скомпилированному phenotype объекта без раскрытия mutable `Calculator`.

## 5. First-world proof

Первый мир определяет `Energy Storage` как phenotype-only function type:

```text
Genome parameter:
    Capacity = 5

World-derived parameter:
    Organic size = Capacity / 5

Compiled phenotype:
    Capacity = 5
    Organic size = 1
```

После изменения только `Capacity` на `10` новая компиляция даёт `Organic size = 2`. Это пока не выделяет structural Organic и не меняет объём клетки автоматически: structural allocation является отдельной будущей механикой.

Существующие `Light Absorption` и `Energy Use` также мигрированы в function types. Их независимый throughput хранится в genome, а согласованный constant result factor принадлежит world definition. Численная pipeline первого мира остаётся прежней.

## 6. Godot editor

Godot facade возвращает structured function types и genome instances. Template inspector показывает редактируемые **Genome parameters** и read-only **Derived parameters**. Изменение независимого значения вызывает phenotype preview recompilation; derived field не является editable control.

Function types доступны отдельным разделом world tree. Все новые application captions проходят через существующие RU/EN PO resources. Function/parameter display names остаются world data и не переводятся автоматически.

## 7. Не входит в C7

C7 не реализует mutation, inheritance transport, reproduction, division, topology, structural Organic allocation, storage leakage, save/load или runtime formula evaluation. Structural operations по-прежнему зарезервированы для tick boundary.
