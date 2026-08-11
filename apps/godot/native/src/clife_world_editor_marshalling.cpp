#include "clife_world_editor_internal.hpp"

namespace clife::godot_adapter::detail {
godot::String to_godot_string(std::string_view value)
{
    return godot::String::utf8(value.data(), static_cast<std::int64_t>(value.size()));
}

std::string to_std_string(const godot::String& value)
{
    const godot::CharString utf8 = value.utf8();
    return {utf8.get_data(), static_cast<std::size_t>(utf8.length())};
}

world::ValueKey value_key(std::int64_t raw)
{
    if (raw <= 0 || raw > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument{"invalid ValueKey"};
    }
    return {static_cast<std::uint32_t>(raw)};
}

world::UnitId unit_id(std::int64_t raw)
{
    if (raw <= 0 || raw > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument{"invalid UnitId"};
    }
    return {static_cast<std::uint32_t>(raw)};
}

world::UnitConversionId unit_conversion_id(std::int64_t raw)
{
    if (raw <= 0 || raw > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument{"invalid UnitConversionId"};
    }
    return {static_cast<std::uint32_t>(raw)};
}

world::ObjectCharacteristicId object_characteristic_id(std::int64_t raw)
{
    if (raw <= 0 || raw > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument{"invalid ObjectCharacteristicId"};
    }
    return {static_cast<std::uint32_t>(raw)};
}

world::TemplateId template_id(std::int64_t raw)
{
    if (raw <= 0 || raw > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument{"invalid TemplateId"};
    }
    return {static_cast<std::uint32_t>(raw)};
}

world::FunctionTypeId function_type_id(std::int64_t raw)
{
    if (raw <= 0 || raw > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument{"invalid FunctionTypeId"};
    }
    return {static_cast<std::uint32_t>(raw)};
}

world::ParameterId parameter_id(std::int64_t raw)
{
    if (raw <= 0 || raw > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument{"invalid ParameterId"};
    }
    return {static_cast<std::uint32_t>(raw)};
}

world::CalculationId calculation_id(std::int64_t raw)
{
    if (raw <= 0 || raw > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument{"invalid CalculationId"};
    }
    return {static_cast<std::uint32_t>(raw)};
}

world::CalculationPortId calculation_port_id(std::int64_t raw)
{
    if (raw <= 0 || raw > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument{"invalid CalculationPortId"};
    }
    return {static_cast<std::uint32_t>(raw)};
}

godot::Variant required_field(const godot::Dictionary& object, const char* name);
std::string required_string(const godot::Variant& value, const char* context);
std::uint32_t required_uint32(const godot::Variant& value, const char* context);

godot::Dictionary function_value_source_dictionary(const world::FunctionValueSource& source)
{
    godot::Dictionary result;
    result["kind"] = source.kind == world::FunctionValueSourceKind::genome_parameter ? "genome" : "calculation";
    result["genome_parameter_id"] = static_cast<std::int64_t>(source.genome_parameter.value);
    result["calculation_id"] = static_cast<std::int64_t>(source.calculation.value);
    result["calculation_output_id"] = static_cast<std::int64_t>(source.calculation_output.value);
    return result;
}

world::FunctionValueSource function_value_source(const godot::Dictionary& source)
{
    const std::string kind = required_string(required_field(source, "kind"), "function value source kind");
    if (kind == "genome") {
        return {.kind = world::FunctionValueSourceKind::genome_parameter,
                .genome_parameter = parameter_id(required_uint32(
                    required_field(source, "genome_parameter_id"), "genome_parameter_id"))};
    }
    if (kind == "calculation") {
        return {.kind = world::FunctionValueSourceKind::calculation_output,
                .calculation = calculation_id(required_uint32(required_field(source, "calculation_id"),
                                                              "calculation_id")),
                .calculation_output = calculation_port_id(required_uint32(
                    required_field(source, "calculation_output_id"), "calculation_output_id"))};
    }
    throw std::invalid_argument{"function value source kind must be genome or calculation"};
}

std::size_t item_index(std::int64_t raw)
{
    if (raw < 0) {
        throw std::out_of_range{"item index is out of range"};
    }
    return static_cast<std::size_t>(raw);
}

world::HostChannelDirection host_direction(std::int64_t raw)
{
    if (raw == kInputDirection) {
        return world::HostChannelDirection::input;
    }
    if (raw == kOutputDirection) {
        return world::HostChannelDirection::output;
    }
    throw std::invalid_argument{"host binding direction must be Input or Output"};
}

godot::String direction_name(world::HostChannelDirection direction)
{
    return direction == world::HostChannelDirection::input ? godot::String{"Input"} : godot::String{"Output"};
}

world::HostBinding host_binding(const godot::String& channel, std::int64_t direction, const godot::Dictionary& source)
{
    const std::string kind = required_string(required_field(source, "kind"), "host binding source kind");
    world::HostBinding result{.channel = to_std_string(channel), .direction = host_direction(direction)};
    if (kind == "value") {
        result.source_kind = world::HostBinding::SourceKind::value;
        result.value = value_key(required_uint32(required_field(source, "value_key"), "host binding value key"));
    } else if (kind == "characteristic") {
        result.source_kind = world::HostBinding::SourceKind::object_characteristic;
        result.characteristic = object_characteristic_id(required_uint32(required_field(source, "characteristic_id"), "host binding characteristic id"));
    } else { throw std::invalid_argument{"host binding source kind must be value or characteristic"}; }
    return result;
}

godot::Variant required_field(const godot::Dictionary& object, const char* name)
{
    if (!object.has(name)) {
        throw std::invalid_argument{std::string{"snapshot is missing required field: "} + name};
    }
    return object[name];
}

godot::Dictionary required_dictionary(const godot::Variant& value, const char* context)
{
    if (value.get_type() != godot::Variant::DICTIONARY) {
        throw std::invalid_argument{std::string{context} + " must be an object"};
    }
    return value;
}

godot::Array required_array(const godot::Variant& value, const char* context)
{
    if (value.get_type() != godot::Variant::ARRAY) {
        throw std::invalid_argument{std::string{context} + " must be an array"};
    }
    return value;
}

std::string required_string(const godot::Variant& value, const char* context)
{
    if (value.get_type() != godot::Variant::STRING) {
        throw std::invalid_argument{std::string{context} + " must be a string"};
    }
    return to_std_string(value);
}

double required_number(const godot::Variant& value, const char* context)
{
    if (value.get_type() != godot::Variant::INT && value.get_type() != godot::Variant::FLOAT) {
        throw std::invalid_argument{std::string{context} + " must be a number"};
    }
    const double result = value;
    if (!std::isfinite(result)) {
        throw std::invalid_argument{std::string{context} + " must be finite"};
    }
    return result;
}

std::uint32_t required_uint32(const godot::Variant& value, const char* context)
{
    if (value.get_type() == godot::Variant::INT) {
        const std::int64_t result = value;
        if (result < 0 || result > std::numeric_limits<std::uint32_t>::max()) {
            throw std::invalid_argument{std::string{context} + " is out of range"};
        }
        return static_cast<std::uint32_t>(result);
    }
    if (value.get_type() != godot::Variant::FLOAT) {
        throw std::invalid_argument{std::string{context} + " must be an integer"};
    }
    const double result = value;
    if (!std::isfinite(result) || result < 0.0 || result > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument{std::string{context} + " is out of range"};
    }
    if (std::floor(result) != result) {
        throw std::invalid_argument{std::string{context} + " must be an integer"};
    }
    return static_cast<std::uint32_t>(result);
}

std::int32_t required_int32(const godot::Variant& value, const char* context)
{
    if (value.get_type() == godot::Variant::INT) {
        const std::int64_t result = value;
        if (result < std::numeric_limits<std::int32_t>::min() || result > std::numeric_limits<std::int32_t>::max()) {
            throw std::invalid_argument{std::string{context} + " is out of range"};
        }
        return static_cast<std::int32_t>(result);
    }
    if (value.get_type() != godot::Variant::FLOAT) {
        throw std::invalid_argument{std::string{context} + " must be an integer"};
    }
    const double result = value;
    if (!std::isfinite(result) || result < std::numeric_limits<std::int32_t>::min() ||
        result > std::numeric_limits<std::int32_t>::max()) {
        throw std::invalid_argument{std::string{context} + " is out of range"};
    }
    if (std::floor(result) != result) {
        throw std::invalid_argument{std::string{context} + " must be an integer"};
    }
    return static_cast<std::int32_t>(result);
}

world::HostChannelDirection snapshot_direction(const godot::Variant& value)
{
    const std::string direction = required_string(value, "host binding direction");
    if (direction == "input") {
        return world::HostChannelDirection::input;
    }
    if (direction == "output") {
        return world::HostChannelDirection::output;
    }
    throw std::invalid_argument{"host binding direction must be input or output"};
}

godot::String snapshot_direction_name(world::HostChannelDirection direction)
{
    return direction == world::HostChannelDirection::input ? godot::String{"input"} : godot::String{"output"};
}

} // namespace clife::godot_adapter::detail

namespace clife::godot_adapter {

CLifeWorldEditor::CLifeWorldEditor() = default;

CLifeWorldEditor::~CLifeWorldEditor() = default;

} // namespace clife::godot_adapter
