# C1 — архитектура CLife

Статус: **CURRENT / NORMATIVE**.

```text
clife_core <- clife_world <- hosts
```

`clife_core` — универсальный числовой runtime/calculator. Он получает готовую программу с числовыми параметрами и не знает authoring expressions, units или phenotype. `clife_world` — authoring model и compilation boundary: Values, Units/UnitConversions, Calculations, semantic genotype/FunctionTypes, ObjectTemplates, ObjectCharacteristics, ObjectConstruction, HostBindings, snapshots, phenotype и RuntimeWorld. Godot — текущий основной editor host; Unreal — отдельный host.

Реализованный путь compilation:

```text
WorldDefinition
  ↓
semantic genome / GenomeFunctionInstance
  ↓
Function Calculations and FunctionValueSources
  ↓
function runtime parameters + material contributions
  ↓
aggregated material phenotype + function characteristic contribution sums
  ↓
ObjectConstruction Calculation
  ↓
CompiledPhenotype
  ↓
RuntimeWorld / Calculator
```

Host/environment path отделён от legacy HostBinding:

```text
Godot host source
  ↓
Godot-side mapping
  ↓
ValueKey
  ↓
RuntimeWorld::set_external_input(ObjectId, ValueKey, Amount)
```

Host serializes `WorldDefinitionSnapshot`; world layer не зависит от JSON.

Принятая target-архитектура physical genome допускает несколько независимых детерминированных phenotype-проекций:

```text
Physical Genome
  ├── functional phenotype projection
  ├── construction phenotype projection
  └── future shape phenotype projection
```

Godot и Unreal остаются consumers/adapters этих результатов, а не владельцами biological shape semantics.
