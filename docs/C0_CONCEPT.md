# C0 — концептуальная модель CLife

Статус: **CURRENT / NORMATIVE**.

CLife описывает универсальные `Object`, `ObjectTemplate`, `Genome` и `Phenotype`. «Клетка» — объект и термин первого клеточного мира, а не обязательный тип универсального core. Мир развивается от минимально необходимого: новые сущности и законы добавляются, только когда без них нельзя выразить нужное поведение.

## Базовые инварианты

- Adult object не растёт на обычном runtime tick. Genome задаёт готовую adult-конструкцию; embryo/bud/reproduction — отдельные будущие механики.
- Genome описывает способности объекта, а не imperative-программу.
- Конкуренцию за общий runtime ресурс определяет calculator, а не порядок объявления функций.
- Мир и host взаимодействуют через числа. Host интерпретирует собственные каналы, но не навязывает биологические понятия.
- Целевая физическая форма genome — каноническая encoded byte/hex последовательность. Текущий master использует semantic genotype как implementation scaffold; encoder, decoder и mutations ещё не реализованы.

## Текущая универсальная онтология

`Value` — изменяемое runtime-количество. `GenomeParameter` — независимый параметр функции. `CalculationDefinition` — world-authored математика. `ObjectCharacteristic` — статическое свойство скомпилированного phenotype. Мир задаёт Units, conversions и laws.

Термины `Field`, `Resource`, `State`, `Matter`, `Measure` и `Property` не являются обязательной универсальной C++-онтологией. Это может быть историческая терминология или понятия конкретного мира.

## Construction phenotype — реализовано сейчас

Готовый объект может иметь базовую конструкцию без функций. Function instance явно сообщает world-authored статические вклады. В частности, `MaterialContributionDefinition` связывает пользовательский material `ValueKey` с `FunctionValueSource`; это не встроенный закон и не означает, что каждый genome parameter автоматически является количеством материала.

```text
Function material contribution
    ↓
aggregated phenotype materials
    ↓
ObjectConstruction Calculation
    ↓
ObjectCharacteristics
```

`StructuralOrganic` в первом мире может быть обычной пользовательской World Quantity, а не builtin core-сущностью. Так же не встроены ни `Volume`, ни формула перехода от материала к объёму: их задаёт мир обычной `CalculationDefinition`. Сумма — лишь агрегация одинаковых вкладов, а не обязательный финальный физический закон.

Будущий embryo получает определённые миром requirements и дискретно формируется в adult phenotype. Это не фиксирует старый цикл по `MatterType`.

## Функции и эффективность

Текущая ordinary function имеет один input, throughput, выбранный UnitConversion и несколько outputs. Buffer function имеет capacity, throughput и leakage. Это реализованный уровень, но не обещание окончательного набора biological primitives.

Независимые genome parameters через `CalculationDefinition` могут задавать свойства конкретной функции. Формулы принадлежат миру, а не являются универсальными законами CLife. Целевой genome состоит из малых мутируемых primitives; текущий крупный `FunctionType` — полезный scaffold, но не гарантированный атом мутации.

## Несколько phenotype-проекций — принятая target-модель

Одна будущая physical genome sequence может иметь несколько независимых детерминированных проекций: functional, construction и future shape. Они читают один byte stream, поэтому одна mutation потенциально имеет сразу функциональное, конструктивное и морфологическое следствие. Подробная нормативная граница ShapePhenotype находится в [C8](C8_SHAPE_PHENOTYPE.md).
