#pragma once

#include <clife/world/definition.hpp>

#include <string_view>

namespace clife::presets {

inline constexpr std::string_view kLightInputChannel{"world.light"};
inline constexpr std::string_view kUsedEnergyOutputChannel{"cell.used_energy"};
inline constexpr std::string_view kTemperatureOutputChannel{"cell.temperature"};
inline constexpr std::string_view kGeometryVolumeOutputChannel{"geometry.volume"};

inline constexpr std::string_view kValuesSummary{"Light\nEnergy\nUsedEnergy\nHeat\nTemperature\nOrganic"};
inline constexpr std::string_view kGenomeSummary{"Light Absorption: Throughput 1.0 -> Light to Energy\n"
                                                 "Energy Use: Throughput 0.5 -> Energy to UsedEnergy\n"
                                                 "Energy Storage: Capacity 5.0, Throughput 1.5, Stored 0"};
inline constexpr std::string_view kWorldRuleSummary{"remaining Energy -> END Heat -> Temperature * 0.1"};
inline constexpr std::string_view kBindingSummary{"world.light -> Light\n"
                                                  "UsedEnergy -> cell.used_energy\n"
                                                  "Temperature -> cell.temperature\n"
                                                  "Organic -> geometry.volume"};

struct FirstWorldPreset final {
    world::WorldDefinition definition;
    world::ValueKey light;
    world::ValueKey energy;
    world::ValueKey used_energy;
    world::ValueKey heat;
    world::ValueKey temperature;
    world::ValueKey organic;
    world::TemplateId cell;
    world::FunctionTypeId light_absorption;
    world::FunctionTypeId energy_use;
    world::FunctionTypeId energy_storage;
    world::ParameterId storage_capacity;
    world::CalculationId storage_calculation;
    world::CalculationPortId storage_organic_size;
    world::CalculationPortId storage_throughput;
    world::CalculationPortId storage_leakage;
};

[[nodiscard]] FirstWorldPreset make_first_world_preset();

} // namespace clife::presets
