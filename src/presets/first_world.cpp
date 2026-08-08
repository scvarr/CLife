#include <clife/presets/first_world.hpp>

#include <utility>

namespace clife::presets {

FirstWorldPreset make_first_world_preset()
{
    world::WorldDefinition definition;
    const world::ValueKey light = definition.add_value("Light");
    const world::ValueKey energy = definition.add_value("Energy");
    const world::ValueKey used_energy = definition.add_value("UsedEnergy");
    const world::ValueKey temperature = definition.add_value("Temperature");
    const world::TemplateId cell = definition.add_template("Cell");

    (void)definition.add_genome_function(cell, {
                                                   .input = light,
                                                   .output = energy,
                                                   .throughput = 1.0,
                                                   .result_per_input = 1.0,
                                               });
    (void)definition.add_genome_function(cell, {
                                                   .input = energy,
                                                   .output = used_energy,
                                                   .throughput = 0.25,
                                                   .result_per_input = 1.0,
                                               });
    (void)definition.add_world_rule({
        .source = energy,
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

    return {
        .definition = std::move(definition),
        .light = light,
        .energy = energy,
        .used_energy = used_energy,
        .temperature = temperature,
        .cell = cell,
    };
}

} // namespace clife::presets
