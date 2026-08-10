# CLife — журнал будущих идей

Этот файл **не является roadmap или спецификацией**. Записи не требуют реализации; идея переносится в normative documentation только после конкретной задачи и принятого решения.

## Physical genome encoding

Статус: идея.

Semantic genotype уже существует. Future string/byte encoding должен отдельно определить формат, decoding errors, determinism и mutation relation. `WorldDefinition` JSON save file не является biological genome encoding.

## Mutations and function composition

Статус: открытый вопрос.

Mutation может потребовать числовые и структурные операции, а сложные functions — композицию конечного alphabet primitives. Не принято, что FunctionType станет macro или что genome станет graph.

## Cell library and world instances

Статус: отложено.

Template — library definition, а не размещённый world object. Placement, transforms и multi-cell world ещё не спроектированы.

## Units, dimensions and world scales

Статус: частично реализовано; дальнейшее направление открыто.

World-authored Units, UnitExpressions и UnitConversions существуют. FunctionProcess уже использует UnitConversion при phenotype compilation. Compound-unit authoring, conversion paths, material properties и dimension checking expressions пока не реализованы.

## Structural materials and construction properties

Статус: открытый вопрос.

Structural organic, membrane и material properties могут стать inputs `ObjectConstruction` Calculations для объёма, массы, толщины, прочности, проницаемости и surface/volume relations. Конкретные свойства и формулы не приняты.

## Editor libraries

Статус: частично реализовано.

Function Library уже отдельный workspace. Calculation Library, Template Library и World Library остаются future workspaces по мере необходимости; основной экран остаётся simulation/world screen.

## Structural operations

Статус: отложено.

CreateNode, division, topology changes и structural mutations at tick boundary требуют отдельной задачи и модели последствий.
