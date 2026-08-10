# C4 — World, Runtime и Host

Статус: **CURRENT / NORMATIVE**.

`WorldDefinition` содержит `ValueDefinition`, `UnitDefinition`, `UnitConversionDefinition`, `CalculationDefinition`, `FunctionTypeDefinition`, `ObjectCharacteristicDefinition`, `ObjectConstructionDefinition`, `ObjectTemplate`, `WorldRuleDefinition`, `HostBinding` и `WorldDefinitionSnapshot`. Их identity обеспечивают stable IDs, включая `UnitId`, `UnitConversionId` и `ObjectCharacteristicId`; имена являются authoring data.

## Compilation boundary

`compile_phenotype(template)` разрешает genome parameters, выполняет привязанные Calculations, `FunctionValueSource`, material contributions и function characteristic contributions. После этого `ObjectConstructionDefinition` запускает одну обычную Calculation над базовыми характеристиками Template и суммами вкладов функций.

`CompiledPhenotype` хранит function parameters, calculation outputs, material totals, function contribution sums и final ObjectCharacteristics. `Value` остаётся изменяемым runtime количеством; `ObjectCharacteristic` — статическое свойство собранного phenotype.

## HostBinding

Input binding допускает только `HostChannel -> runtime Value`. Output binding допускает `runtime Value -> HostChannel` либо `ObjectCharacteristic -> HostChannel`. Например `world.light -> Свет` и `phenotype.Объём -> geometry.volume`. Host не знает специальных биологических имён.

Snapshot compatibility между schema versions пока не гарантируется: host может принимать только актуальную schema и старый test save может быть пересоздан.
