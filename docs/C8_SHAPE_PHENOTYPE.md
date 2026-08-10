# C8 — Shape phenotype

Статус: **CURRENT / NORMATIVE TARGET MODEL**.

Архитектурная граница принята; implementation и algorithm ещё не выбраны.

## One genome, deterministic shape

У объекта нет отдельного shape genome. Есть одна physical genome sequence, для которой canonical shape определяется детерминированно:

```text
Shape = F(WorldDefinition, PhysicalGenome)
```

При одинаковых `WorldDefinition` и `PhysicalGenome` должна получаться та же canonical shape.

Genome не содержит renderer-oriented representation: vertices, triangles, mesh commands, direct coordinates, renderer primitives sphere/box/capsule или position/orientation fields, добавленных только ради rendering. Это не запрещает будущим biological primitives иметь пространственный смысл; запрет касается embedding render geometry/mesh representation в genome.

## Independent projection

Одна physical genome имеет как минимум conceptual functional, construction и future shape projections. ShapeDecoder не обязан интерпретировать semantic FunctionType так же, как functional decoder: он может детерминированно использовать encoded bytes и их order. Его API и algorithm пока открыты.

## Volume and shape are different

`Volume` — сколько physical space объект имеет право занимать; он получается из construction phenotype и world laws. `Shape` — как этот допустимый объём распределён в пространстве.

```text
normalized canonical shape + compiled Volume
    ↓
volume-constrained final geometry
```

Shape decoder не может создать дополнительную материю простым масштабированием geometry.

## Engine-neutral boundary

Godot и Unreal не являются source of truth canonical shape:

```text
PhysicalGenome
    ↓
engine-neutral ShapePhenotype
    ↓
host tessellation / rendering
```

Количество triangles — rendering property, а не biological property. Конкретная engine-neutral data structure не выбрана.

## Algorithm remains open

Следующие семейства — только **NON-NORMATIVE examples**, ни одно не выбрано:

- smooth mathematical deformation;
- implicit surface / SDF;
- basis coefficients;
- spherical-harmonic-like families;
- continuous deterministic functions.

Не определены coefficients, opcodes, tessellation resolution или geometry API.

## Mutation consequence

Одна mutation physical genome может одновременно изменить functional, material/construction и Shape phenotype. Это целевое emergent property одной sequence, а не отдельная «mutation of shape».
