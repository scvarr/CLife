#include "clife_world_editor_internal.hpp"

namespace clife::godot_adapter {

using namespace detail;

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
                if (binding.source.kind == world::ObjectConstructionSourceKind::base_characteristic) {
                    item["kind"] = "base";
                    item["characteristic_id"] = static_cast<std::int64_t>(binding.source.characteristic.value);
                } else if (binding.source.kind == world::ObjectConstructionSourceKind::function_contribution_sum) {
                    item["kind"] = "function_sum";
                    item["characteristic_id"] = static_cast<std::int64_t>(binding.source.characteristic.value);
                } else if (binding.source.kind == world::ObjectConstructionSourceKind::material_amount) {
                    item["kind"] = "material";
                    item["value_key"] = static_cast<std::int64_t>(binding.source.value.value);
                } else {
                    throw std::invalid_argument{"invalid construction source kind"};
                }
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
                if (kind != "base" && kind != "function_sum" && kind != "material") throw std::invalid_argument{"invalid construction source kind"};
                stored.inputs.push_back({.input = {required_uint32(required_field(input, "input_id"), "construction input id")},
                    .source = kind == "material"
                                  ? world::ObjectConstructionSource{.kind = world::ObjectConstructionSourceKind::material_amount,
                                                                    .value = {required_uint32(required_field(input, "value_key"), "material value key")}}
                                  : world::ObjectConstructionSource{.kind = kind == "base" ? world::ObjectConstructionSourceKind::base_characteristic : world::ObjectConstructionSourceKind::function_contribution_sum,
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

} // namespace clife::godot_adapter
