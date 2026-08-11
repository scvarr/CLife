#pragma once

#include "clife_world_editor.hpp"

#include <clife/world/calculation.hpp>
#include <clife/world/shape.hpp>

#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace clife::godot_adapter::detail {

inline constexpr double kFixedTickSeconds = 0.1;
inline constexpr std::int64_t kInputDirection = 0;
inline constexpr std::int64_t kOutputDirection = 1;

struct HostCapability final {
    std::string_view channel;
    world::HostChannelDirection direction;
    std::string_view display_key;
};

inline constexpr std::array kHostCapabilities{
    HostCapability{.channel = "world.light", .direction = world::HostChannelDirection::input,
                   .display_key = "capability.world_light"},
    HostCapability{.channel = "geometry.volume", .direction = world::HostChannelDirection::output,
                   .display_key = "capability.geometry_volume"},
};

godot::String to_godot_string(std::string_view value);
std::string to_std_string(const godot::String& value);
world::ValueKey value_key(std::int64_t raw);
world::UnitId unit_id(std::int64_t raw);
world::UnitConversionId unit_conversion_id(std::int64_t raw);
world::ObjectCharacteristicId object_characteristic_id(std::int64_t raw);
world::TemplateId template_id(std::int64_t raw);
world::FunctionTypeId function_type_id(std::int64_t raw);
world::ParameterId parameter_id(std::int64_t raw);
world::CalculationId calculation_id(std::int64_t raw);
world::CalculationPortId calculation_port_id(std::int64_t raw);
godot::Dictionary function_value_source_dictionary(const world::FunctionValueSource& source);
world::FunctionValueSource function_value_source(const godot::Dictionary& source);
std::size_t item_index(std::int64_t raw);
world::HostChannelDirection host_direction(std::int64_t raw);
godot::String direction_name(world::HostChannelDirection direction);
world::HostBinding host_binding(const godot::String& channel, std::int64_t direction, const godot::Dictionary& source);
godot::Variant required_field(const godot::Dictionary& object, const char* name);
godot::Dictionary required_dictionary(const godot::Variant& value, const char* context);
godot::Array required_array(const godot::Variant& value, const char* context);
std::string required_string(const godot::Variant& value, const char* context);
double required_number(const godot::Variant& value, const char* context);
std::uint32_t required_uint32(const godot::Variant& value, const char* context);
std::int32_t required_int32(const godot::Variant& value, const char* context);
world::HostChannelDirection snapshot_direction(const godot::Variant& value);
godot::String snapshot_direction_name(world::HostChannelDirection direction);

} // namespace clife::godot_adapter::detail
