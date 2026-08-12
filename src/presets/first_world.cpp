#include <clife/presets/first_world.hpp>

#include <utility>

namespace clife::presets {
namespace {

world::FunctionValueSource genome(world::ParameterId parameter)
{
    return {.kind = world::FunctionValueSourceKind::genome_parameter, .genome_parameter = parameter};
}

world::FunctionValueSource calculated(world::CalculationId calculation, world::CalculationPortId output)
{
    return {.kind = world::FunctionValueSourceKind::calculation_output,
            .calculation = calculation,
            .calculation_output = output};
}

} // namespace

FirstWorldPreset make_first_world_preset()
{
    world::WorldDefinition definition;
    const world::ValueKey light = definition.add_value("Light");
    const world::ValueKey energy = definition.add_value("Energy");
    const world::ValueKey used_energy = definition.add_value("UsedEnergy");
    const world::ValueKey heat = definition.add_value("Heat");
    const world::ValueKey temperature = definition.add_value("Temperature");
    const world::ValueKey organic = definition.add_value("Organic");
    const world::UnitId legacy_unit = definition.add_unit("legacy");
    const world::UnitConversionId identity_conversion = definition.add_unit_conversion(
        {.components = {{.unit = legacy_unit, .exponent = 1}}}, 1.0,
        {.components = {{.unit = legacy_unit, .exponent = 1}}}, 1.0);
    const world::TemplateId cell = definition.add_template("Cell");

    const world::FunctionTypeId light_absorption = definition.add_function_type("Light Absorption");
    const world::ParameterId light_throughput = definition.add_genome_parameter(light_absorption, "Throughput", 1.0);
    const world::ParameterId light_result = definition.add_genome_parameter(light_absorption, "Allocation", 1.0);
    definition.set_function_process(light_absorption, {
                                                          .input = light,
                                                          .throughput = genome(light_throughput),
                                                          .conversion = identity_conversion,
                                                          .outputs = {{.output = energy, .allocation = genome(light_result)}},
                                                      });
    definition.set_function_material_contribution(light_absorption, organic, genome(light_result));

    const world::FunctionTypeId energy_use = definition.add_function_type("Energy Use");
    const world::ParameterId use_throughput = definition.add_genome_parameter(energy_use, "Throughput", 0.5);
    const world::ParameterId use_result = definition.add_genome_parameter(energy_use, "Allocation", 1.0);
    definition.set_function_process(energy_use, {
                                                    .input = energy,
                                                    .throughput = genome(use_throughput),
                                                    .conversion = identity_conversion,
                                                    .outputs = {{.output = used_energy, .allocation = genome(use_result)}},
                                                });
    definition.set_function_material_contribution(energy_use, organic, genome(use_result));

    const world::FunctionTypeId energy_storage = definition.add_function_type("Energy Storage");
    const world::ParameterId storage_capacity = definition.add_genome_parameter(energy_storage, "Capacity", 5.0);
    const world::CalculationId storage_calculation = definition.add_calculation("Energy Storage Parameters");
    const world::CalculationPortId storage_input = definition.add_calculation_input(storage_calculation, "Capacity");
    const world::CalculationPortId storage_organic_size =
        definition.add_calculation_output(storage_calculation, "OrganicSize", "Capacity / 5");
    const world::CalculationPortId storage_throughput =
        definition.add_calculation_output(storage_calculation, "Throughput", "Capacity * 0.3");
    const world::CalculationPortId storage_leakage =
        definition.add_calculation_output(storage_calculation, "Leakage", "0");
    const world::CalculationPortId storage_material =
        definition.add_calculation_output(storage_calculation, "Material", "1 + OrganicSize");
    definition.set_function_calculation_binding(energy_storage, {
        .calculation = storage_calculation,
        .inputs = {{.input = storage_input, .genome_parameter = storage_capacity}},
    });
    definition.set_buffer_process(energy_storage, {
                                                      .value = energy,
                                                      .capacity = genome(storage_capacity),
                                                      .throughput = calculated(storage_calculation, storage_throughput),
                                                      .leakage = calculated(storage_calculation, storage_leakage),
                                                  });
    definition.set_function_material_contribution(energy_storage, organic,
                                                  calculated(storage_calculation, storage_material));

    definition.set_template_material_contribution(cell, organic, 1.0);

    (void)definition.add_genome_function(cell, light_absorption);
    (void)definition.add_genome_function(cell, energy_use);
    (void)definition.add_genome_function(cell, energy_storage);
    (void)definition.add_world_rule({
        .source = energy,
        .end_buffer = heat,
        .target = temperature,
        .target_per_source = 0.1,
    });
    definition.set_initial_value(cell, temperature, 0.2);
    (void)definition.add_host_binding(cell, {
                                                .channel = std::string{kLightInputChannel},
                                                .direction = world::HostChannelDirection::input,
                                                .value = light,
                                            });
    (void)definition.add_host_binding(cell, {
                                                .channel = std::string{kUsedEnergyOutputChannel},
                                                .direction = world::HostChannelDirection::output,
                                                .value = used_energy,
                                            });
    (void)definition.add_host_binding(cell, {
                                                .channel = std::string{kTemperatureOutputChannel},
                                                .direction = world::HostChannelDirection::output,
                                                .value = temperature,
                                            });
    (void)definition.add_host_binding(cell, {
                                                .channel = std::string{kGeometryVolumeOutputChannel},
                                                .direction = world::HostChannelDirection::output,
                                                .value = organic,
                                            });

    return {
        .definition = std::move(definition),
        .light = light,
        .energy = energy,
        .used_energy = used_energy,
        .heat = heat,
        .temperature = temperature,
        .organic = organic,
        .cell = cell,
        .light_absorption = light_absorption,
        .energy_use = energy_use,
        .energy_storage = energy_storage,
        .storage_capacity = storage_capacity,
        .storage_calculation = storage_calculation,
        .storage_organic_size = storage_organic_size,
        .storage_throughput = storage_throughput,
        .storage_leakage = storage_leakage,
    };
}

} // namespace clife::presets
