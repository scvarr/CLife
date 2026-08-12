# C7 — Genome, phenotype и construction

Статус: **CURRENT / NORMATIVE**. Этот документ различает реализованный semantic слой master, принятую target-модель physical genome и открытые детали.

## Реализовано сейчас: semantic genome

`ObjectTemplate` хранит упорядоченные `GenomeFunctionInstance`. Каждый instance содержит `FunctionTypeId` и упорядоченные semantic parameter values. Godot показывает это как semantic preview, например:

```text
02 | 5.0
02 | 2.0
```

Порядок entries сохраняется. Identifier в таком preview — текущий `FunctionTypeId`, показанный в hexadecimal для удобства; это не зафиксированный physical opcode и не physical HEX genome.

`compile_phenotype(template)` детерминированно применяет world-defined Calculations, FunctionValueSources, processes и contributions. Результат — статический adult phenotype; runtime calculator хранит отдельное состояние, которое меняется по tick. Обычный tick не перестраивает adult phenotype.

`CalculationDefinition` — world-authored математика. FunctionType может связать genome parameters с её inputs, а outputs использовать как throughput, allocation, buffer parameter, material contribution или function characteristic contribution. `FunctionProcessDefinition` выбирает `UnitConversionId`; compilation передаёт calculator уже числовые `result_per_input` и allocations.

Function Calculation output (например, пользовательский `КПД` или `Утечка`) — результат конкретной функции, а не ObjectCharacteristic всего объекта.

## Function definition не является genome entry

World-defined FunctionType может содержать runtime input, throughput source, conversion, Calculation binding, output allocations и material contribution. В целевом physical genome entry концептуально остаются только identity function/primitive и mutable inherited parameter values. Имена World Quantities, formula expression, conversions, bindings, allocations, material bindings и host bindings в genome не дублируются.

## Runtime projection — реализовано сейчас

Следующая вертикальная цепочка уже работает:

```text
Godot environmental value
  ↓
World Quantity / ValueKey
  ↓
RuntimeObject
  ↓
semantic genome
  ↓
FunctionTypeDefinition / FunctionProcess
  ↓
runtime outputs
```

Godot-side mapping `world.light -> Свет` хранится отдельно от WorldDefinition. Для конкретного object он подаётся через direct `RuntimeWorld::set_external_input(ObjectId, ValueKey, Amount)`, а не через ObjectTemplate HostBinding. Это не придаёт core специальную семантику света.

## Material and construction projection — реализовано сейчас

`MaterialDefinition` задаёт статический construction material с собственным `MaterialId`. `MaterialContributionDefinition` явно связывает этот `MaterialId` и `FunctionValueSource`, например:

```text
СтруктурнаяОрганика <- genome.Канал
```

Поэтому entry `02 | 5.0` может дать compiled material `СтруктурнаяОрганика = 5.0`; две entries `02 | 5.0` и `02 | 2.0` дают aggregate `СтруктурнаяОрганика = 7.0`. `StructuralOrganic` не hardcoded и не лежит в genome: это user-defined material. Он не является runtime `ValueKey`: material `Структурная органика` и изменяемый runtime Value `Organic` могут существовать одновременно и независимо. Связь material = parameter также не универсальна — её выбирает author функции.

`ObjectConstructionDefinition` запускает обычную Calculation над base characteristics, function-contribution sums и aggregate materials. Источник `ObjectConstructionSourceKind::material_amount` передаёт `CompiledPhenotype::material_amount(MaterialId)` во вход Calculation. Например, world law может получить material `СтруктурнаяОрганика`, вычислить `ОбъёмИзОрганики` и опубликовать final `ObjectCharacteristic Объём = 7`. Ни StructuralOrganic, ни Volume, ни эта формула не являются hardcoded. Compiled materials принадлежат статическому adult phenotype и не инициализируют `Calculator` runtime values.

Function characteristic contributions продолжают быть отдельным путём: их SUM — лишь промежуточная агрегация; final characteristic определяется ObjectConstruction Calculation, а не встроенным SUM.

## Принятая target-модель physical genome

## Current temporary semantic shape projection

Current master derives the first `ShapePhenotype` from the ordered semantic genome scaffold: stable `FunctionTypeId`, `ParameterId`, finite parameter values and entry order are mixed deterministically into a bounded low-frequency radial field. Authoring names and construction characteristics are not inputs. This temporary projection exists only until physical-genome decoding is implemented; it does not define a byte layout, folding rule, or mutation model for that future genome.

Physical genome — каноническая encoded byte/hex sequence, а не JSON/debug serialization. Human-readable names в нём не лежат. Decoder/compiler интерпретирует encoded identity и parameters относительно WorldDefinition.

Целевое направление — маленькие мутируемые primitives и их композиция. Текущий крупный FunctionType — implementation scaffold, не гарантированный окончательный атом мутации. Canonical physical genome уже принят как target, но точные byte layout, opcode width, record width, float representation, endian, primitive alphabet, encoder, decoder и mutation engine остаются **OPEN / NOT YET DESIGNED**.

`GenomeLength` означает физическую длину будущей canonical encoded sequence; оно не равно material requirement decoded mechanisms. Возможные laws для maintenance или construction overhead пока не определены.

## Несколько phenotype-проекций

Один physical genome является входом нескольких независимых детерминированных проекций:

```text
                 Physical Genome
                  /      |      \
                 ↓       ↓       ↓
          Functional  Construction  Shape
          projection  projection   projection
```

Они читают один byte stream и могут интерпретировать его по-разному. Поэтому одна mutation потенциально имеет функциональное, конструктивное и морфологическое следствие; порядок physical records также *может* влиять на будущую Shape projection. Это accepted architectural possibility, а не выбранный Shape algorithm. Нормативная граница shape находится в [C8](C8_SHAPE_PHENOTYPE.md).

## Objects and adult/embryo

Universal vocabulary: `Object`, `ObjectTemplate`, `Genome`, `Phenotype`. «Клетка» — пример первого world preset. Adult object не растёт на обычном tick. Будущий embryo/bud получает определённые миром материалы от parent, после выполнения requirements компилирует genome и дискретно становится adult object. Embryo subsystem пока не реализован.

## Persistence

`WorldDefinitionSnapshot` сохраняет authoring semantic genome и definitions; expressions хранятся source-текстом и компилируются заново. RuntimeWorld и physical biological genome пока не сохраняются. Backward compatibility snapshot schema не гарантируется до отдельного этапа стабилизации.
