#include "clife_world_editor_internal.hpp"

namespace clife::godot_adapter {

using namespace detail;

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
                                            const godot::Dictionary& throughput_source, std::int64_t raw_output,
                                            const godot::Dictionary& allocation_source, std::int64_t raw_conversion)
{
    return edit([&] {
        definition_.set_function_process(function_type_id(raw_type), {
            .input = value_key(raw_input),
            .throughput = function_value_source(throughput_source),
            .outputs = {{.output = value_key(raw_output), .allocation = function_value_source(allocation_source),
                         .conversion = unit_conversion_id(raw_conversion)}},
        });
    });
}

bool CLifeWorldEditor::set_function_process_full(std::int64_t raw_type, std::int64_t raw_input,
                                                 const godot::Dictionary& throughput_source,
                                                 const godot::Array& outputs)
{
    return edit([&] {
        world::FunctionProcessDefinition process{
            .input = value_key(raw_input),
            .throughput = function_value_source(throughput_source),
        };
        process.outputs.reserve(outputs.size());
        for (const godot::Variant& value : outputs) {
            const godot::Dictionary output = required_dictionary(value, "function process output");
            process.outputs.push_back({
                .output = value_key(required_uint32(required_field(output, "output_key"), "output_key")),
                .allocation = function_value_source(required_dictionary(
                    required_field(output, "allocation_source"), "allocation_source")),
                .conversion = unit_conversion_id(required_uint32(required_field(output, "conversion_id"), "conversion_id")),
            });
        }
        definition_.set_function_process(function_type_id(raw_type), std::move(process));
    });
}

bool CLifeWorldEditor::add_function_process_output(std::int64_t raw_type, std::int64_t raw_output,
                                                   const godot::Dictionary& allocation_source, std::int64_t raw_conversion)
{
    return edit([&] { definition_.add_function_process_output(function_type_id(raw_type),
                                                                 {.output = value_key(raw_output),
                                                                  .allocation = function_value_source(allocation_source),
                                                                  .conversion = unit_conversion_id(raw_conversion)}); });
}

bool CLifeWorldEditor::change_function_process_settings(std::int64_t raw_type, std::int64_t raw_input,
                                                        const godot::Dictionary& throughput_source)
{
    return edit([&] { definition_.change_function_process_settings(function_type_id(raw_type), value_key(raw_input),
                                                                     function_value_source(throughput_source)); });
}

bool CLifeWorldEditor::change_function_process_output(std::int64_t raw_type, std::int64_t raw_existing_output,
                                                      std::int64_t raw_output,
                                                      const godot::Dictionary& allocation_source,
                                                      std::int64_t raw_conversion)
{
    return edit([&] { definition_.change_function_process_output(
                          function_type_id(raw_type), value_key(raw_existing_output),
                          {.output = value_key(raw_output),
                           .allocation = function_value_source(allocation_source),
                           .conversion = unit_conversion_id(raw_conversion)}); });
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

bool CLifeWorldEditor::rename_calculation(std::int64_t raw_calculation_id, const godot::String& name)
{
    return edit([&] { definition_.rename_calculation(calculation_id(raw_calculation_id), to_std_string(name)); });
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

bool CLifeWorldEditor::add_calculation_world_rule(std::int64_t raw_source_key, std::int64_t raw_calculation_id,
                                                  const godot::Array& input_bindings,
                                                  const godot::Array& output_bindings)
{
    return edit([&] {
        world::CalculationWorldRuleDefinition rule{
            .source = value_key(raw_source_key),
            .calculation = calculation_id(raw_calculation_id),
        };
        for (const godot::Variant& value : input_bindings) {
            rule.inputs.push_back(calculation_world_rule_input_binding(
                required_dictionary(value, "calculation world rule input binding")));
        }
        for (const godot::Variant& value : output_bindings) {
            rule.outputs.push_back(calculation_world_rule_output_binding(
                required_dictionary(value, "calculation world rule output binding")));
        }
        (void)definition_.add_calculation_world_rule(std::move(rule));
    });
}

bool CLifeWorldEditor::change_calculation_world_rule(std::int64_t index, std::int64_t raw_source_key,
                                                     std::int64_t raw_calculation_id,
                                                     const godot::Array& input_bindings,
                                                     const godot::Array& output_bindings)
{
    return edit([&] {
        world::CalculationWorldRuleDefinition rule{
            .source = value_key(raw_source_key),
            .calculation = calculation_id(raw_calculation_id),
        };
        for (const godot::Variant& value : input_bindings) {
            rule.inputs.push_back(calculation_world_rule_input_binding(
                required_dictionary(value, "calculation world rule input binding")));
        }
        for (const godot::Variant& value : output_bindings) {
            rule.outputs.push_back(calculation_world_rule_output_binding(
                required_dictionary(value, "calculation world rule output binding")));
        }
        definition_.change_calculation_world_rule(item_index(index), std::move(rule));
    });
}

bool CLifeWorldEditor::remove_calculation_world_rule(std::int64_t index)
{
    return edit([&] { definition_.remove_calculation_world_rule(item_index(index)); });
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
            if (kind != "base" && kind != "function_sum" && kind != "material") {
                throw std::invalid_argument{"construction source kind must be base, function_sum, or material"};
            }
            construction.inputs.push_back({
                .input = calculation_port_id(required_uint32(required_field(item, "input_id"), "construction input id")),
                .source = kind == "material"
                              ? world::ObjectConstructionSource{.kind = world::ObjectConstructionSourceKind::material_amount,
                                                                .value = value_key(required_uint32(required_field(item, "value_key"), "construction material value key"))}
                              : world::ObjectConstructionSource{.kind = kind == "base" ? world::ObjectConstructionSourceKind::base_characteristic : world::ObjectConstructionSourceKind::function_contribution_sum,
                                                                .characteristic = object_characteristic_id(required_uint32(required_field(item, "characteristic_id"), "construction characteristic id"))},
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

} // namespace clife::godot_adapter
