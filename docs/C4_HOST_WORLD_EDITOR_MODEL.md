# C4 — World, Runtime и Host

Статус: **CURRENT / NORMATIVE**.

`WorldDefinition` содержит, среди прочего, `ValueDefinition`, `UnitDefinition`, `UnitConversionDefinition`, `CalculationDefinition`, `FunctionTypeDefinition`, `ObjectTemplate`, `ObjectCharacteristicDefinition`, `ObjectConstructionDefinition`, `WorldRuleDefinition`, `HostBinding` и `WorldDefinitionSnapshot`. Stable identities включают `UnitId`, `UnitConversionId` и `ObjectCharacteristicId`; имена — authoring data.

## Compilation boundary

`compile_phenotype(template)` разрешает semantic genome, function Calculations и `FunctionValueSource`. Результат задаёт process/buffer parameters, material contributions и function characteristic contributions. Материалы и одинаковые characteristic contributions агрегируются, после чего `ObjectConstructionDefinition` запускает обычную Calculation и публикует её outputs как final `ObjectCharacteristic`.

```text
semantic genome
  ↓
function Calculations / FunctionValueSources
  ↓
process parameters + material contributions + characteristic contributions
  ↓
aggregation
  ↓
ObjectConstruction
  ↓
CompiledPhenotype
```

`CompiledPhenotype` хранит function parameters, calculation outputs, material totals, function contribution sums и final ObjectCharacteristics. `Value` остаётся изменяемым runtime-количеством; `ObjectCharacteristic` — статическое свойство собранного phenotype.

`MaterialContributionDefinition` не менял свой смысл: это явная world-authored связь `material ValueKey <- FunctionValueSource`. Материал не является builtin и не выводится автоматически из genome parameter.

`ObjectConstructionSourceKind` поддерживает `base_characteristic`, `function_contribution_sum` и `material_amount`. `material_amount` хранит `ValueKey` и подаёт `CompiledPhenotype::material_amount(ValueKey)` во вход Calculation. Для двух первых видов источником остаётся `ObjectCharacteristicId`.

## Host inputs and bindings

Существуют два разных механизма ввода.

`HostBinding` остаётся текущим declared host-binding API. Его input имеет вид `HostChannel -> runtime Value`; output может иметь вид `runtime Value -> HostChannel` либо `ObjectCharacteristic -> HostChannel`. Input к ObjectCharacteristic запрещён.

Новый Godot editor не использует `ObjectTemplate::HostBinding` для environmental inputs. Его host-side configuration хранит mapping `channel -> ValueKey + test value` отдельно от WorldDefinition в `user://current_world.godot.json`. Для конкретного runtime object этот mapping превращается в `RuntimeWorld::set_external_input(ObjectId, ValueKey, Amount)`. Этот direct input не требует HostBinding.

Старый `RuntimeWorld::set_input(...)` сохраняет declared HostBinding semantics; он не является синонимом direct environmental input. Core не знает, что `world.light` означает свет.

## Stateful editor runtime preview

The current Godot Objects editor can hold one selected `ObjectTemplate` preview runtime across manual `Step` actions. `Start` creates the one-object runtime from the current definition, `Step` stages the current Godot external-input test values and executes exactly one tick, `Reset` reconstructs the same template from initial state, and `Stop` destroys the preview. The host reads runtime Values and compiled buffer state (`stored_amount`, `received_last_tick`, `supplied_last_tick`) through the existing engine-neutral `RuntimeWorld` projection. Any facade edit or template selection stops an active preview before changing its source definition.

## Persistence

WorldDefinition snapshot сохраняется в `user://current_world.clife.json`. Godot-specific external-input configuration сохраняется отдельно в `user://current_world.godot.json`. Snapshot backward compatibility между schema versions пока не гарантируется: host может принимать только актуальную schema, а старый test save допускается пересоздать.
