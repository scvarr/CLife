#include "clife_world_editor_internal.hpp"

#include <godot_cpp/core/class_db.hpp>

namespace clife::godot_adapter {

using namespace detail;

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
    godot::ClassDB::bind_method(godot::D_METHOD("get_calculation_world_rules"),
                                &CLifeWorldEditor::get_calculation_world_rules);
    godot::ClassDB::bind_method(godot::D_METHOD("get_bindings", "template_id"), &CLifeWorldEditor::get_bindings);
    godot::ClassDB::bind_method(godot::D_METHOD("get_template_characteristic_preview", "template_id"), &CLifeWorldEditor::get_template_characteristic_preview);
    godot::ClassDB::bind_method(godot::D_METHOD("sample_template_shape", "template_id", "directions"),
                                &CLifeWorldEditor::sample_template_shape);
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
                        "output_value_key", "allocation_source", "conversion_id"),
        &CLifeWorldEditor::set_function_process);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_function_process_full", "function_type_id", "input_value_key", "throughput_source",
                        "outputs"),
        &CLifeWorldEditor::set_function_process_full);
    godot::ClassDB::bind_method(
        godot::D_METHOD("add_function_process_output", "function_type_id", "output_value_key", "allocation_source", "conversion_id"),
        &CLifeWorldEditor::add_function_process_output);
    godot::ClassDB::bind_method(
        godot::D_METHOD("change_function_process_settings", "function_type_id", "input_value_key",
                        "throughput_source"),
        &CLifeWorldEditor::change_function_process_settings);
    godot::ClassDB::bind_method(
        godot::D_METHOD("change_function_process_output", "function_type_id", "existing_output_value_key",
                        "output_value_key", "allocation_source", "conversion_id"),
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
    godot::ClassDB::bind_method(godot::D_METHOD("rename_calculation", "calculation_id", "name"),
                                &CLifeWorldEditor::rename_calculation);
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
    godot::ClassDB::bind_method(
        godot::D_METHOD("add_calculation_world_rule", "source_key", "calculation_id", "input_bindings",
                        "output_bindings"),
        &CLifeWorldEditor::add_calculation_world_rule);
    godot::ClassDB::bind_method(
        godot::D_METHOD("change_calculation_world_rule", "index", "source_key", "calculation_id",
                        "input_bindings", "output_bindings"),
        &CLifeWorldEditor::change_calculation_world_rule);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_calculation_world_rule", "index"),
                                &CLifeWorldEditor::remove_calculation_world_rule);
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
    godot::ClassDB::bind_method(godot::D_METHOD("set_preview_input", "value_key", "amount"),
                                &CLifeWorldEditor::set_preview_input);
    godot::ClassDB::bind_method(godot::D_METHOD("export_world_snapshot"), &CLifeWorldEditor::export_world_snapshot);
    godot::ClassDB::bind_method(godot::D_METHOD("import_world_snapshot", "snapshot"),
                                &CLifeWorldEditor::import_world_snapshot);
    godot::ClassDB::bind_method(godot::D_METHOD("get_last_error"), &CLifeWorldEditor::get_last_error);
    godot::ClassDB::bind_method(godot::D_METHOD("clear_last_error"), &CLifeWorldEditor::clear_last_error);
}

} // namespace clife::godot_adapter
