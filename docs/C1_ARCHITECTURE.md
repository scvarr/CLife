# C1 — архитектура CLife

Статус: **CURRENT / NORMATIVE**.

```text
clife_core <- clife_world <- hosts
                         <- Godot (current graphical/editor host)
```

`clife_core` — универсальный числовой runtime/calculator. Он получает готовую программу с числовыми параметрами и не знает authoring expressions, units или phenotype. `clife_world` — authoring model и compilation boundary: Values, Units/UnitConversions, Calculations, semantic genotype/FunctionTypes, ObjectTemplates, ObjectCharacteristics, ObjectConstruction, HostBindings, snapshots, phenotype и RuntimeWorld. Godot — текущий реализованный graphical/editor host.

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

Независимый от construction/runtime текущий путь shape compilation:

```text
ObjectTemplate semantic genome
  ↓
temporary semantic shape projection
  ↓
ShapePhenotype
```

Этот временный semantic projection не является `compile_phenotype()` и не зависит от runtime calculator state. `ShapePhenotype` остаётся engine-neutral результатом world layer; host получает только samples для своей tessellation.

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

Rendering hosts, включая current Godot host, остаются consumers/adapters этих результатов, а не владельцами biological shape semantics.
