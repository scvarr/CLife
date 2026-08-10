# CLife — журнал будущих идей

Этот файл **NON-NORMATIVE**: он не является roadmap или спецификацией. Идея попадает в normative documentation только после отдельного принятого решения.

## Physical genome implementation

Canonical physical byte/hex genome уже принят как target-модель; semantic genotype master — лишь scaffold. Открыты и не выбраны формат записей, opcode table, widths, encoding чисел, decoding errors, encoder/decoder и mutation implementation. `WorldDefinition` JSON snapshot не является biological genome.

## Mutations and primitive composition

Направление к малым мутируемым primitives и их композиции принято. Точный alphabet primitives, операторы, вероятности и последствия mutations остаются открытыми. Не принято, что текущий FunctionType обязательно станет macro или что genome обязательно станет graph.

## World instances

Template — library definition, а не размещённый world object. Placement, transforms, multi-object world и spatial simulation пока не спроектированы.

## Units and dimensions

World-authored Units, UnitExpressions и UnitConversions реализованы; FunctionProcess уже использует UnitConversion при phenotype compilation. Compound-unit authoring, conversion paths и полная dimensional checking system остаются будущей работой.

## Material physics and construction properties

Function material contributions, aggregate materials и `material_amount` source для ObjectConstruction уже реализованы. Future остаются material properties и physics: density, mass, membrane, thickness, strength, permeability, surface/volume relations и их world-authored formulas.

## Shape phenotype

Архитектурная граница ShapePhenotype принята, алгоритм остаётся открытым; см. [C8](C8_SHAPE_PHENOTYPE.md). Конкретные SDF, basis или tessellation formats не выбраны.

## Editor workspaces

Новый `world_definition_editor` уже author-ит Units, World Quantities, Conversions, Formulas, Functions, Object Characteristics, Construction, Objects и External Inputs. Старый Function Library — legacy/development stand, не основной UX. Отдельные более широкие libraries и будущий simulation/world screen вводятся только при реальной необходимости.

## Structural operations

Division, topology changes и structural mutations at tick boundary требуют отдельной модели последствий и будущей задачи.
