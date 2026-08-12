#include "clife_world_editor_internal.hpp"

namespace clife::godot_adapter {

using namespace detail;

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

godot::Array CLifeWorldEditor::get_calculation_world_rules()
{
    godot::Array result;
    try {
        const auto& rules = definition_.calculation_world_rules();
        for (std::size_t index = 0; index < rules.size(); ++index) {
            const world::CalculationWorldRuleDefinition& rule = rules[index];
            godot::Dictionary item;
            item["index"] = static_cast<std::int64_t>(index);
            item["source"] = static_cast<std::int64_t>(rule.source.value);
            item["calculation"] = static_cast<std::int64_t>(rule.calculation.value);
            godot::Array inputs;
            for (const auto& binding : rule.inputs) {
                inputs.push_back(calculation_world_rule_input_dictionary(binding));
            }
            godot::Array outputs;
            for (const auto& binding : rule.outputs) {
                outputs.push_back(calculation_world_rule_output_dictionary(binding));
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

godot::PackedFloat64Array CLifeWorldEditor::sample_template_shape(
    std::int64_t raw_template_id, const godot::PackedVector3Array& directions)
{
    godot::PackedFloat64Array result;
    try {
        const world::ShapePhenotype shape = world::compile_semantic_shape_phenotype(definition_, template_id(raw_template_id));
        result.resize(directions.size());
        for (std::int64_t index = 0; index < directions.size(); ++index) {
            const godot::Vector3 direction = directions[index];
            result.set(index, shape.radius(direction.x, direction.y, direction.z));
        }
        clear_error();
    } catch (...) {
        capture_current_error();
        result.clear();
    }
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

} // namespace clife::godot_adapter
