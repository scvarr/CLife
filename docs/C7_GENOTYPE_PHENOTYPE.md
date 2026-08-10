# C7 — genotype, phenotype и construction

Статус: **CURRENT / NORMATIVE**.

Semantic genotype состоит из `GenomeFunctionInstance`, `FunctionTypeId` и независимых `ParameterId -> Amount`. Это не физическая byte/string последовательность: encoding, decoding и mutations пока не определены.

Phenotype детерминированно компилируется из Template и semantic genotype. Runtime — отдельное tick-изменяемое состояние calculator; adult phenotype остаётся статическим после compilation.

## Calculations и FunctionType

`CalculationDefinition` — единственное место пользовательской математики. FunctionType привязывает genome parameters к inputs Calculation; её outputs могут быть источниками throughput, allocations, buffer parameters, material contributions и function characteristic contributions.

Function Calculation output (например пользовательский `Размер`, `КПД` или `Утечка`) — результат конкретной функции. Он не равен `ObjectCharacteristic` всего объекта.

`FunctionProcessDefinition` выбирает `UnitConversionId`; phenotype compilation превращает target/source ratio и allocations в числовой `result_per_input`, который получает calculator.

## Object characteristics

Template задаёт base characteristics. Каждый экземпляр FunctionType может дать статический contribution в characteristic. Contributions одинаковой характеристики суммируются только как промежуточная агрегация.

`ObjectConstructionDefinition` связывает один ordinary `CalculationDefinition` с base characteristics и function contribution sums и публикует его outputs как final `ObjectCharacteristic`. Поэтому final characteristic не является hardcoded SUM.

```text
base Volume = 5
function contribution = 2
Calculation: Volume = Base + Functions
final Volume = 7
```

Мир мог бы вместо этого включить packaging cost без изменения архитектуры.

## Proof slice

`ЭнергоСинтез` может вычислить `Размер` из `Канал`; этот output вносится в `Объём`. Template даёт base `Объём = 5`, object construction вычисляет итог, а host output связывает `ObjectCharacteristic.Объём -> geometry.volume`. `Свет`, полезная энергия и утечка остаются runtime Values.

## Persistence

Сохраняется authoring `WorldDefinitionSnapshot`; expressions сохраняются как source и компилируются заново. Backward compatibility snapshot schema пока не гарантируется. RuntimeWorld и физический biological genome не сохраняются.
