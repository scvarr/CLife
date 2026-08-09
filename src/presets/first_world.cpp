#include <clife/presets/first_world.hpp>

#include <utility>

namespace clife::presets {

FirstWorldPreset make_first_world_preset()
{
    world::WorldDefinition definition;
    const world::ValueKey light = definition.add_value("Light");
    const world::ValueKey energy = definition.add_value("Energy");
    const world::ValueKey used_energy = definition.add_value("UsedEnergy");
    const world::ValueKey heat = definition.add_value("Heat");
    const world::ValueKey temperature = definition.add_value("Temperature");
    const world::ValueKey organic = definition.add_value("Organic");
    const world::TemplateId cell = definition.add_template("Cell");

    const world::FunctionTypeId light_absorption = definition.add_function_type("Light Absorption");
    const world::ParameterId light_throughput = definition.add_genome_parameter(light_absorption, "Throughput", 1.0);
    const world::ParameterId light_result = definition.add_derived_parameter(light_absorption, "Result per input", "1");
    definition.set_function_process(light_absorption, {
                                                          .input = light,
                                                          .output = energy,
                                                          .throughput = light_throughput,
                                                          .result_per_input = light_result,
                                                      });
    definition.add_function_material_contribution(light_absorption, organic, "1");

    const world::FunctionTypeId energy_use = definition.add_function_type("Energy Use");
    const world::ParameterId use_throughput = definition.add_genome_parameter(energy_use, "Throughput", 0.5);
    const world::ParameterId use_result = definition.add_derived_parameter(energy_use, "Result per input", "1");
    definition.set_function_process(energy_use, {
                                                    .input = energy,
                                                    .output = used_energy,
                                                    .throughput = use_throughput,
                                                    .result_per_input = use_result,
                                                });
    definition.add_function_material_contribution(energy_use, organic, "1");

    const world::FunctionTypeId energy_storage = definition.add_function_type("Energy Storage");
    const world::ParameterId storage_capacity = definition.add_genome_parameter(energy_storage, "Capacity", 5.0);
    const world::ParameterId storage_organic_size =
        definition.add_derived_parameter(energy_storage, "OrganicSize", "Capacity / 5");
    const world::ParameterId storage_throughput =
        definition.add_derived_parameter(energy_storage, "Throughput", "Capacity * 0.3");
    const world::ParameterId storage_leakage = definition.add_derived_parameter(energy_storage, "Leakage", "0");
    definition.set_buffer_process(energy_storage, {
                                                      .value = energy,
                                                      .capacity = storage_capacity,
                                                      .throughput = storage_throughput,
                                                      .leakage = storage_leakage,
                                                  });
    definition.add_function_material_contribution(energy_storage, organic, "1 + OrganicSize");

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
        .storage_organic_size = storage_organic_size,
        .storage_throughput = storage_throughput,
        .storage_leakage = storage_leakage,
    };
}

} // namespace clife::presets
