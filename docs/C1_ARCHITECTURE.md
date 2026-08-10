# C1 — архитектура CLife

Статус: **CURRENT / NORMATIVE**.

```text
clife_core <- clife_world <- hosts
```

`clife_core` — универсальный числовой runtime/calculator. Он получает готовую программу с числовыми параметрами и не знает authoring expressions, units или phenotype.

`clife_world` — authoring model и compilation boundary: Values, Units/UnitConversions, Calculations, semantic genotype/FunctionTypes, ObjectTemplates, ObjectCharacteristics, ObjectConstruction, HostBindings, snapshots, phenotype и RuntimeWorld.

Godot — основной текущий editor host. Presets являются examples/tests и не являются обязательным источником editor world. Unreal — отдельный отложенный host.

```text
WorldDefinition
  -> semantic genotype
  -> function Calculations and FunctionValueSources
  -> function characteristic contributions
  -> object construction Calculation
  -> CompiledPhenotype
  -> RuntimeWorld / Calculator
```

Host сериализует `WorldDefinitionSnapshot`; world layer не зависит от JSON.
