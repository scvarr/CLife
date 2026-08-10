# C7 — genotype, phenotype и construction

Статус: **CURRENT / NORMATIVE**. Этот документ различает реализованный semantic слой master и принятую target-модель физического genome.

## Реализовано сейчас

`GenomeFunctionInstance`, `FunctionTypeId` и независимые `ParameterId -> Amount` образуют **semantic genotype** текущего master. Это рабочий implementation scaffold, не физическая byte/string последовательность.

`compile_phenotype(template)` детерминированно применяет world-defined `CalculationDefinition`, `FunctionValueSource`, process и contributions. Результат — статический adult phenotype; runtime calculator хранит отдельное состояние, меняющееся на tick. Обычный tick не перестраивает adult phenotype.

`CalculationDefinition` — единственное место пользовательской математики. Она принадлежит миру: FunctionType может привязать независимые genome parameters к её inputs, а outputs использовать как throughput, allocation, параметры buffer, material contribution или function characteristic contribution. `FunctionProcessDefinition` выбирает `UnitConversionId`; compilation превращает его target/source ratio и allocations в числовой `result_per_input` для calculator.

Function Calculation output (например пользовательский `Размер`, `КПД` или `Утечка`) — характеристика одной функции, а не `ObjectCharacteristic` объекта.

## Construction phenotype

Template задаёт base characteristics. Каждый экземпляр FunctionType может дать статический contribution; вклады одинаковой характеристики суммируются только как промежуточная агрегация.

`ObjectConstructionDefinition` связывает обычную `CalculationDefinition` с base characteristics и function contribution sums и публикует её outputs как final `ObjectCharacteristic`. Поэтому итоговая характеристика не является hardcoded SUM:

```text
base Volume = 5
function contribution = 2
Calculation: Volume = Base + Functions
final Volume = 7
```

Мир может включить packaging cost или иной закон, не меняя архитектуру. Host output может экспортировать runtime `Value` либо static `ObjectCharacteristic`; например `ObjectCharacteristic.Объём -> geometry.volume`.

## Принятая target-модель genome

Физический genome — каноническая свёрнутая hex/byte последовательность, например `A3 02 17 00 04 ...`. Это сам genome, а не JSON/debug serialization. Человеческие имена (`Свет`, `Энергия`, `Накопитель`, `ЭнергоСинтез`) в нём не хранятся: decoder/compiler интерпретирует encoded значения, opcodes, identifiers и параметры относительно `WorldDefinition`, а UI показывает декодированное semantic представление.

Целевой genome состоит из небольших мутируемых primitives. В будущем mutation сможет вставлять, удалять, дублировать primitive или менять его encoded parameter; сложная способность возникает из композиции primitives. Текущий крупный `FunctionType` — полезный scaffold, но не гарантированный окончательный атом мутации.

Точный binary/hex format, opcode table, размеры записей, набор primitives, encoder, decoder и mutation engine **не реализованы и не проектируются этим документом**.

## World laws и параметры

Genome содержит мутируемые encoded primitives и наследуемые параметры. World laws неизменяемы для данного мира и интерпретируют эти параметры; phenotype — результат такого применения. Например `throughput = 5` может мутировать, а `КПД = f(throughput)` и `Утечка = 1 - КПД` являются законами мира, выраженными через `CalculationDefinition`, а не содержимым genome.

Для первого мира допустим world law: основной функциональный параметр модуля одновременно задаёт его requirement structural organic — `throughput = 5 -> requirement = 5`, `capacity = 12 -> requirement = 12`. Изменение параметра тогда меняет способность, construction requirement и потенциально итоговый объём. Это не universal law `clife_core`.

`GenomeLength` — физическая длина канонического encoded genome. Она не равна physical structural requirement decoded механизмов. Будущий world law может использовать GenomeLength для maintenance, construction overhead или mutation mechanics; коэффициенты и формулы пока открыты.

## Объекты и adult/embryo

Архитектурный язык универсален: `Object`, `ObjectTemplate`, `Genome`, `Phenotype`. «Клетка» — объект первого world preset, а не обязательный universal core type.

Для первого мира целевая цепочка может быть `decoded genome -> functional parameters -> total StructuralOrganic requirement -> world Calculation -> ObjectCharacteristic Volume`; например `Volume = total StructuralOrganic` при unit «куб». Это пример world law, не hardcoded физика. В дальнейшем объём может участвовать в замедлении процессов, но его формула не определена.

Adult object не растёт на обычном runtime tick. Будущий embryo/bud получает требуемые миром материалы от parent; после выполнения requirements genome компилируется и embryo становится adult object. Embryo subsystem пока не реализован.

## Persistence

Сохраняется authoring `WorldDefinitionSnapshot`; expressions сохраняются как source и компилируются заново. Backward compatibility snapshot schema пока не гарантируется. RuntimeWorld и физический biological genome не сохраняются.
