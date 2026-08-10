#include "clife_world_editor.hpp"

#include <clife/world/calculation.hpp>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <array>
#include <cmath>
#include <exception>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace clife::godot_adapter {
namespace {

constexpr double kFixedTickSeconds = 0.1;
constexpr std::int64_t kInputDirection = 0;
constexpr std::int64_t kOutputDirection = 1;

struct HostCapability final {
    std::string_view channel;
    world::HostChannelDirection direction;
    std::string_view display_key;
};

constexpr std::array kHostCapabilities{
    HostCapability{
        .channel = "world.light",
        .direction = world::HostChannelDirection::input,
        .display_key = "capability.world_light",
    },
    HostCapability{
        .channel = "geometry.volume",
        .direction = world::HostChannelDirection::output,
        .display_key = "capability.geometry_volume",
    },
};

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

} // namespace

CLifeWorldEditor::CLifeWorldEditor() = default;

CLifeWorldEditor::~CLifeWorldEditor() = default;

godot::Array CLifeWorldEditor::get_values()
{
    godot::Array result;
    try {
        for (const world::ValueDefinition& value : definition_.values()) {
            godot::Dictionary item;
            item["key"] = static_cast<std::int64_t>(value.key.value);
            item["name"] = to_godot_string(value.name);
            godot::Array unit_components;
            if (value.unit) {
                for (const world::UnitComponent& component : value.unit->components) {
                    godot::Dictionary unit;
                    unit["id"] = static_cast<std::int64_t>(component.unit.value);
                    unit["exponent"] = component.exponent;
                    unit_components.push_back(unit);
                }
            }
            item["unit_components"] = unit_components;
            result.push_back(item);
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
    return result;
}

godot::Array CLifeWorldEditor::get_units()
{
    godot::Array result;
    try {
        for (const world::UnitDefinition& unit : definition_.units()) {
            godot::Dictionary item;
            item["id"] = static_cast<std::int64_t>(unit.id.value);
            item["symbol"] = to_godot_string(unit.symbol);
            item["description"] = to_godot_string(unit.description);
            result.push_back(item);
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
    return result;
}

godot::Array CLifeWorldEditor::get_unit_conversions()
{
    godot::Array result;
    try {
        for (const world::UnitConversionDefinition& conversion : definition_.unit_conversions()) {
            godot::Dictionary item;
            item["id"] = static_cast<std::int64_t>(conversion.id.value);
            item["source_amount"] = conversion.source_amount;
            item["target_amount"] = conversion.target_amount;
            godot::Array source_components;
            for (const world::UnitComponent& component : conversion.source_unit.components) {
                godot::Dictionary stored;
                stored["id"] = static_cast<std::int64_t>(component.unit.value);
                stored["exponent"] = component.exponent;
                source_components.push_back(stored);
            }
            item["source_components"] = source_components;
            godot::Array target_components;
            for (const world::UnitComponent& component : conversion.target_unit.components) {
                godot::Dictionary stored;
                stored["id"] = static_cast<std::int64_t>(component.unit.value);
                stored["exponent"] = component.exponent;
                target_components.push_back(stored);
            }
            item["target_components"] = target_components;
            result.push_back(item);
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
    return result;
}

godot::Array CLifeWorldEditor::get_templates()
{
    godot::Array result;
    try {
        for (const world::ObjectTemplate& object : definition_.templates()) {
            godot::Dictionary item;
            item["id"] = static_cast<std::int64_t>(object.id.value);
            item["name"] = to_godot_string(object.name);
            godot::Array bases;
            for (const auto& base : object.base_characteristics) {
                godot::Dictionary entry;
                entry["characteristic_id"] = static_cast<std::int64_t>(base.characteristic.value);
                entry["amount"] = base.amount;
                bases.push_back(entry);
            }
            item["base_characteristics"] = bases;
            result.push_back(item);
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
    return result;
}

godot::Array CLifeWorldEditor::get_object_characteristics()
{
    godot::Array result;
    try {
        for (const auto& characteristic : definition_.object_characteristics()) {
            godot::Dictionary item;
            item["id"] = static_cast<std::int64_t>(characteristic.id.value);
            item["name"] = to_godot_string(characteristic.name);
            result.push_back(item);
        }
        clear_error();
    } catch (...) { capture_current_error(); result.clear(); }
    return result;
}

godot::Dictionary CLifeWorldEditor::get_object_construction()
{
    godot::Dictionary result;
    try {
        if (!definition_.object_construction()) { clear_error(); return result; }
        const auto& construction = *definition_.object_construction();
        result["calculation_id"] = static_cast<std::int64_t>(construction.calculation.value);
        godot::Array inputs;
        for (const auto& binding : construction.inputs) {
            godot::Dictionary item;
            item["input_id"] = static_cast<std::int64_t>(binding.input.value);
            item["kind"] = binding.source.kind == world::ObjectConstructionSourceKind::base_characteristic ? "base" : "function_sum";
            item["characteristic_id"] = static_cast<std::int64_t>(binding.source.characteristic.value);
            inputs.push_back(item);
        }
        godot::Array outputs;
        for (const auto& binding : construction.outputs) {
            godot::Dictionary item;
            item["output_id"] = static_cast<std::int64_t>(binding.output.value);
            item["characteristic_id"] = static_cast<std::int64_t>(binding.characteristic.value);
            outputs.push_back(item);
        }
        result["inputs"] = inputs; result["outputs"] = outputs; clear_error();
    } catch (...) { capture_current_error(); result.clear(); }
    return result;
}

godot::Array CLifeWorldEditor::get_function_types()
{
    godot::Array result;
    try {
        for (const world::FunctionTypeDefinition& type : definition_.function_types()) {
            godot::Dictionary item;
            item["id"] = static_cast<std::int64_t>(type.id.value);
            item["name"] = to_godot_string(type.name);
            godot::Array genome_parameters;
            for (const world::GenomeParameterDefinition& parameter : type.genome_parameters) {
                godot::Dictionary entry;
                entry["id"] = static_cast<std::int64_t>(parameter.id.value);
                entry["name"] = to_godot_string(parameter.name);
                entry["default_value"] = parameter.default_value;
                genome_parameters.push_back(entry);
            }
            godot::Array calculation_bindings;
            for (const world::FunctionCalculationBinding& binding : type.calculations) {
                godot::Dictionary entry;
                entry["calculation_id"] = static_cast<std::int64_t>(binding.calculation.value);
                godot::Array inputs;
                for (const world::FunctionCalculationInputBinding& input : binding.inputs) {
                    godot::Dictionary stored;
                    stored["input_id"] = static_cast<std::int64_t>(input.input.value);
                    stored["genome_parameter_id"] = static_cast<std::int64_t>(input.genome_parameter.value);
                    inputs.push_back(stored);
                }
                entry["inputs"] = inputs;
                calculation_bindings.push_back(entry);
            }
            godot::Array material_contributions;
            for (const world::MaterialContributionDefinition& contribution : type.material_contributions) {
                godot::Dictionary entry;
                entry["value_key"] = static_cast<std::int64_t>(contribution.value.value);
                entry["amount_source"] = function_value_source_dictionary(contribution.amount);
                material_contributions.push_back(entry);
            }
            item["genome_parameters"] = genome_parameters;
            item["calculations"] = calculation_bindings;
            item["material_contributions"] = material_contributions;
            godot::Array characteristic_contributions;
            for (const auto& contribution : type.characteristic_contributions) {
                godot::Dictionary entry;
                entry["characteristic_id"] = static_cast<std::int64_t>(contribution.characteristic.value);
                entry["amount_source"] = function_value_source_dictionary(contribution.amount);
                characteristic_contributions.push_back(entry);
            }
            item["characteristic_contributions"] = characteristic_contributions;
            item["has_process"] = type.process.has_value();
            item["process"] = godot::Variant();
            if (type.process) {
                godot::Dictionary process;
                process["input_key"] = static_cast<std::int64_t>(type.process->input.value);
                process["throughput_source"] = function_value_source_dictionary(type.process->throughput);
                process["conversion_id"] = static_cast<std::int64_t>(type.process->conversion.value);
                godot::Array outputs;
                for (const world::FunctionProcessOutputDefinition& output : type.process->outputs) {
                    godot::Dictionary stored;
                    stored["output_key"] = static_cast<std::int64_t>(output.output.value);
                    stored["allocation_source"] = function_value_source_dictionary(output.allocation);
                    outputs.push_back(stored);
                }
                process["outputs"] = outputs;
                item["process"] = process;
            }
            item["has_buffer"] = type.buffer_process.has_value();
            if (type.buffer_process) {
                godot::Dictionary buffer;
                buffer["value_key"] = static_cast<std::int64_t>(type.buffer_process->value.value);
                buffer["capacity_source"] = function_value_source_dictionary(type.buffer_process->capacity);
                buffer["throughput_source"] = function_value_source_dictionary(type.buffer_process->throughput);
                buffer["leakage_source"] = function_value_source_dictionary(type.buffer_process->leakage);
                item["buffer"] = buffer;
            }
            result.push_back(item);
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
    return result;
}

godot::Array CLifeWorldEditor::get_calculations()
{
    godot::Array result;
    try {
        for (const world::CalculationDefinition& calculation : definition_.calculations()) {
            godot::Dictionary item;
            item["id"] = static_cast<std::int64_t>(calculation.id.value);
            item["name"] = to_godot_string(calculation.name);
            godot::Array inputs;
            for (const world::CalculationInputDefinition& input : calculation.inputs) {
                godot::Dictionary entry;
                entry["id"] = static_cast<std::int64_t>(input.id.value);
                entry["name"] = to_godot_string(input.name);
                inputs.push_back(entry);
            }
            godot::Array outputs;
            for (const world::CalculationOutputDefinition& output : calculation.outputs) {
                godot::Dictionary entry;
                entry["id"] = static_cast<std::int64_t>(output.id.value);
                entry["name"] = to_godot_string(output.name);
                entry["expression_source"] = to_godot_string(output.expression_source);
                outputs.push_back(entry);
            }
            item["inputs"] = inputs;
            item["outputs"] = outputs;
            result.push_back(item);
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
    return result;
}

godot::Array CLifeWorldEditor::get_initial_values(std::int64_t raw_template_id)
{
    godot::Array result;
    try {
        const world::ObjectTemplate& object = definition_.object_template(template_id(raw_template_id));
        for (const world::InitialValueDefinition& initial : object.initial_values) {
            godot::Dictionary item;
            item["value_key"] = static_cast<std::int64_t>(initial.value.value);
            item["amount"] = initial.amount;
            result.push_back(item);
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
    return result;
}

godot::Array CLifeWorldEditor::get_material_contributions(std::int64_t raw_template_id)
{
    godot::Array result;
    try {
        const world::TemplateId id = template_id(raw_template_id);
        const world::CompiledPhenotype phenotype = world::compile_phenotype(definition_, id);
        for (const world::MaterialAmount& material : phenotype.material_amounts()) {
            godot::Dictionary item;
            item["value_key"] = static_cast<std::int64_t>(material.value.value);
            item["amount"] = material.amount;
            result.push_back(item);
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
    return result;
}

godot::Array CLifeWorldEditor::get_genome(std::int64_t raw_template_id)
{
    godot::Array result;
    try {
        const world::TemplateId id = template_id(raw_template_id);
        const world::ObjectTemplate& object = definition_.object_template(id);
        for (std::size_t index = 0; index < object.genome.size(); ++index) {
            const world::GenomeFunctionInstance& function = object.genome[index];
            const world::FunctionTypeDefinition& type = definition_.function_type(function.type);
            godot::Dictionary item;
            item["index"] = static_cast<std::int64_t>(index);
            item["function_type_id"] = static_cast<std::int64_t>(type.id.value);
            item["function_type_name"] = to_godot_string(type.name);
            godot::Array genome_parameters;
            for (const world::GenomeParameterDefinition& parameter : type.genome_parameters) {
                const auto value = std::ranges::find(function.parameters, parameter.id, &world::ParameterValue::parameter);
                if (value == function.parameters.end()) {
                    throw std::invalid_argument{"genome function parameter identity is missing"};
                }
                godot::Dictionary entry;
                entry["parameter_id"] = static_cast<std::int64_t>(parameter.id.value);
                entry["name"] = to_godot_string(parameter.name);
                entry["amount"] = value->value;
                genome_parameters.push_back(entry);
            }
            item["genome_parameters"] = genome_parameters;
            item["calculation_outputs"] = godot::Array{};
            result.push_back(item);
        }

        try {
            const world::CompiledPhenotype phenotype = world::compile_phenotype(definition_, id);
            for (std::size_t index = 0; index < object.genome.size(); ++index) {
                const world::CompiledFunctionPhenotype& compiled = phenotype.function(index);
                godot::Array calculation_outputs;
                for (const world::CompiledCalculationOutputValue& output : compiled.calculation_outputs()) {
                    const world::CalculationDefinition& calculation = definition_.calculation(output.calculation);
                    const auto definition = std::ranges::find(calculation.outputs, output.output,
                                                              &world::CalculationOutputDefinition::id);
                    godot::Dictionary entry;
                    entry["calculation_id"] = static_cast<std::int64_t>(calculation.id.value);
                    entry["calculation_name"] = to_godot_string(calculation.name);
                    entry["output_id"] = static_cast<std::int64_t>(output.output.value);
                    entry["name"] = to_godot_string(definition->name);
                    entry["amount"] = output.value;
                    calculation_outputs.push_back(entry);
                }
                godot::Dictionary item = result[index];
                item["calculation_outputs"] = calculation_outputs;
                result[index] = item;
            }
        } catch (...) {
            // Raw authoring state remains visible while the template is incomplete.
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
    return result;
}

godot::Array CLifeWorldEditor::get_world_rules()
{
    godot::Array result;
    try {
        const auto& rules = definition_.world_rules();
        for (std::size_t index = 0; index < rules.size(); ++index) {
            const world::WorldRuleDefinition& rule = rules[index];
            godot::Dictionary item;
            item["index"] = static_cast<std::int64_t>(index);
            item["source_key"] = static_cast<std::int64_t>(rule.source.value);
            item["end_buffer_key"] = static_cast<std::int64_t>(rule.end_buffer.value);
            item["target_key"] = static_cast<std::int64_t>(rule.target.value);
            item["target_per_source"] = rule.target_per_source;
            result.push_back(item);
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
    return result;
}

godot::Array CLifeWorldEditor::get_bindings(std::int64_t raw_template_id)
{
    godot::Array result;
    try {
        const world::ObjectTemplate& object = definition_.object_template(template_id(raw_template_id));
        for (std::size_t index = 0; index < object.host_bindings.size(); ++index) {
            const world::HostBinding& binding = object.host_bindings[index];
            godot::Dictionary item;
            item["index"] = static_cast<std::int64_t>(index);
            item["channel"] = to_godot_string(binding.channel);
            item["direction"] = direction_name(binding.direction);
            item["direction_id"] =
                binding.direction == world::HostChannelDirection::input ? kInputDirection : kOutputDirection;
            item["value_key"] = static_cast<std::int64_t>(binding.value.value);
            item["characteristic_id"] = static_cast<std::int64_t>(binding.characteristic.value);
            item["source_kind"] = binding.source_kind == world::HostBinding::SourceKind::value ? "value" : "characteristic";
            result.push_back(item);
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
    return result;
}

godot::Dictionary CLifeWorldEditor::get_template_characteristic_preview(std::int64_t raw_template_id)
{
    godot::Dictionary result;
    try {
        const auto phenotype = world::compile_phenotype(definition_, template_id(raw_template_id));
        godot::Array sums; godot::Array characteristics;
        for (const auto& item : definition_.object_characteristics()) {
            godot::Dictionary sum; sum["characteristic_id"] = static_cast<std::int64_t>(item.id.value);
            sum["amount"] = phenotype.function_contribution_sum(item.id); sums.push_back(sum);
            godot::Dictionary final; final["characteristic_id"] = static_cast<std::int64_t>(item.id.value);
            final["amount"] = phenotype.characteristic(item.id); characteristics.push_back(final);
        }
        result["function_sums"] = sums; result["characteristics"] = characteristics; clear_error();
    } catch (...) { capture_current_error(); }
    return result;
}

godot::Array CLifeWorldEditor::get_host_capabilities()
{
    godot::Array result;
    try {
        for (const HostCapability& capability : kHostCapabilities) {
            godot::Dictionary item;
            item["channel"] = to_godot_string(capability.channel);
            item["direction"] = direction_name(capability.direction);
            item["direction_id"] =
                capability.direction == world::HostChannelDirection::input ? kInputDirection : kOutputDirection;
            item["display_key"] = to_godot_string(capability.display_key);
            result.push_back(item);
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
    return result;
}

std::int64_t CLifeWorldEditor::add_value(const godot::String& name)
{
    try {
        require_edit_mode();
        const world::ValueKey key = definition_.add_value(to_std_string(name));
        clear_error();
        return static_cast<std::int64_t>(key.value);
    } catch (...) {
        capture_current_error();
        return 0;
    }
}

std::int64_t CLifeWorldEditor::add_unit(const godot::String& symbol, const godot::String& description)
{
    try {
        require_edit_mode();
        const world::UnitId id = definition_.add_unit(to_std_string(symbol), to_std_string(description));
        clear_error();
        return static_cast<std::int64_t>(id.value);
    } catch (...) {
        capture_current_error();
        return 0;
    }
}

bool CLifeWorldEditor::update_unit(std::int64_t raw_unit_id, const godot::String& symbol, const godot::String& description)
{
    return edit([&] { definition_.update_unit(unit_id(raw_unit_id), to_std_string(symbol), to_std_string(description)); });
}

bool CLifeWorldEditor::remove_unit(std::int64_t raw_unit_id)
{
    return edit([&] { definition_.remove_unit(unit_id(raw_unit_id)); });
}

std::int64_t CLifeWorldEditor::add_unit_conversion(std::int64_t raw_source_unit_id, double source_amount,
                                                   std::int64_t raw_target_unit_id, double target_amount)
{
    try {
        require_edit_mode();
        const world::UnitConversionId id = definition_.add_unit_conversion(
            {.components = {{.unit = unit_id(raw_source_unit_id), .exponent = 1}}}, source_amount,
            {.components = {{.unit = unit_id(raw_target_unit_id), .exponent = 1}}}, target_amount);
        clear_error();
        return static_cast<std::int64_t>(id.value);
    } catch (...) {
        capture_current_error();
        return 0;
    }
}

bool CLifeWorldEditor::remove_unit_conversion(std::int64_t raw_conversion_id)
{
    return edit([&] { definition_.remove_unit_conversion(unit_conversion_id(raw_conversion_id)); });
}

bool CLifeWorldEditor::set_value_unit(std::int64_t raw_value_key, std::int64_t raw_unit_id)
{
    return edit([&] {
        definition_.set_value_unit(value_key(raw_value_key), {
                                                              .components = {{.unit = unit_id(raw_unit_id), .exponent = 1}},
                                                          });
    });
}

bool CLifeWorldEditor::clear_value_unit(std::int64_t raw_value_key)
{
    return edit([&] { definition_.clear_value_unit(value_key(raw_value_key)); });
}

std::int64_t CLifeWorldEditor::add_object_characteristic(const godot::String& name)
{
    try { require_edit_mode(); const auto id = definition_.add_object_characteristic(to_std_string(name)); clear_error(); return id.value; }
    catch (...) { capture_current_error(); return 0; }
}

bool CLifeWorldEditor::rename_object_characteristic(std::int64_t id, const godot::String& name)
{
    return edit([&] { definition_.rename_object_characteristic(object_characteristic_id(id), to_std_string(name)); });
}

bool CLifeWorldEditor::remove_object_characteristic(std::int64_t id)
{
    return edit([&] { definition_.remove_object_characteristic(object_characteristic_id(id)); });
}

bool CLifeWorldEditor::set_function_process(std::int64_t raw_type, std::int64_t raw_input,
                                            const godot::Dictionary& throughput_source, std::int64_t raw_conversion,
                                            std::int64_t raw_output, const godot::Dictionary& allocation_source)
{
    return edit([&] {
        definition_.set_function_process(function_type_id(raw_type), {
            .input = value_key(raw_input),
            .throughput = function_value_source(throughput_source),
            .conversion = unit_conversion_id(raw_conversion),
            .outputs = {{.output = value_key(raw_output), .allocation = function_value_source(allocation_source)}},
        });
    });
}

bool CLifeWorldEditor::set_function_process_full(std::int64_t raw_type, std::int64_t raw_input,
                                                 const godot::Dictionary& throughput_source,
                                                 std::int64_t raw_conversion, const godot::Array& outputs)
{
    return edit([&] {
        world::FunctionProcessDefinition process{
            .input = value_key(raw_input),
            .throughput = function_value_source(throughput_source),
            .conversion = unit_conversion_id(raw_conversion),
        };
        process.outputs.reserve(outputs.size());
        for (const godot::Variant& value : outputs) {
            const godot::Dictionary output = required_dictionary(value, "function process output");
            process.outputs.push_back({
                .output = value_key(required_uint32(required_field(output, "output_key"), "output_key")),
                .allocation = function_value_source(required_dictionary(
                    required_field(output, "allocation_source"), "allocation_source")),
            });
        }
        definition_.set_function_process(function_type_id(raw_type), std::move(process));
    });
}

bool CLifeWorldEditor::add_function_process_output(std::int64_t raw_type, std::int64_t raw_output,
                                                   const godot::Dictionary& allocation_source)
{
    return edit([&] { definition_.add_function_process_output(function_type_id(raw_type),
                                                                 {.output = value_key(raw_output),
                                                                  .allocation = function_value_source(allocation_source)}); });
}

bool CLifeWorldEditor::change_function_process_settings(std::int64_t raw_type, std::int64_t raw_input,
                                                        const godot::Dictionary& throughput_source,
                                                        std::int64_t raw_conversion)
{
    return edit([&] { definition_.change_function_process_settings(function_type_id(raw_type), value_key(raw_input),
                                                                     function_value_source(throughput_source),
                                                                     unit_conversion_id(raw_conversion)); });
}

bool CLifeWorldEditor::change_function_process_output(std::int64_t raw_type, std::int64_t raw_existing_output,
                                                      std::int64_t raw_output,
                                                      const godot::Dictionary& allocation_source)
{
    return edit([&] { definition_.change_function_process_output(
                          function_type_id(raw_type), value_key(raw_existing_output),
                          {.output = value_key(raw_output),
                           .allocation = function_value_source(allocation_source)}); });
}

bool CLifeWorldEditor::remove_function_process_output(std::int64_t raw_type, std::int64_t raw_output)
{
    return edit([&] { definition_.remove_function_process_output(function_type_id(raw_type), value_key(raw_output)); });
}

bool CLifeWorldEditor::remove_function_process(std::int64_t raw_type)
{
    return edit([&] { definition_.remove_function_process(function_type_id(raw_type)); });
}

bool CLifeWorldEditor::set_buffer_process(std::int64_t raw_type, std::int64_t raw_value,
                                          const godot::Dictionary& capacity_source,
                                          const godot::Dictionary& throughput_source,
                                          const godot::Dictionary& leakage_source)
{
    return edit([&] { definition_.set_buffer_process(function_type_id(raw_type), {
        .value = value_key(raw_value),
        .capacity = function_value_source(capacity_source),
        .throughput = function_value_source(throughput_source),
        .leakage = function_value_source(leakage_source),
    }); });
}

bool CLifeWorldEditor::remove_buffer_process(std::int64_t raw_type)
{
    return edit([&] { definition_.remove_buffer_process(function_type_id(raw_type)); });
}

bool CLifeWorldEditor::rename_value(std::int64_t key, const godot::String& name)
{
    return edit([&] { definition_.rename_value(value_key(key), to_std_string(name)); });
}

bool CLifeWorldEditor::remove_value(std::int64_t key)
{
    return edit([&] { definition_.remove_value(value_key(key)); });
}

std::int64_t CLifeWorldEditor::add_template(const godot::String& name)
{
    try {
        require_edit_mode();
        const world::TemplateId id = definition_.add_template(to_std_string(name));
        selected_template_ = id;
        clear_error();
        return static_cast<std::int64_t>(id.value);
    } catch (...) {
        capture_current_error();
        return 0;
    }
}

std::int64_t CLifeWorldEditor::add_function_type(const godot::String& name)
{
    try {
        require_edit_mode();
        const world::FunctionTypeId id = definition_.add_function_type(to_std_string(name));
        clear_error();
        return static_cast<std::int64_t>(id.value);
    } catch (...) {
        capture_current_error();
        return 0;
    }
}

bool CLifeWorldEditor::rename_function_type(std::int64_t raw_function_type_id, const godot::String& name)
{
    return edit([&] { definition_.rename_function_type(function_type_id(raw_function_type_id), to_std_string(name)); });
}

bool CLifeWorldEditor::remove_function_type(std::int64_t raw_function_type_id)
{
    return edit([&] { definition_.remove_function_type(function_type_id(raw_function_type_id)); });
}

std::int64_t CLifeWorldEditor::add_genome_parameter(std::int64_t raw_function_type_id, const godot::String& name,
                                                     double default_value)
{
    try {
        require_edit_mode();
        const world::ParameterId id = definition_.add_genome_parameter(function_type_id(raw_function_type_id),
                                                                         to_std_string(name), default_value);
        clear_error();
        return static_cast<std::int64_t>(id.value);
    } catch (...) {
        capture_current_error();
        return 0;
    }
}

bool CLifeWorldEditor::update_genome_parameter(std::int64_t raw_function_type_id, std::int64_t raw_parameter_id,
                                                const godot::String& name, double default_value)
{
    return edit([&] {
        definition_.update_genome_parameter(function_type_id(raw_function_type_id), parameter_id(raw_parameter_id),
                                             to_std_string(name), default_value);
    });
}

bool CLifeWorldEditor::remove_genome_parameter(std::int64_t raw_function_type_id, std::int64_t raw_parameter_id)
{
    return edit([&] {
        definition_.remove_genome_parameter(function_type_id(raw_function_type_id), parameter_id(raw_parameter_id));
    });
}

std::int64_t CLifeWorldEditor::add_calculation(const godot::String& name)
{
    try {
        require_edit_mode();
        const world::CalculationId id = definition_.add_calculation(to_std_string(name));
        clear_error();
        return static_cast<std::int64_t>(id.value);
    } catch (...) {
        capture_current_error();
        return 0;
    }
}

std::int64_t CLifeWorldEditor::add_calculation_input(std::int64_t raw_calculation_id, const godot::String& name)
{
    try {
        require_edit_mode();
        const std::int64_t id = raw_calculation_id;
        if (id <= 0 || id > std::numeric_limits<std::uint32_t>::max()) {
            throw std::invalid_argument{"invalid CalculationId"};
        }
        const world::CalculationPortId port =
            definition_.add_calculation_input({static_cast<std::uint32_t>(id)}, to_std_string(name));
        clear_error();
        return static_cast<std::int64_t>(port.value);
    } catch (...) {
        capture_current_error();
        return 0;
    }
}

std::int64_t CLifeWorldEditor::add_calculation_output(std::int64_t raw_calculation_id, const godot::String& name,
                                                       const godot::String& expression)
{
    try {
        require_edit_mode();
        if (raw_calculation_id <= 0 || raw_calculation_id > std::numeric_limits<std::uint32_t>::max()) {
            throw std::invalid_argument{"invalid CalculationId"};
        }
        const world::CalculationPortId port = definition_.add_calculation_output(
            {static_cast<std::uint32_t>(raw_calculation_id)}, to_std_string(name), to_std_string(expression));
        clear_error();
        return static_cast<std::int64_t>(port.value);
    } catch (...) {
        capture_current_error();
        return 0;
    }
}

bool CLifeWorldEditor::remove_calculation(std::int64_t raw_calculation_id)
{
    return edit([&] { definition_.remove_calculation(calculation_id(raw_calculation_id)); });
}

bool CLifeWorldEditor::remove_calculation_input(std::int64_t raw_calculation_id, std::int64_t raw_input_port_id)
{
    return edit([&] {
        definition_.remove_calculation_input(calculation_id(raw_calculation_id), calculation_port_id(raw_input_port_id));
    });
}

bool CLifeWorldEditor::remove_calculation_output(std::int64_t raw_calculation_id, std::int64_t raw_output_port_id)
{
    return edit([&] {
        definition_.remove_calculation_output(calculation_id(raw_calculation_id),
                                              calculation_port_id(raw_output_port_id));
    });
}

bool CLifeWorldEditor::set_calculation_output_expression(std::int64_t raw_calculation_id,
                                                          std::int64_t raw_output_port_id,
                                                          const godot::String& expression)
{
    return edit([&] {
        definition_.set_calculation_output_expression(calculation_id(raw_calculation_id),
                                                      calculation_port_id(raw_output_port_id),
                                                      to_std_string(expression));
    });
}

godot::Array CLifeWorldEditor::evaluate_calculation(std::int64_t raw_calculation_id, const godot::Array& inputs)
{
    godot::Array result;
    try {
        require_edit_mode();
        std::vector<world::CalculationPortAmount> amounts;
        amounts.reserve(inputs.size());
        for (const godot::Variant& input_value : inputs) {
            const godot::Dictionary input = required_dictionary(input_value, "calculation input");
            amounts.push_back({
                .port = calculation_port_id(required_uint32(required_field(input, "port_id"), "input port_id")),
                .amount = required_number(required_field(input, "amount"), "input amount"),
            });
        }
        for (const world::CalculationPortAmount& output :
             world::evaluate_calculation(definition_.calculation(calculation_id(raw_calculation_id)), amounts)) {
            godot::Dictionary item;
            item["port_id"] = static_cast<std::int64_t>(output.port.value);
            item["amount"] = output.amount;
            result.push_back(item);
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
    return result;
}

bool CLifeWorldEditor::rename_template(std::int64_t id, const godot::String& name)
{
    return edit([&] { definition_.rename_template(template_id(id), to_std_string(name)); });
}

bool CLifeWorldEditor::remove_template(std::int64_t id)
{
    return edit([&] {
        const world::TemplateId removed = template_id(id);
        definition_.remove_template(removed);
        if (selected_template_ == removed) {
            selected_template_.reset();
        }
    });
}

bool CLifeWorldEditor::set_initial_value(std::int64_t raw_template_id, std::int64_t raw_value_key, double amount)
{
    return edit([&] { definition_.set_initial_value(template_id(raw_template_id), value_key(raw_value_key), amount); });
}

bool CLifeWorldEditor::remove_initial_value(std::int64_t raw_template_id, std::int64_t raw_value_key)
{
    return edit([&] { definition_.remove_initial_value(template_id(raw_template_id), value_key(raw_value_key)); });
}

bool CLifeWorldEditor::set_template_base_characteristic(std::int64_t raw_template_id,
                                                         std::int64_t raw_characteristic_id, double amount)
{
    return edit([&] { definition_.set_template_base_characteristic(template_id(raw_template_id),
        object_characteristic_id(raw_characteristic_id), amount); });
}

bool CLifeWorldEditor::remove_template_base_characteristic(std::int64_t raw_template_id,
                                                            std::int64_t raw_characteristic_id)
{
    return edit([&] { definition_.remove_template_base_characteristic(template_id(raw_template_id),
        object_characteristic_id(raw_characteristic_id)); });
}

bool CLifeWorldEditor::set_function_calculation_binding(std::int64_t raw_function_type_id,
                                                        std::int64_t raw_calculation_id,
                                                        const godot::Array& input_bindings)
{
    return edit([&] {
        world::FunctionCalculationBinding binding{.calculation = calculation_id(raw_calculation_id)};
        for (std::int64_t index = 0; index < input_bindings.size(); ++index) {
            const godot::Dictionary input = required_dictionary(input_bindings[index], "calculation input binding");
            binding.inputs.push_back({
                .input = calculation_port_id(required_uint32(required_field(input, "input_id"), "input_id")),
                .genome_parameter = parameter_id(required_uint32(
                    required_field(input, "genome_parameter_id"), "genome_parameter_id")),
            });
        }
        definition_.set_function_calculation_binding(function_type_id(raw_function_type_id), std::move(binding));
    });
}

bool CLifeWorldEditor::remove_function_calculation_binding(std::int64_t raw_function_type_id,
                                                           std::int64_t raw_calculation_id)
{
    return edit([&] {
        definition_.remove_function_calculation_binding(function_type_id(raw_function_type_id),
                                                        calculation_id(raw_calculation_id));
    });
}

bool CLifeWorldEditor::set_function_material_contribution(std::int64_t raw_function_type_id,
                                                           std::int64_t raw_value_key,
                                                           const godot::Dictionary& amount_source)
{
    return edit([&] {
        definition_.set_function_material_contribution(function_type_id(raw_function_type_id), value_key(raw_value_key),
                                                       function_value_source(amount_source));
    });
}

bool CLifeWorldEditor::remove_function_material_contribution(std::int64_t raw_function_type_id,
                                                              std::int64_t raw_value_key)
{
    return edit([&] {
        definition_.remove_function_material_contribution(function_type_id(raw_function_type_id), value_key(raw_value_key));
    });
}

bool CLifeWorldEditor::set_function_characteristic_contribution(std::int64_t raw_function_type_id,
                                                                 std::int64_t raw_characteristic_id,
                                                                 const godot::Dictionary& amount_source)
{
    return edit([&] { definition_.set_function_characteristic_contribution(function_type_id(raw_function_type_id),
        object_characteristic_id(raw_characteristic_id), function_value_source(amount_source)); });
}

bool CLifeWorldEditor::remove_function_characteristic_contribution(std::int64_t raw_function_type_id,
                                                                    std::int64_t raw_characteristic_id)
{
    return edit([&] { definition_.remove_function_characteristic_contribution(function_type_id(raw_function_type_id),
        object_characteristic_id(raw_characteristic_id)); });
}

bool CLifeWorldEditor::add_genome_function(std::int64_t raw_template_id, std::int64_t raw_function_type_id)
{
    return edit([&] {
        (void)definition_.add_genome_function(template_id(raw_template_id), function_type_id(raw_function_type_id));
    });
}

bool CLifeWorldEditor::set_genome_parameter(std::int64_t raw_template_id, std::int64_t index,
                                            std::int64_t raw_parameter_id, double value)
{
    return edit([&] {
        definition_.set_genome_parameter(template_id(raw_template_id), item_index(index),
                                         parameter_id(raw_parameter_id), value);
    });
}

bool CLifeWorldEditor::remove_genome_function(std::int64_t raw_template_id, std::int64_t index)
{
    return edit([&] { definition_.remove_genome_function(template_id(raw_template_id), item_index(index)); });
}

bool CLifeWorldEditor::add_world_rule(std::int64_t source_key, std::int64_t end_buffer_key, std::int64_t target_key,
                                      double target_per_source)
{
    return edit([&] {
        (void)definition_.add_world_rule({
            .source = value_key(source_key),
            .end_buffer = value_key(end_buffer_key),
            .target = value_key(target_key),
            .target_per_source = target_per_source,
        });
    });
}

bool CLifeWorldEditor::change_world_rule(std::int64_t index, std::int64_t source_key,
                                         std::int64_t end_buffer_key, std::int64_t target_key,
                                         double target_per_source)
{
    return edit([&] {
        definition_.change_world_rule(item_index(index), {
                                                             .source = value_key(source_key),
                                                             .end_buffer = value_key(end_buffer_key),
                                                             .target = value_key(target_key),
                                                             .target_per_source = target_per_source,
                                                         });
    });
}

bool CLifeWorldEditor::remove_world_rule(std::int64_t index)
{
    return edit([&] { definition_.remove_world_rule(item_index(index)); });
}

bool CLifeWorldEditor::add_host_binding(std::int64_t raw_template_id, const godot::String& channel,
                                        std::int64_t direction, const godot::Dictionary& source)
{
    return edit([&] {
        const world::TemplateId id = template_id(raw_template_id);
        (void)definition_.add_host_binding(id, host_binding(channel, direction, source));
        ensure_host_inputs(definition_, id);
    });
}

bool CLifeWorldEditor::change_host_binding(std::int64_t raw_template_id, std::int64_t index,
                                           const godot::String& channel, std::int64_t direction,
                                           const godot::Dictionary& source)
{
    return edit([&] {
        const world::TemplateId id = template_id(raw_template_id);
        definition_.change_host_binding(id, item_index(index), host_binding(channel, direction, source));
        ensure_host_inputs(definition_, id);
    });
}

bool CLifeWorldEditor::remove_host_binding(std::int64_t raw_template_id, std::int64_t index)
{
    return edit([&] { definition_.remove_host_binding(template_id(raw_template_id), item_index(index)); });
}

bool CLifeWorldEditor::set_object_construction(std::int64_t raw_calculation_id, const godot::Array& inputs,
                                                const godot::Array& outputs)
{
    return edit([&] {
        world::ObjectConstructionDefinition construction{.calculation = calculation_id(raw_calculation_id)};
        for (const godot::Variant& value : inputs) {
            const auto item = required_dictionary(value, "construction input");
            const std::string kind = required_string(required_field(item, "kind"), "construction source kind");
            if (kind != "base" && kind != "function_sum") {
                throw std::invalid_argument{"construction source kind must be base or function_sum"};
            }
            construction.inputs.push_back({
                .input = calculation_port_id(required_uint32(required_field(item, "input_id"), "construction input id")),
                .source = {.kind = kind == "base" ? world::ObjectConstructionSourceKind::base_characteristic :
                                                     world::ObjectConstructionSourceKind::function_contribution_sum,
                           .characteristic = object_characteristic_id(required_uint32(
                               required_field(item, "characteristic_id"), "construction characteristic id"))},
            });
        }
        for (const godot::Variant& value : outputs) {
            const auto item = required_dictionary(value, "construction output");
            construction.outputs.push_back({
                .output = calculation_port_id(required_uint32(required_field(item, "output_id"), "construction output id")),
                .characteristic = object_characteristic_id(required_uint32(
                    required_field(item, "characteristic_id"), "construction characteristic id")),
            });
        }
        definition_.set_object_construction(std::move(construction));
    });
}

bool CLifeWorldEditor::remove_object_construction()
{
    return edit([&] { definition_.remove_object_construction(); });
}

bool CLifeWorldEditor::select_template(std::int64_t raw_template_id)
{
    return edit([&] {
        const world::TemplateId id = template_id(raw_template_id);
        (void)definition_.object_template(id);
        selected_template_ = id;
        ensure_host_inputs(definition_, id);
    });
}

std::int64_t CLifeWorldEditor::get_selected_template_id() const
{
    return selected_template_ ? static_cast<std::int64_t>(selected_template_->value) : 0;
}

bool CLifeWorldEditor::run()
{
    try {
        if (runtime_) {
            throw std::logic_error{"runtime is already active"};
        }
        if (!selected_template_) {
            throw std::invalid_argument{"select or create an object template before Run"};
        }
        run_definition_ = definition_;
        run_template_ = selected_template_;
        ensure_host_inputs(*run_definition_, *run_template_);
        rebuild_runtime_from_snapshot();
        playing_ = true;
        clear_error();
        return true;
    } catch (...) {
        runtime_.reset();
        preview_object_.reset();
        run_definition_.reset();
        run_template_.reset();
        playing_ = false;
        capture_current_error();
        return false;
    }
}

void CLifeWorldEditor::stop()
{
    runtime_.reset();
    preview_object_.reset();
    run_definition_.reset();
    run_template_.reset();
    accumulator_ = 0.0;
    tick_ = 0;
    playing_ = false;
    clear_error();
}

bool CLifeWorldEditor::reset_runtime()
{
    try {
        if (!runtime_ || !run_definition_ || !run_template_) {
            throw std::logic_error{"Reset requires an active runtime"};
        }
        rebuild_runtime_from_snapshot();
        playing_ = false;
        clear_error();
        return true;
    } catch (...) {
        capture_current_error();
        return false;
    }
}

bool CLifeWorldEditor::play()
{
    try {
        if (!runtime_) {
            throw std::logic_error{"Play requires an active runtime"};
        }
        playing_ = true;
        clear_error();
        return true;
    } catch (...) {
        capture_current_error();
        return false;
    }
}

void CLifeWorldEditor::pause() { playing_ = false; }

bool CLifeWorldEditor::step_once()
{
    try {
        if (!runtime_) {
            throw std::logic_error{"Step requires an active runtime"};
        }
        stage_inputs_and_step();
        clear_error();
        return true;
    } catch (...) {
        capture_current_error();
        return false;
    }
}

void CLifeWorldEditor::advance_time(double elapsed_seconds)
{
    try {
        if (!runtime_ || !playing_) {
            return;
        }
        if (!std::isfinite(elapsed_seconds) || elapsed_seconds < 0.0) {
            throw std::invalid_argument{"frame delta must be finite and non-negative"};
        }
        accumulator_ += elapsed_seconds;
        while (accumulator_ >= kFixedTickSeconds) {
            stage_inputs_and_step();
            accumulator_ -= kFixedTickSeconds;
        }
        clear_error();
    } catch (...) {
        playing_ = false;
        capture_current_error();
    }
}

bool CLifeWorldEditor::is_run_active() const noexcept { return runtime_ != nullptr; }
bool CLifeWorldEditor::is_playing() const noexcept { return playing_; }
std::int64_t CLifeWorldEditor::get_tick() const noexcept { return static_cast<std::int64_t>(tick_); }
double CLifeWorldEditor::get_fixed_tick_seconds() const noexcept { return kFixedTickSeconds; }
std::int64_t CLifeWorldEditor::get_preview_object_id() const noexcept
{
    return preview_object_ ? static_cast<std::int64_t>(preview_object_->value) : 0;
}

godot::Array CLifeWorldEditor::get_runtime_values()
{
    godot::Array result;
    try {
        if (!runtime_ || !preview_object_ || !run_definition_) {
            return result;
        }
        for (const world::ValueDefinition& value : run_definition_->values()) {
            godot::Dictionary item;
            item["key"] = static_cast<std::int64_t>(value.key.value);
            item["name"] = to_godot_string(value.name);
            item["amount"] = runtime_->value(*preview_object_, value.key);
            result.push_back(item);
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
    return result;
}

godot::Array CLifeWorldEditor::get_runtime_functions()
{
    godot::Array result;
    try {
        if (!runtime_ || !preview_object_ || !run_definition_) {
            return result;
        }
        const world::CompiledPhenotype& phenotype = runtime_->phenotype(*preview_object_);
        const std::vector<world::RuntimeFunctionState> states = runtime_->function_states(*preview_object_);
        for (const world::RuntimeFunctionState& state : states) {
            const world::FunctionTypeDefinition& type = run_definition_->function_type(state.type);
            const world::CompiledFunctionPhenotype& function = phenotype.function(state.function_index);
            godot::Dictionary item;
            item["object_id"] = static_cast<std::int64_t>(preview_object_->value);
            item["function_index"] = static_cast<std::int64_t>(state.function_index);
            item["function_type_id"] = static_cast<std::int64_t>(state.type.value);
            item["function_type_name"] = to_godot_string(type.name);
            godot::Array genome_parameters;
            for (const world::GenomeParameterDefinition& parameter : type.genome_parameters) {
                godot::Dictionary entry;
                entry["parameter_id"] = static_cast<std::int64_t>(parameter.id.value);
                entry["name"] = to_godot_string(parameter.name);
                entry["amount"] = function.parameter(parameter.id);
                genome_parameters.push_back(entry);
            }
            godot::Array calculation_outputs;
            for (const world::CompiledCalculationOutputValue& output : function.calculation_outputs()) {
                const world::CalculationDefinition& calculation = run_definition_->calculation(output.calculation);
                const auto definition = std::ranges::find(calculation.outputs, output.output,
                                                          &world::CalculationOutputDefinition::id);
                godot::Dictionary entry;
                entry["calculation_id"] = static_cast<std::int64_t>(calculation.id.value);
                entry["calculation_name"] = to_godot_string(calculation.name);
                entry["output_id"] = static_cast<std::int64_t>(output.output.value);
                entry["name"] = to_godot_string(definition->name);
                entry["amount"] = output.value;
                calculation_outputs.push_back(entry);
            }
            item["genome_parameters"] = genome_parameters;
            item["calculation_outputs"] = calculation_outputs;
            if (state.buffer) {
                const world::CompiledBufferParameters& parameters = *function.buffer_parameters();
                godot::Dictionary buffer;
                buffer["capacity"] = parameters.capacity;
                buffer["throughput"] = parameters.throughput;
                buffer["leakage"] = parameters.leakage;
                buffer["stored_amount"] = state.buffer->stored_amount;
                buffer["received_last_tick"] = state.buffer->received_last_tick;
                buffer["supplied_last_tick"] = state.buffer->supplied_last_tick;
                item["buffer"] = buffer;
            }
            result.push_back(item);
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
    return result;
}

godot::Array CLifeWorldEditor::get_last_end_buffer()
{
    godot::Array result;
    try {
        if (!runtime_ || !preview_object_ || !run_definition_) {
            return result;
        }
        std::set<world::ValueKey> included;
        for (const world::WorldRuleDefinition& rule : run_definition_->world_rules()) {
            if (!included.insert(rule.end_buffer).second) {
                continue;
            }
            const world::ValueDefinition& value = run_definition_->value(rule.end_buffer);
            godot::Dictionary item;
            item["value_key"] = static_cast<std::int64_t>(value.key.value);
            item["name"] = to_godot_string(value.name);
            item["amount"] = runtime_->last_end_value(*preview_object_, value.key);
            result.push_back(item);
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
    return result;
}

godot::Array CLifeWorldEditor::get_host_inputs()
{
    godot::Array result;
    try {
        const world::WorldDefinition& source = run_definition_ ? *run_definition_ : definition_;
        const std::optional<world::TemplateId> source_template = run_template_ ? run_template_ : selected_template_;
        if (!source_template) {
            return result;
        }
        ensure_host_inputs(source, *source_template);
        const world::ObjectTemplate& object = source.object_template(*source_template);
        for (const world::HostBinding& binding : object.host_bindings) {
            if (binding.direction != world::HostChannelDirection::input) {
                continue;
            }
            godot::Dictionary item;
            item["channel"] = to_godot_string(binding.channel);
            item["value_key"] = static_cast<std::int64_t>(binding.value.value);
            item["amount"] = host_inputs_.at(binding.channel);
            result.push_back(item);
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
    return result;
}

godot::Array CLifeWorldEditor::get_host_outputs()
{
    godot::Array result;
    try {
        if (!runtime_ || !preview_object_ || !run_definition_ || !run_template_) {
            return result;
        }
        const world::ObjectTemplate& object = run_definition_->object_template(*run_template_);
        for (const world::HostBinding& binding : object.host_bindings) {
            if (binding.direction != world::HostChannelDirection::output) {
                continue;
            }
            godot::Dictionary item;
            item["object_id"] = static_cast<std::int64_t>(preview_object_->value);
            item["channel"] = to_godot_string(binding.channel);
            item["value_key"] = static_cast<std::int64_t>(binding.value.value);
            item["amount"] = runtime_->value(*preview_object_, binding.value);
            result.push_back(item);
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
    return result;
}

bool CLifeWorldEditor::set_host_input(const godot::String& channel, double amount)
{
    try {
        if (!std::isfinite(amount)) {
            throw std::invalid_argument{"host input must be finite"};
        }
        const std::string requested = to_std_string(channel);
        const world::WorldDefinition& source = run_definition_ ? *run_definition_ : definition_;
        const std::optional<world::TemplateId> source_template = run_template_ ? run_template_ : selected_template_;
        if (!source_template) {
            throw std::invalid_argument{"no template is selected"};
        }
        const world::ObjectTemplate& object = source.object_template(*source_template);
        bool found = false;
        for (const world::HostBinding& binding : object.host_bindings) {
            if (binding.direction == world::HostChannelDirection::input && binding.channel == requested) {
                found = true;
                break;
            }
        }
        if (!found) {
            throw std::invalid_argument{"unknown input host channel"};
        }
        host_inputs_[requested] = amount;
        clear_error();
        return true;
    } catch (...) {
        capture_current_error();
        return false;
    }
}

godot::Dictionary CLifeWorldEditor::export_world_snapshot()
{
    godot::Dictionary result;
    try {
        const world::WorldDefinitionSnapshot snapshot = definition_.snapshot();
        result["schema_version"] = static_cast<std::int64_t>(snapshot.schema_version);
        result["next_value_key"] = static_cast<std::int64_t>(snapshot.next_value_key);
        result["next_template_id"] = static_cast<std::int64_t>(snapshot.next_template_id);
        result["next_function_type_id"] = static_cast<std::int64_t>(snapshot.next_function_type_id);
        result["next_parameter_id"] = static_cast<std::int64_t>(snapshot.next_parameter_id);
        result["next_calculation_id"] = static_cast<std::int64_t>(snapshot.next_calculation_id);
        result["next_calculation_port_id"] = static_cast<std::int64_t>(snapshot.next_calculation_port_id);
        result["next_unit_id"] = static_cast<std::int64_t>(snapshot.next_unit_id);
        result["next_unit_conversion_id"] = static_cast<std::int64_t>(snapshot.next_unit_conversion_id);
        result["next_object_characteristic_id"] = static_cast<std::int64_t>(snapshot.next_object_characteristic_id);
        godot::Array values;
        for (const auto& value : snapshot.values) {
            godot::Dictionary entry;
            entry["id"] = static_cast<std::int64_t>(value.key.value);
            entry["name"] = to_godot_string(value.name);
            entry["unit"] = godot::Variant();
            if (value.unit) {
                godot::Array components;
                for (const world::UnitComponent& component : value.unit->components) {
                    godot::Dictionary stored;
                    stored["unit_id"] = static_cast<std::int64_t>(component.unit.value);
                    stored["exponent"] = component.exponent;
                    components.push_back(stored);
                }
                entry["unit"] = components;
            }
            values.push_back(entry);
        }
        result["values"] = values;
        godot::Array units;
        for (const world::UnitDefinition& unit : snapshot.units) {
            godot::Dictionary entry;
            entry["id"] = static_cast<std::int64_t>(unit.id.value);
            entry["symbol"] = to_godot_string(unit.symbol);
            entry["description"] = to_godot_string(unit.description);
            units.push_back(entry);
        }
        result["units"] = units;
        godot::Array characteristics;
        for (const auto& characteristic : snapshot.object_characteristics) {
            godot::Dictionary entry;
            entry["id"] = static_cast<std::int64_t>(characteristic.id.value);
            entry["name"] = to_godot_string(characteristic.name);
            characteristics.push_back(entry);
        }
        result["object_characteristics"] = characteristics;
        godot::Array unit_conversions;
        for (const world::UnitConversionDefinition& conversion : snapshot.unit_conversions) {
            godot::Dictionary entry;
            entry["id"] = static_cast<std::int64_t>(conversion.id.value);
            entry["source_amount"] = conversion.source_amount;
            entry["target_amount"] = conversion.target_amount;
            godot::Array source_unit;
            for (const world::UnitComponent& component : conversion.source_unit.components) {
                godot::Dictionary stored;
                stored["unit_id"] = static_cast<std::int64_t>(component.unit.value);
                stored["exponent"] = component.exponent;
                source_unit.push_back(stored);
            }
            entry["source_unit"] = source_unit;
            godot::Array target_unit;
            for (const world::UnitComponent& component : conversion.target_unit.components) {
                godot::Dictionary stored;
                stored["unit_id"] = static_cast<std::int64_t>(component.unit.value);
                stored["exponent"] = component.exponent;
                target_unit.push_back(stored);
            }
            entry["target_unit"] = target_unit;
            unit_conversions.push_back(entry);
        }
        result["unit_conversions"] = unit_conversions;
        godot::Array calculations;
        for (const auto& calculation : snapshot.calculations) {
            godot::Dictionary entry;
            entry["id"] = static_cast<std::int64_t>(calculation.id.value);
            entry["name"] = to_godot_string(calculation.name);
            godot::Array inputs;
            for (const auto& input : calculation.inputs) {
                godot::Dictionary port;
                port["id"] = static_cast<std::int64_t>(input.id.value);
                port["name"] = to_godot_string(input.name);
                inputs.push_back(port);
            }
            godot::Array outputs;
            for (const auto& output : calculation.outputs) {
                godot::Dictionary port;
                port["id"] = static_cast<std::int64_t>(output.id.value);
                port["name"] = to_godot_string(output.name);
                port["expression_source"] = to_godot_string(output.expression_source);
                outputs.push_back(port);
            }
            entry["inputs"] = inputs;
            entry["outputs"] = outputs;
            calculations.push_back(entry);
        }
        result["calculations"] = calculations;
        godot::Array types;
        for (const auto& type : snapshot.function_types) {
            godot::Dictionary entry;
            entry["id"] = static_cast<std::int64_t>(type.id.value);
            entry["name"] = to_godot_string(type.name);
            godot::Array genome_parameters;
            for (const auto& parameter : type.genome_parameters) {
                godot::Dictionary item;
                item["id"] = static_cast<std::int64_t>(parameter.id.value);
                item["name"] = to_godot_string(parameter.name);
                item["default_value"] = parameter.default_value;
                genome_parameters.push_back(item);
            }
            godot::Array calculation_bindings;
            for (const auto& binding : type.calculations) {
                godot::Dictionary item;
                item["calculation_id"] = static_cast<std::int64_t>(binding.calculation.value);
                godot::Array inputs;
                for (const auto& input : binding.inputs) {
                    godot::Dictionary stored;
                    stored["input_id"] = static_cast<std::int64_t>(input.input.value);
                    stored["genome_parameter_id"] = static_cast<std::int64_t>(input.genome_parameter.value);
                    inputs.push_back(stored);
                }
                item["inputs"] = inputs;
                calculation_bindings.push_back(item);
            }
            godot::Array contributions;
            for (const auto& contribution : type.material_contributions) {
                godot::Dictionary item;
                item["value_key"] = static_cast<std::int64_t>(contribution.value.value);
                item["amount_source"] = function_value_source_dictionary(contribution.amount);
                contributions.push_back(item);
            }
            entry["genome_parameters"] = genome_parameters;
            entry["calculations"] = calculation_bindings;
            entry["material_contributions"] = contributions;
            godot::Array characteristic_contributions;
            for (const auto& contribution : type.characteristic_contributions) {
                godot::Dictionary item;
                item["characteristic_id"] = static_cast<std::int64_t>(contribution.characteristic.value);
                item["amount_source"] = function_value_source_dictionary(contribution.amount);
                characteristic_contributions.push_back(item);
            }
            entry["characteristic_contributions"] = characteristic_contributions;
            entry["process"] = godot::Variant();
            if (type.process) {
                godot::Dictionary process;
                process["input_key"] = static_cast<std::int64_t>(type.process->input.value);
                process["throughput_source"] = function_value_source_dictionary(type.process->throughput);
                process["conversion_id"] = static_cast<std::int64_t>(type.process->conversion.value);
                godot::Array outputs;
                for (const world::FunctionProcessOutputDefinition& output : type.process->outputs) {
                    godot::Dictionary stored;
                    stored["output_key"] = static_cast<std::int64_t>(output.output.value);
                    stored["allocation_source"] = function_value_source_dictionary(output.allocation);
                    outputs.push_back(stored);
                }
                process["outputs"] = outputs;
                entry["process"] = process;
            }
            entry["buffer_process"] = godot::Variant();
            if (type.buffer_process) {
                godot::Dictionary buffer;
                buffer["value_key"] = static_cast<std::int64_t>(type.buffer_process->value.value);
                buffer["capacity_source"] = function_value_source_dictionary(type.buffer_process->capacity);
                buffer["throughput_source"] = function_value_source_dictionary(type.buffer_process->throughput);
                buffer["leakage_source"] = function_value_source_dictionary(type.buffer_process->leakage);
                entry["buffer_process"] = buffer;
            }
            types.push_back(entry);
        }
        result["function_types"] = types;
        godot::Array templates;
        for (const auto& object : snapshot.templates) {
            godot::Dictionary entry;
            entry["id"] = static_cast<std::int64_t>(object.id.value);
            entry["name"] = to_godot_string(object.name);
            godot::Array initials;
            for (const auto& item : object.initial_values) {
                godot::Dictionary stored;
                stored["value_key"] = static_cast<std::int64_t>(item.value.value);
                stored["amount"] = item.amount;
                initials.push_back(stored);
            }
            godot::Array materials;
            for (const auto& item : object.material_contributions) {
                godot::Dictionary stored;
                stored["value_key"] = static_cast<std::int64_t>(item.value.value);
                stored["amount"] = item.amount;
                materials.push_back(stored);
            }
            godot::Array base_characteristics;
            for (const auto& item : object.base_characteristics) {
                godot::Dictionary stored;
                stored["characteristic_id"] = static_cast<std::int64_t>(item.characteristic.value);
                stored["amount"] = item.amount;
                base_characteristics.push_back(stored);
            }
            godot::Array genome;
            for (const auto& function : object.genome) {
                godot::Dictionary stored;
                stored["function_type_id"] = static_cast<std::int64_t>(function.type.value);
                godot::Array parameters;
                for (const auto& parameter : function.parameters) {
                    godot::Dictionary value;
                    value["parameter_id"] = static_cast<std::int64_t>(parameter.parameter.value);
                    value["amount"] = parameter.value;
                    parameters.push_back(value);
                }
                stored["parameters"] = parameters;
                genome.push_back(stored);
            }
            godot::Array bindings;
            for (const auto& binding : object.host_bindings) {
                godot::Dictionary stored;
                stored["channel"] = to_godot_string(binding.channel);
                stored["direction"] = snapshot_direction_name(binding.direction);
                stored["value_key"] = static_cast<std::int64_t>(binding.value.value);
                stored["source_kind"] = binding.source_kind == world::HostBinding::SourceKind::value ? "value" : "characteristic";
                stored["characteristic_id"] = static_cast<std::int64_t>(binding.characteristic.value);
                bindings.push_back(stored);
            }
            entry["initial_values"] = initials;
            entry["material_contributions"] = materials;
            entry["base_characteristics"] = base_characteristics;
            entry["genome"] = genome;
            entry["host_bindings"] = bindings;
            templates.push_back(entry);
        }
        result["templates"] = templates;
        godot::Array rules;
        for (const auto& rule : snapshot.world_rules) {
            godot::Dictionary entry;
            entry["source_key"] = static_cast<std::int64_t>(rule.source.value);
            entry["end_buffer_key"] = static_cast<std::int64_t>(rule.end_buffer.value);
            entry["target_key"] = static_cast<std::int64_t>(rule.target.value);
            entry["target_per_source"] = rule.target_per_source;
            rules.push_back(entry);
        }
        result["world_rules"] = rules;
        result["object_construction"] = godot::Variant();
        if (snapshot.object_construction) {
            godot::Dictionary construction;
            construction["calculation_id"] = static_cast<std::int64_t>(snapshot.object_construction->calculation.value);
            godot::Array inputs;
            for (const auto& binding : snapshot.object_construction->inputs) {
                godot::Dictionary item;
                item["input_id"] = static_cast<std::int64_t>(binding.input.value);
                item["kind"] = binding.source.kind == world::ObjectConstructionSourceKind::base_characteristic ? "base" : "function_sum";
                item["characteristic_id"] = static_cast<std::int64_t>(binding.source.characteristic.value);
                inputs.push_back(item);
            }
            godot::Array outputs;
            for (const auto& binding : snapshot.object_construction->outputs) {
                godot::Dictionary item;
                item["output_id"] = static_cast<std::int64_t>(binding.output.value);
                item["characteristic_id"] = static_cast<std::int64_t>(binding.characteristic.value);
                outputs.push_back(item);
            }
            construction["inputs"] = inputs; construction["outputs"] = outputs;
            result["object_construction"] = construction;
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
    return result;
}

bool CLifeWorldEditor::import_world_snapshot(const godot::Dictionary& serialized)
{
    try {
        require_edit_mode();
        world::WorldDefinitionSnapshot snapshot;
        snapshot.schema_version = required_uint32(required_field(serialized, "schema_version"), "schema_version");
        if (snapshot.schema_version != 7) {
            throw std::invalid_argument{"unsupported WorldDefinition snapshot schema version; recreate the test world"};
        }
        snapshot.next_value_key = required_uint32(required_field(serialized, "next_value_key"), "next_value_key");
        snapshot.next_template_id = required_uint32(required_field(serialized, "next_template_id"), "next_template_id");
        snapshot.next_function_type_id =
            required_uint32(required_field(serialized, "next_function_type_id"), "next_function_type_id");
        snapshot.next_parameter_id = required_uint32(required_field(serialized, "next_parameter_id"), "next_parameter_id");
        snapshot.next_calculation_id =
            required_uint32(required_field(serialized, "next_calculation_id"), "next_calculation_id");
        snapshot.next_calculation_port_id =
            required_uint32(required_field(serialized, "next_calculation_port_id"), "next_calculation_port_id");
        snapshot.next_unit_id = required_uint32(required_field(serialized, "next_unit_id"), "next_unit_id");
        snapshot.next_object_characteristic_id = required_uint32(required_field(serialized, "next_object_characteristic_id"), "next_object_characteristic_id");
        for (const godot::Variant& value : required_array(required_field(serialized, "units"), "units")) {
                const godot::Dictionary item = required_dictionary(value, "unit");
                snapshot.units.push_back({
                    .id = {required_uint32(required_field(item, "id"), "unit id")},
                    .symbol = required_string(required_field(item, "symbol"), "unit symbol"),
                    .description = required_string(required_field(item, "description"), "unit description"),
                });
        }
        for (const godot::Variant& value : required_array(required_field(serialized, "object_characteristics"), "object characteristics")) {
            const godot::Dictionary item = required_dictionary(value, "object characteristic");
            snapshot.object_characteristics.push_back({.id = {required_uint32(required_field(item, "id"), "characteristic id")},
                .name = required_string(required_field(item, "name"), "characteristic name")});
        }
        snapshot.next_unit_conversion_id =
            required_uint32(required_field(serialized, "next_unit_conversion_id"), "next_unit_conversion_id");
        for (const godot::Variant& value :
                 required_array(required_field(serialized, "unit_conversions"), "unit_conversions")) {
                const godot::Dictionary item = required_dictionary(value, "unit conversion");
                world::UnitConversionDefinition conversion{
                    .id = {required_uint32(required_field(item, "id"), "unit conversion id")},
                    .source_amount = required_number(required_field(item, "source_amount"), "unit conversion source amount"),
                    .target_amount = required_number(required_field(item, "target_amount"), "unit conversion target amount"),
                };
                for (const godot::Variant& component_value :
                     required_array(required_field(item, "source_unit"), "unit conversion source unit")) {
                    const godot::Dictionary component = required_dictionary(component_value, "unit component");
                    conversion.source_unit.components.push_back({
                        .unit = {required_uint32(required_field(component, "unit_id"), "unit component id")},
                        .exponent = required_int32(required_field(component, "exponent"), "unit component exponent"),
                    });
                }
                for (const godot::Variant& component_value :
                     required_array(required_field(item, "target_unit"), "unit conversion target unit")) {
                    const godot::Dictionary component = required_dictionary(component_value, "unit component");
                    conversion.target_unit.components.push_back({
                        .unit = {required_uint32(required_field(component, "unit_id"), "unit component id")},
                        .exponent = required_int32(required_field(component, "exponent"), "unit component exponent"),
                    });
                }
                snapshot.unit_conversions.push_back(std::move(conversion));
        }
        for (const godot::Variant& value : required_array(required_field(serialized, "values"), "values")) {
            const godot::Dictionary item = required_dictionary(value, "value");
            world::ValueDefinition stored{
                .key = {required_uint32(required_field(item, "id"), "value id")},
                .name = required_string(required_field(item, "name"), "value name"),
            };
            const godot::Variant unit_value = required_field(item, "unit");
            if (unit_value.get_type() != godot::Variant::NIL) {
                    world::UnitExpression expression;
                    for (const godot::Variant& component_value : required_array(unit_value, "value unit")) {
                        const godot::Dictionary component = required_dictionary(component_value, "unit component");
                        expression.components.push_back({
                            .unit = {required_uint32(required_field(component, "unit_id"), "unit component id")},
                            .exponent = required_int32(required_field(component, "exponent"), "unit component exponent"),
                        });
                    }
                    stored.unit = std::move(expression);
            }
            snapshot.values.push_back(std::move(stored));
        }
        for (const godot::Variant& value : required_array(required_field(serialized, "calculations"), "calculations")) {
            const godot::Dictionary item = required_dictionary(value, "calculation");
            world::CalculationSnapshot calculation{
                .id = {required_uint32(required_field(item, "id"), "calculation id")},
                .name = required_string(required_field(item, "name"), "calculation name"),
            };
            for (const godot::Variant& port_value : required_array(required_field(item, "inputs"), "calculation inputs")) {
                const godot::Dictionary port = required_dictionary(port_value, "calculation input");
                calculation.inputs.push_back({
                    .id = {required_uint32(required_field(port, "id"), "calculation input id")},
                    .name = required_string(required_field(port, "name"), "calculation input name"),
                });
            }
            for (const godot::Variant& port_value : required_array(required_field(item, "outputs"), "calculation outputs")) {
                const godot::Dictionary port = required_dictionary(port_value, "calculation output");
                calculation.outputs.push_back({
                    .id = {required_uint32(required_field(port, "id"), "calculation output id")},
                    .name = required_string(required_field(port, "name"), "calculation output name"),
                    .expression_source = required_string(required_field(port, "expression_source"), "calculation output expression"),
                });
            }
            snapshot.calculations.push_back(std::move(calculation));
        }
        for (const godot::Variant& value : required_array(required_field(serialized, "function_types"), "function_types")) {
            const godot::Dictionary item = required_dictionary(value, "function type");
            world::FunctionTypeSnapshot type{
                .id = {required_uint32(required_field(item, "id"), "function type id")},
                .name = required_string(required_field(item, "name"), "function type name"),
            };
            for (const godot::Variant& parameter_value :
                 required_array(required_field(item, "genome_parameters"), "genome parameters")) {
                const godot::Dictionary parameter = required_dictionary(parameter_value, "genome parameter");
                type.genome_parameters.push_back({
                    .id = {required_uint32(required_field(parameter, "id"), "genome parameter id")},
                    .name = required_string(required_field(parameter, "name"), "genome parameter name"),
                    .default_value = required_number(required_field(parameter, "default_value"), "genome parameter default"),
                });
            }
            for (const godot::Variant& binding_value :
                 required_array(required_field(item, "calculations"), "function calculation bindings")) {
                const godot::Dictionary stored = required_dictionary(binding_value, "function calculation binding");
                world::FunctionCalculationBinding binding{
                    .calculation = {required_uint32(required_field(stored, "calculation_id"), "calculation id")},
                };
                for (const godot::Variant& input_value :
                     required_array(required_field(stored, "inputs"), "calculation input bindings")) {
                    const godot::Dictionary input = required_dictionary(input_value, "calculation input binding");
                    binding.inputs.push_back({
                        .input = {required_uint32(required_field(input, "input_id"), "input id")},
                        .genome_parameter = {required_uint32(required_field(input, "genome_parameter_id"),
                                                            "genome parameter id")},
                    });
                }
                type.calculations.push_back(std::move(binding));
            }
            const godot::Variant process_value = required_field(item, "process");
            if (process_value.get_type() != godot::Variant::NIL) {
                const godot::Dictionary process = required_dictionary(process_value, "process");
                world::FunctionProcessDefinition stored{
                    .input = {required_uint32(required_field(process, "input_key"), "process input key")},
                    .throughput = function_value_source(required_dictionary(
                        required_field(process, "throughput_source"), "process throughput source")),
                    .conversion = {required_uint32(required_field(process, "conversion_id"), "process conversion")},
                };
                for (const godot::Variant& output_value : required_array(required_field(process, "outputs"), "process outputs")) {
                        const godot::Dictionary output = required_dictionary(output_value, "process output");
                        stored.outputs.push_back({
                            .output = {required_uint32(required_field(output, "output_key"), "process output key")},
                            .allocation = function_value_source(required_dictionary(
                                required_field(output, "allocation_source"), "process allocation source")),
                        });
                }
                type.process = std::move(stored);
            }
            const godot::Variant buffer_value = required_field(item, "buffer_process");
            if (buffer_value.get_type() != godot::Variant::NIL) {
                const godot::Dictionary buffer = required_dictionary(buffer_value, "buffer process");
                type.buffer_process = {
                    .value = {required_uint32(required_field(buffer, "value_key"), "buffer value key")},
                    .capacity = function_value_source(required_dictionary(required_field(buffer, "capacity_source"),
                                                                          "buffer capacity source")),
                    .throughput = function_value_source(required_dictionary(required_field(buffer, "throughput_source"),
                                                                            "buffer throughput source")),
                    .leakage = function_value_source(required_dictionary(required_field(buffer, "leakage_source"),
                                                                         "buffer leakage source")),
                };
            }
            for (const godot::Variant& contribution_value :
                 required_array(required_field(item, "material_contributions"), "function material contributions")) {
                const godot::Dictionary contribution = required_dictionary(contribution_value, "function material contribution");
                type.material_contributions.push_back({
                    .value = {required_uint32(required_field(contribution, "value_key"), "material value key")},
                    .amount = function_value_source(required_dictionary(required_field(contribution, "amount_source"),
                                                                        "material amount source")),
                });
            }
            for (const godot::Variant& contribution_value : required_array(required_field(item, "characteristic_contributions"), "function characteristic contributions")) {
                const auto contribution = required_dictionary(contribution_value, "function characteristic contribution");
                type.characteristic_contributions.push_back({
                    .characteristic = {required_uint32(required_field(contribution, "characteristic_id"), "characteristic id")},
                    .amount = function_value_source(required_dictionary(required_field(contribution, "amount_source"), "characteristic amount source")),
                });
            }
            snapshot.function_types.push_back(std::move(type));
        }
        for (const godot::Variant& value : required_array(required_field(serialized, "templates"), "templates")) {
            const godot::Dictionary item = required_dictionary(value, "template");
            world::ObjectTemplate object{
                .id = {required_uint32(required_field(item, "id"), "template id")},
                .name = required_string(required_field(item, "name"), "template name"),
            };
            for (const godot::Variant& stored_value : required_array(required_field(item, "initial_values"), "initial values")) {
                const godot::Dictionary stored = required_dictionary(stored_value, "initial value");
                object.initial_values.push_back({
                    .value = {required_uint32(required_field(stored, "value_key"), "initial value key")},
                    .amount = required_number(required_field(stored, "amount"), "initial amount"),
                });
            }
            for (const godot::Variant& stored_value : required_array(required_field(item, "material_contributions"), "template materials")) {
                const godot::Dictionary stored = required_dictionary(stored_value, "template material contribution");
                object.material_contributions.push_back({
                    .value = {required_uint32(required_field(stored, "value_key"), "template material value key")},
                    .amount = required_number(required_field(stored, "amount"), "template material amount"),
                });
            }
            for (const godot::Variant& stored_value : required_array(required_field(item, "base_characteristics"), "template base characteristics")) {
                const auto stored = required_dictionary(stored_value, "template base characteristic");
                object.base_characteristics.push_back({
                    .characteristic = {required_uint32(required_field(stored, "characteristic_id"), "characteristic id")},
                    .amount = required_number(required_field(stored, "amount"), "base characteristic amount"),
                });
            }
            for (const godot::Variant& function_value : required_array(required_field(item, "genome"), "template genome")) {
                const godot::Dictionary function = required_dictionary(function_value, "genome function");
                world::GenomeFunctionInstance instance{
                    .type = {required_uint32(required_field(function, "function_type_id"), "genome function type")},
                };
                for (const godot::Variant& parameter_value : required_array(required_field(function, "parameters"), "genome parameters")) {
                    const godot::Dictionary parameter = required_dictionary(parameter_value, "genome parameter value");
                    instance.parameters.push_back({
                        .parameter = {required_uint32(required_field(parameter, "parameter_id"), "genome parameter id")},
                        .value = required_number(required_field(parameter, "amount"), "genome parameter amount"),
                    });
                }
                object.genome.push_back(std::move(instance));
            }
            for (const godot::Variant& binding_value : required_array(required_field(item, "host_bindings"), "host bindings")) {
                const godot::Dictionary binding = required_dictionary(binding_value, "host binding");
                const std::string source_kind = required_string(required_field(binding, "source_kind"), "host binding source kind");
                world::HostBinding stored{.channel = required_string(required_field(binding, "channel"), "host binding channel"),
                    .direction = snapshot_direction(required_field(binding, "direction"))};
                if (source_kind == "value") stored.value = {required_uint32(required_field(binding, "value_key"), "host binding value key")};
                else if (source_kind == "characteristic") { stored.source_kind = world::HostBinding::SourceKind::object_characteristic; stored.characteristic = {required_uint32(required_field(binding, "characteristic_id"), "host binding characteristic id")}; }
                else throw std::invalid_argument{"host binding source kind is invalid"};
                object.host_bindings.push_back(std::move(stored));
            }
            snapshot.templates.push_back(std::move(object));
        }
        for (const godot::Variant& value : required_array(required_field(serialized, "world_rules"), "world rules")) {
            const godot::Dictionary item = required_dictionary(value, "world rule");
            snapshot.world_rules.push_back({
                .source = {required_uint32(required_field(item, "source_key"), "world rule source")},
                .end_buffer = {required_uint32(required_field(item, "end_buffer_key"), "world rule end buffer")},
                .target = {required_uint32(required_field(item, "target_key"), "world rule target")},
                .target_per_source = required_number(required_field(item, "target_per_source"), "world rule factor"),
            });
        }
        const godot::Variant construction_value = required_field(serialized, "object_construction");
        if (construction_value.get_type() != godot::Variant::NIL) {
            const auto construction = required_dictionary(construction_value, "object construction");
            world::ObjectConstructionDefinition stored{.calculation = {required_uint32(required_field(construction, "calculation_id"), "construction calculation id")}};
            for (const godot::Variant& input_value : required_array(required_field(construction, "inputs"), "construction inputs")) {
                const auto input = required_dictionary(input_value, "construction input");
                const auto kind = required_string(required_field(input, "kind"), "construction source kind");
                if (kind != "base" && kind != "function_sum") throw std::invalid_argument{"invalid construction source kind"};
                stored.inputs.push_back({.input = {required_uint32(required_field(input, "input_id"), "construction input id")},
                    .source = {.kind = kind == "base" ? world::ObjectConstructionSourceKind::base_characteristic : world::ObjectConstructionSourceKind::function_contribution_sum,
                               .characteristic = {required_uint32(required_field(input, "characteristic_id"), "characteristic id")}}});
            }
            for (const godot::Variant& output_value : required_array(required_field(construction, "outputs"), "construction outputs")) {
                const auto output = required_dictionary(output_value, "construction output");
                stored.outputs.push_back({.output = {required_uint32(required_field(output, "output_id"), "construction output id")},
                    .characteristic = {required_uint32(required_field(output, "characteristic_id"), "characteristic id")}});
            }
            snapshot.object_construction = std::move(stored);
        }
        world::WorldDefinition restored = world::WorldDefinition::from_snapshot(snapshot);
        definition_ = std::move(restored);
        selected_template_.reset();
        host_inputs_.clear();
        runtime_.reset();
        preview_object_.reset();
        run_definition_.reset();
        run_template_.reset();
        accumulator_ = 0.0;
        tick_ = 0;
        playing_ = false;
        clear_error();
        return true;
    } catch (...) {
        capture_current_error();
        return false;
    }
}

godot::String CLifeWorldEditor::get_last_error() const
{
    try {
        return to_godot_string(last_error_);
    } catch (...) {
        return godot::String{"CLife error message is unavailable"};
    }
}
void CLifeWorldEditor::clear_last_error() { clear_error(); }

void CLifeWorldEditor::require_edit_mode() const
{
    if (runtime_) {
        throw std::logic_error{"world definition cannot be edited while runtime is active"};
    }
}

void CLifeWorldEditor::capture_current_error() noexcept
{
    try {
        throw;
    } catch (const std::exception& error) {
        try {
            last_error_ = error.what();
        } catch (...) {
            last_error_.clear();
        }
    } catch (...) {
        try {
            last_error_ = "unknown CLife error";
        } catch (...) {
            last_error_.clear();
        }
    }
}

void CLifeWorldEditor::clear_error() noexcept { last_error_.clear(); }

void CLifeWorldEditor::rebuild_runtime_from_snapshot()
{
    runtime_ = std::make_unique<world::RuntimeWorld>(*run_definition_);
    preview_object_ = runtime_->instantiate(*run_template_);
    accumulator_ = 0.0;
    tick_ = 0;
}

void CLifeWorldEditor::stage_inputs_and_step()
{
    const world::ObjectTemplate& object = run_definition_->object_template(*run_template_);
    for (const world::HostBinding& binding : object.host_bindings) {
        if (binding.direction == world::HostChannelDirection::input) {
            runtime_->set_input(*preview_object_, binding.value, host_inputs_.at(binding.channel));
        }
    }
    runtime_->step();
    ++tick_;
}

void CLifeWorldEditor::ensure_host_inputs(const world::WorldDefinition& definition, world::TemplateId id)
{
    for (const world::HostBinding& binding : definition.object_template(id).host_bindings) {
        if (binding.direction == world::HostChannelDirection::input) {
            host_inputs_.try_emplace(binding.channel, 0.0);
        }
    }
}

void CLifeWorldEditor::_bind_methods()
{
    godot::ClassDB::bind_method(godot::D_METHOD("get_values"), &CLifeWorldEditor::get_values);
    godot::ClassDB::bind_method(godot::D_METHOD("get_units"), &CLifeWorldEditor::get_units);
    godot::ClassDB::bind_method(godot::D_METHOD("get_unit_conversions"), &CLifeWorldEditor::get_unit_conversions);
    godot::ClassDB::bind_method(godot::D_METHOD("get_object_characteristics"), &CLifeWorldEditor::get_object_characteristics);
    godot::ClassDB::bind_method(godot::D_METHOD("get_object_construction"), &CLifeWorldEditor::get_object_construction);
    godot::ClassDB::bind_method(godot::D_METHOD("get_templates"), &CLifeWorldEditor::get_templates);
    godot::ClassDB::bind_method(godot::D_METHOD("get_function_types"), &CLifeWorldEditor::get_function_types);
    godot::ClassDB::bind_method(godot::D_METHOD("get_calculations"), &CLifeWorldEditor::get_calculations);
    godot::ClassDB::bind_method(godot::D_METHOD("get_initial_values", "template_id"),
                                &CLifeWorldEditor::get_initial_values);
    godot::ClassDB::bind_method(godot::D_METHOD("get_material_contributions", "template_id"),
                                &CLifeWorldEditor::get_material_contributions);
    godot::ClassDB::bind_method(godot::D_METHOD("get_genome", "template_id"), &CLifeWorldEditor::get_genome);
    godot::ClassDB::bind_method(godot::D_METHOD("get_world_rules"), &CLifeWorldEditor::get_world_rules);
    godot::ClassDB::bind_method(godot::D_METHOD("get_bindings", "template_id"), &CLifeWorldEditor::get_bindings);
    godot::ClassDB::bind_method(godot::D_METHOD("get_template_characteristic_preview", "template_id"), &CLifeWorldEditor::get_template_characteristic_preview);
    godot::ClassDB::bind_method(godot::D_METHOD("get_host_capabilities"), &CLifeWorldEditor::get_host_capabilities);
    godot::ClassDB::bind_method(godot::D_METHOD("add_value", "name"), &CLifeWorldEditor::add_value);
    godot::ClassDB::bind_method(godot::D_METHOD("add_unit", "symbol", "description"), &CLifeWorldEditor::add_unit, DEFVAL(godot::String{}));
    godot::ClassDB::bind_method(godot::D_METHOD("update_unit", "unit_id", "symbol", "description"), &CLifeWorldEditor::update_unit);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_unit", "unit_id"), &CLifeWorldEditor::remove_unit);
    godot::ClassDB::bind_method(godot::D_METHOD("add_unit_conversion", "source_unit_id", "source_amount",
                                                "target_unit_id", "target_amount"),
                                &CLifeWorldEditor::add_unit_conversion);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_unit_conversion", "conversion_id"),
                                &CLifeWorldEditor::remove_unit_conversion);
    godot::ClassDB::bind_method(godot::D_METHOD("set_value_unit", "value_key", "unit_id"),
                                &CLifeWorldEditor::set_value_unit);
    godot::ClassDB::bind_method(godot::D_METHOD("clear_value_unit", "value_key"),
                                &CLifeWorldEditor::clear_value_unit);
    godot::ClassDB::bind_method(godot::D_METHOD("add_object_characteristic", "name"), &CLifeWorldEditor::add_object_characteristic);
    godot::ClassDB::bind_method(godot::D_METHOD("rename_object_characteristic", "characteristic_id", "name"), &CLifeWorldEditor::rename_object_characteristic);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_object_characteristic", "characteristic_id"), &CLifeWorldEditor::remove_object_characteristic);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_function_process", "function_type_id", "input_value_key", "throughput_source",
                        "conversion_id", "output_value_key", "allocation_source"),
        &CLifeWorldEditor::set_function_process);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_function_process_full", "function_type_id", "input_value_key", "throughput_source",
                        "conversion_id", "outputs"),
        &CLifeWorldEditor::set_function_process_full);
    godot::ClassDB::bind_method(
        godot::D_METHOD("add_function_process_output", "function_type_id", "output_value_key", "allocation_source"),
        &CLifeWorldEditor::add_function_process_output);
    godot::ClassDB::bind_method(
        godot::D_METHOD("change_function_process_settings", "function_type_id", "input_value_key",
                        "throughput_source", "conversion_id"),
        &CLifeWorldEditor::change_function_process_settings);
    godot::ClassDB::bind_method(
        godot::D_METHOD("change_function_process_output", "function_type_id", "existing_output_value_key",
                        "output_value_key", "allocation_source"),
        &CLifeWorldEditor::change_function_process_output);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_function_process_output", "function_type_id", "output_value_key"),
                                &CLifeWorldEditor::remove_function_process_output);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_function_process", "function_type_id"),
                                &CLifeWorldEditor::remove_function_process);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_buffer_process", "function_type_id", "value_key", "capacity_source",
                        "throughput_source", "leakage_source"),
        &CLifeWorldEditor::set_buffer_process);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_buffer_process", "function_type_id"),
                                &CLifeWorldEditor::remove_buffer_process);
    godot::ClassDB::bind_method(godot::D_METHOD("rename_value", "key", "name"), &CLifeWorldEditor::rename_value);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_value", "key"), &CLifeWorldEditor::remove_value);
    godot::ClassDB::bind_method(godot::D_METHOD("add_template", "name"), &CLifeWorldEditor::add_template);
    godot::ClassDB::bind_method(godot::D_METHOD("add_function_type", "name"), &CLifeWorldEditor::add_function_type);
    godot::ClassDB::bind_method(godot::D_METHOD("rename_function_type", "function_type_id", "name"),
                                &CLifeWorldEditor::rename_function_type);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_function_type", "function_type_id"),
                                &CLifeWorldEditor::remove_function_type);
    godot::ClassDB::bind_method(godot::D_METHOD("add_genome_parameter", "function_type_id", "name", "default_value"),
                                &CLifeWorldEditor::add_genome_parameter);
    godot::ClassDB::bind_method(
        godot::D_METHOD("update_genome_parameter", "function_type_id", "parameter_id", "name", "default_value"),
        &CLifeWorldEditor::update_genome_parameter);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_genome_parameter", "function_type_id", "parameter_id"),
                                &CLifeWorldEditor::remove_genome_parameter);
    godot::ClassDB::bind_method(godot::D_METHOD("add_calculation", "name"), &CLifeWorldEditor::add_calculation);
    godot::ClassDB::bind_method(godot::D_METHOD("add_calculation_input", "calculation_id", "name"),
                                &CLifeWorldEditor::add_calculation_input);
    godot::ClassDB::bind_method(godot::D_METHOD("add_calculation_output", "calculation_id", "name", "expression"),
                                &CLifeWorldEditor::add_calculation_output);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_calculation", "calculation_id"),
                                &CLifeWorldEditor::remove_calculation);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_calculation_input", "calculation_id", "input_port_id"),
                                &CLifeWorldEditor::remove_calculation_input);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_calculation_output", "calculation_id", "output_port_id"),
                                &CLifeWorldEditor::remove_calculation_output);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_calculation_output_expression", "calculation_id", "output_port_id", "expression"),
        &CLifeWorldEditor::set_calculation_output_expression);
    godot::ClassDB::bind_method(godot::D_METHOD("evaluate_calculation", "calculation_id", "inputs"),
                                &CLifeWorldEditor::evaluate_calculation);
    godot::ClassDB::bind_method(godot::D_METHOD("rename_template", "id", "name"), &CLifeWorldEditor::rename_template);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_template", "id"), &CLifeWorldEditor::remove_template);
    godot::ClassDB::bind_method(godot::D_METHOD("set_initial_value", "template_id", "value_key", "amount"),
                                &CLifeWorldEditor::set_initial_value);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_initial_value", "template_id", "value_key"),
                                &CLifeWorldEditor::remove_initial_value);
    godot::ClassDB::bind_method(godot::D_METHOD("set_template_base_characteristic", "template_id", "characteristic_id", "amount"), &CLifeWorldEditor::set_template_base_characteristic);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_template_base_characteristic", "template_id", "characteristic_id"), &CLifeWorldEditor::remove_template_base_characteristic);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_function_calculation_binding", "function_type_id", "calculation_id", "input_bindings"),
        &CLifeWorldEditor::set_function_calculation_binding);
    godot::ClassDB::bind_method(
        godot::D_METHOD("remove_function_calculation_binding", "function_type_id", "calculation_id"),
        &CLifeWorldEditor::remove_function_calculation_binding);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_function_material_contribution", "function_type_id", "value_key", "amount_source"),
        &CLifeWorldEditor::set_function_material_contribution);
    godot::ClassDB::bind_method(
        godot::D_METHOD("remove_function_material_contribution", "function_type_id", "value_key"),
        &CLifeWorldEditor::remove_function_material_contribution);
    godot::ClassDB::bind_method(godot::D_METHOD("set_function_characteristic_contribution", "function_type_id", "characteristic_id", "amount_source"), &CLifeWorldEditor::set_function_characteristic_contribution);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_function_characteristic_contribution", "function_type_id", "characteristic_id"), &CLifeWorldEditor::remove_function_characteristic_contribution);
    godot::ClassDB::bind_method(godot::D_METHOD("add_genome_function", "template_id", "function_type_id"),
                                &CLifeWorldEditor::add_genome_function);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_genome_parameter", "template_id", "index", "parameter_id", "value"),
        &CLifeWorldEditor::set_genome_parameter);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_genome_function", "template_id", "index"),
                                &CLifeWorldEditor::remove_genome_function);
    godot::ClassDB::bind_method(
        godot::D_METHOD("add_world_rule", "source_key", "end_buffer_key", "target_key", "target_per_source"),
        &CLifeWorldEditor::add_world_rule);
    godot::ClassDB::bind_method(
        godot::D_METHOD("change_world_rule", "index", "source_key", "end_buffer_key", "target_key",
                        "target_per_source"),
        &CLifeWorldEditor::change_world_rule);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_world_rule", "index"), &CLifeWorldEditor::remove_world_rule);
    godot::ClassDB::bind_method(godot::D_METHOD("add_host_binding", "template_id", "channel", "direction", "value_key"),
                                &CLifeWorldEditor::add_host_binding);
    godot::ClassDB::bind_method(
        godot::D_METHOD("change_host_binding", "template_id", "index", "channel", "direction", "value_key"),
        &CLifeWorldEditor::change_host_binding);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_host_binding", "template_id", "index"),
                                &CLifeWorldEditor::remove_host_binding);
    godot::ClassDB::bind_method(godot::D_METHOD("set_object_construction", "calculation_id", "inputs", "outputs"), &CLifeWorldEditor::set_object_construction);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_object_construction"), &CLifeWorldEditor::remove_object_construction);
    godot::ClassDB::bind_method(godot::D_METHOD("select_template", "template_id"), &CLifeWorldEditor::select_template);
    godot::ClassDB::bind_method(godot::D_METHOD("get_selected_template_id"),
                                &CLifeWorldEditor::get_selected_template_id);
    godot::ClassDB::bind_method(godot::D_METHOD("run"), &CLifeWorldEditor::run);
    godot::ClassDB::bind_method(godot::D_METHOD("stop"), &CLifeWorldEditor::stop);
    godot::ClassDB::bind_method(godot::D_METHOD("reset_runtime"), &CLifeWorldEditor::reset_runtime);
    godot::ClassDB::bind_method(godot::D_METHOD("play"), &CLifeWorldEditor::play);
    godot::ClassDB::bind_method(godot::D_METHOD("pause"), &CLifeWorldEditor::pause);
    godot::ClassDB::bind_method(godot::D_METHOD("step_once"), &CLifeWorldEditor::step_once);
    godot::ClassDB::bind_method(godot::D_METHOD("advance_time", "elapsed_seconds"), &CLifeWorldEditor::advance_time);
    godot::ClassDB::bind_method(godot::D_METHOD("is_run_active"), &CLifeWorldEditor::is_run_active);
    godot::ClassDB::bind_method(godot::D_METHOD("is_playing"), &CLifeWorldEditor::is_playing);
    godot::ClassDB::bind_method(godot::D_METHOD("get_tick"), &CLifeWorldEditor::get_tick);
    godot::ClassDB::bind_method(godot::D_METHOD("get_fixed_tick_seconds"), &CLifeWorldEditor::get_fixed_tick_seconds);
    godot::ClassDB::bind_method(godot::D_METHOD("get_preview_object_id"), &CLifeWorldEditor::get_preview_object_id);
    godot::ClassDB::bind_method(godot::D_METHOD("get_runtime_values"), &CLifeWorldEditor::get_runtime_values);
    godot::ClassDB::bind_method(godot::D_METHOD("get_runtime_functions"), &CLifeWorldEditor::get_runtime_functions);
    godot::ClassDB::bind_method(godot::D_METHOD("get_last_end_buffer"), &CLifeWorldEditor::get_last_end_buffer);
    godot::ClassDB::bind_method(godot::D_METHOD("get_host_inputs"), &CLifeWorldEditor::get_host_inputs);
    godot::ClassDB::bind_method(godot::D_METHOD("get_host_outputs"), &CLifeWorldEditor::get_host_outputs);
    godot::ClassDB::bind_method(godot::D_METHOD("set_host_input", "channel", "amount"),
                                &CLifeWorldEditor::set_host_input);
    godot::ClassDB::bind_method(godot::D_METHOD("export_world_snapshot"), &CLifeWorldEditor::export_world_snapshot);
    godot::ClassDB::bind_method(godot::D_METHOD("import_world_snapshot", "snapshot"),
                                &CLifeWorldEditor::import_world_snapshot);
    godot::ClassDB::bind_method(godot::D_METHOD("get_last_error"), &CLifeWorldEditor::get_last_error);
    godot::ClassDB::bind_method(godot::D_METHOD("clear_last_error"), &CLifeWorldEditor::clear_last_error);
}

} // namespace clife::godot_adapter
