#pragma once

#include <clife/world/definition.hpp>

#include <string_view>

namespace clife::presets {

inline constexpr std::string_view kLightInputChannel{"world.light"};
inline constexpr std::string_view kUsedEnergyOutputChannel{"cell.used_energy"};
inline constexpr std::string_view kTemperatureOutputChannel{"cell.temperature"};
inline constexpr std::string_view kGeometryVolumeOutputChannel{"geometry.volume"};

inline constexpr std::string_view kValuesSummary{"Light\nEnergy\nUsedEnergy\nTemperature\nOrganic"};
inline constexpr std::string_view kGenomeSummary{
    "Light -> Energy (throughput 1.0, result 1.0)\n"
    "Energy -> UsedEnergy (throughput 0.25, result 1.0)"};
inline constexpr std::string_view kWorldRuleSummary{"remaining Energy -> Temperature * 0.1"};
inline constexpr std::string_view kBindingSummary{
    "world.light -> Light\n"
    "UsedEnergy -> cell.used_energy\n"
    "Temperature -> cell.temperature\n"
    "Organic -> geometry.volume"};

struct FirstWorldPreset final {
    world::WorldDefinition definition;
    world::ValueKey light;
    world::ValueKey energy;
    world::ValueKey used_energy;
    world::ValueKey temperature;
    world::ValueKey organic;
    world::TemplateId cell;
};

[[nodiscard]] FirstWorldPreset make_first_world_preset();

} // namespace clife::presets
