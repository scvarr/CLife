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

`world_definition_editor` already authors Units, World Quantities, Conversions, Formulas, Functions, Object Characteristics, Construction, Objects, World Rules, and External Inputs. Broader libraries and a future simulation/world screen are introduced only when genuinely needed.

## Product/project name

The current CLife name is historical and temporary. Near release, a separate product/project rename is needed because the system is evolving into a broader engine-neutral programmable-world/calculator framework. Nothing is renamed now.
## Structural operations

Division, topology changes и structural mutations at tick boundary требуют отдельной модели последствий и будущей задачи.

## Object lifecycle transitions

Принято направление, но не API: world-defined Object Transition сохраняет `ObjectId`, атомарно меняет текущую форму/type/template на границе tick после numeric world rules и начинает действовать со следующего tick. Condition должен использовать обычную world-authored Calculation; несколько одновременно подходящих transitions одного объекта — неоднозначность, а не declaration-order choice. Если transition удаляет buffer mechanism, его stored amount должен вернуться в связанный Value, а обычные world rules уже определят последствия.

OPEN: transition не обязан пересобирать geometry из target genome/template. Для состояния наподобие `cell -> inert/hot organic` может быть корректно оставить текущие размер и форму, убрав genomic mechanisms. Поэтому понадобится отдельная, ещё не выбранная модель различения genome-derived `ShapePhenotype` и текущего shape/geometry state object instance.

Это не задаёт non-genomic function system, pair-interaction API, environment simulation, division, destruction into multiple objects или material physics. Теплообмен, контактные процессы и иные world interactions должны рассматриваться только при отдельном simulation crisis.

Ненормативные примеры универсальности механизма: phase/state changes материалов, перегрев и отказ машин, горение и разрушение, игровые state transitions. Они не являются core concepts CLife.
