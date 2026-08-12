# C8 — Shape phenotype

Статус: **CURRENT / NORMATIVE TARGET MODEL**.

Архитектурная граница принята; первый ограниченный engine-neutral implementation уже существует, а будущие replacement/extension algorithms остаются открытыми.

## One genome, deterministic shape

У объекта нет отдельного shape genome. Есть одна physical genome sequence, для которой canonical shape определяется детерминированно:

```text
Shape = F(WorldDefinition, PhysicalGenome)
```

При одинаковых `WorldDefinition` и `PhysicalGenome` должна получаться та же canonical shape.

Genome не содержит renderer-oriented representation: vertices, triangles, mesh commands, direct coordinates, renderer primitives sphere/box/capsule или position/orientation fields, добавленных только ради rendering. Это не запрещает будущим biological primitives иметь пространственный смысл; запрет касается embedding render geometry/mesh representation в genome.

## Independent projection

Одна physical genome имеет как минимум conceptual functional, construction и shape projections. Current master временно использует semantic genome scaffold до появления physical genome. Будущий ShapeDecoder не обязан интерпретировать semantic FunctionType так же, как functional decoder: он может детерминированно использовать encoded bytes и их order. Его API и algorithm пока открыты.

## Volume and shape are different

`Volume` — сколько physical space объект имеет право занимать; он получается из construction phenotype и world laws. `Shape` — как этот допустимый объём распределён в пространстве.

```text
normalized canonical shape + compiled Volume
    ↓
volume-constrained final geometry
```

Shape decoder не может создать дополнительную материю простым масштабированием geometry.

## Engine-neutral boundary

Rendering hosts, включая current Godot host, не являются source of truth canonical shape:

```text
PhysicalGenome
    ↓
engine-neutral ShapePhenotype
    ↓
host tessellation / rendering
```

Количество triangles — rendering property, а не biological property. Текущая engine-neutral data structure — continuous star-shaped radial phenotype; Godot chooses only its tessellation resolution.

## Current implemented radial ShapePhenotype

Current `ShapePhenotype` is a continuous star-shaped radial morphology around a canonical origin. For normalized direction `d = (x, y, z)`, it defines positive radius `r(d)` and surface point `d * r(d)`. It uses the fixed eight-term low-frequency engine-neutral basis `x`, `y`, `z`, `2xy`, `2yz`, `2zx`, `x²-y²`, `0.5(3z²-1)`. The bounded radial field is `exp(clamp(sum(ci * Bi), -0.9, 0.9))`; deterministic world-layer normalization makes canonical volume equal to one.

This first family supports smooth sphere-like forms, elongation, flattening, asymmetry, broad lobes and smooth irregular unicellular morphology. It intentionally does not support holes, disconnected components, overhangs invisible from the canonical origin, arbitrary topology, or non-star-shaped tentacles. These are current-model limitations, not a permanent universal restriction.

The current Godot Objects editor samples this `ShapePhenotype`, tessellates it, and uniformly corrects the discrete mesh to the explicitly selected final construction-characteristic volume. The resulting radial morphology is a visualization of the current `ShapePhenotype`; the selected characteristic-to-volume mapping remains editor-only debug configuration and does not define ShapePhenotype or future physical-genome decoding.

## Future extension remains open

Следующие семейства — только **NON-NORMATIVE examples**, ни одно не выбрано:

- smooth mathematical deformation;
- implicit surface / SDF;
- basis coefficients;
- spherical-harmonic-like families;
- continuous deterministic functions.

Не определены coefficients, opcodes, tessellation resolution или geometry API.

## Lifecycle transition: shape state remains open

Future world-defined Object Transition не обязан автоматически пересобирать geometry из target genome/template. Например, `cell -> inert/hot organic` может разумно сохранить уже сформированные размер и форму конкретного объекта, одновременно убрав его genomic mechanisms.

Следствие принято как **OPEN**, а не как API: после lifecycle transitions потребуется различать детерминированное происхождение `ShapePhenotype` из genome и текущее shape/geometry state конкретного object instance. Точная модель сохранения или reconstruction формы пока не выбрана и должна появиться только при конкретном simulation crisis.

## Mutation consequence

Одна mutation physical genome может одновременно изменить functional, material/construction и Shape phenotype. Это целевое emergent property одной sequence, а не отдельная «mutation of shape».
