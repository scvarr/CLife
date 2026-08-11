#pragma once

#include <clife/world/runtime.hpp>

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_float64_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace clife::godot_adapter {

class CLifeWorldEditor final : public godot::RefCounted {
    GDCLASS(CLifeWorldEditor, godot::RefCounted)

public:
    CLifeWorldEditor();
    ~CLifeWorldEditor() override;

    [[nodiscard]] godot::Array get_values();
    [[nodiscard]] godot::Array get_units();
    [[nodiscard]] godot::Array get_unit_conversions();
    [[nodiscard]] godot::Array get_object_characteristics();
    [[nodiscard]] godot::Dictionary get_object_construction();
    [[nodiscard]] godot::Array get_templates();
    [[nodiscard]] godot::Array get_function_types();
    [[nodiscard]] godot::Array get_calculations();
    [[nodiscard]] godot::Array get_initial_values(std::int64_t template_id);
    [[nodiscard]] godot::Array get_material_contributions(std::int64_t template_id);
    [[nodiscard]] godot::Array get_genome(std::int64_t template_id);
    [[nodiscard]] godot::Array get_world_rules();
    [[nodiscard]] godot::Array get_bindings(std::int64_t template_id);
    [[nodiscard]] godot::Dictionary get_template_characteristic_preview(std::int64_t template_id);
    [[nodiscard]] godot::PackedFloat64Array sample_template_shape(std::int64_t template_id,
                                                                   const godot::PackedVector3Array& directions);
    [[nodiscard]] godot::Array get_host_capabilities();

    [[nodiscard]] std::int64_t add_value(const godot::String& name);
    [[nodiscard]] std::int64_t add_unit(const godot::String& symbol, const godot::String& description = godot::String{});
    bool update_unit(std::int64_t unit_id, const godot::String& symbol, const godot::String& description);
    bool remove_unit(std::int64_t unit_id);
    [[nodiscard]] std::int64_t add_unit_conversion(std::int64_t source_unit_id, double source_amount,
                                                    std::int64_t target_unit_id, double target_amount);
    bool remove_unit_conversion(std::int64_t conversion_id);
    bool set_value_unit(std::int64_t value_key, std::int64_t unit_id);
    bool clear_value_unit(std::int64_t value_key);
    [[nodiscard]] std::int64_t add_object_characteristic(const godot::String& name);
    bool rename_object_characteristic(std::int64_t characteristic_id, const godot::String& name);
    bool remove_object_characteristic(std::int64_t characteristic_id);
    bool set_function_process(std::int64_t function_type_id, std::int64_t input_value_key,
                              const godot::Dictionary& throughput_source, std::int64_t conversion_id,
                              std::int64_t output_value_key, const godot::Dictionary& allocation_source);
    bool set_function_process_full(std::int64_t function_type_id, std::int64_t input_value_key,
                                   const godot::Dictionary& throughput_source, std::int64_t conversion_id,
                                   const godot::Array& outputs);
    bool change_function_process_settings(std::int64_t function_type_id, std::int64_t input_value_key,
                                          const godot::Dictionary& throughput_source, std::int64_t conversion_id);
    bool change_function_process_output(std::int64_t function_type_id, std::int64_t existing_output_value_key,
                                        std::int64_t output_value_key, const godot::Dictionary& allocation_source);
    bool add_function_process_output(std::int64_t function_type_id, std::int64_t output_value_key,
                                     const godot::Dictionary& allocation_source);
    bool remove_function_process_output(std::int64_t function_type_id, std::int64_t output_value_key);
    bool remove_function_process(std::int64_t function_type_id);
    bool set_buffer_process(std::int64_t function_type_id, std::int64_t value_key,
                            const godot::Dictionary& capacity_source, const godot::Dictionary& throughput_source,
                            const godot::Dictionary& leakage_source);
    bool remove_buffer_process(std::int64_t function_type_id);
    bool rename_value(std::int64_t key, const godot::String& name);
    bool remove_value(std::int64_t key);
    [[nodiscard]] std::int64_t add_template(const godot::String& name);
    [[nodiscard]] std::int64_t add_function_type(const godot::String& name);
    bool rename_function_type(std::int64_t function_type_id, const godot::String& name);
    bool remove_function_type(std::int64_t function_type_id);
    [[nodiscard]] std::int64_t add_genome_parameter(std::int64_t function_type_id, const godot::String& name,
                                                     double default_value);
    bool update_genome_parameter(std::int64_t function_type_id, std::int64_t parameter_id, const godot::String& name,
                                 double default_value);
    bool remove_genome_parameter(std::int64_t function_type_id, std::int64_t parameter_id);
    [[nodiscard]] std::int64_t add_calculation(const godot::String& name);
    [[nodiscard]] std::int64_t add_calculation_input(std::int64_t calculation_id, const godot::String& name);
    [[nodiscard]] std::int64_t add_calculation_output(std::int64_t calculation_id, const godot::String& name,
                                                       const godot::String& expression);
    bool remove_calculation(std::int64_t calculation_id);
    bool remove_calculation_input(std::int64_t calculation_id, std::int64_t input_port_id);
    bool remove_calculation_output(std::int64_t calculation_id, std::int64_t output_port_id);
    bool set_calculation_output_expression(std::int64_t calculation_id, std::int64_t output_port_id,
                                           const godot::String& expression);
    [[nodiscard]] godot::Array evaluate_calculation(std::int64_t calculation_id, const godot::Array& inputs);
    bool rename_template(std::int64_t id, const godot::String& name);
    bool remove_template(std::int64_t id);
    bool set_initial_value(std::int64_t template_id, std::int64_t value_key, double amount);
    bool remove_initial_value(std::int64_t template_id, std::int64_t value_key);
    bool set_template_base_characteristic(std::int64_t template_id, std::int64_t characteristic_id, double amount);
    bool remove_template_base_characteristic(std::int64_t template_id, std::int64_t characteristic_id);
    bool set_function_calculation_binding(std::int64_t function_type_id, std::int64_t calculation_id,
                                          const godot::Array& input_bindings);
    bool remove_function_calculation_binding(std::int64_t function_type_id, std::int64_t calculation_id);
    bool set_function_material_contribution(std::int64_t function_type_id, std::int64_t value_key,
                                            const godot::Dictionary& amount_source);
    bool remove_function_material_contribution(std::int64_t function_type_id, std::int64_t value_key);
    bool set_function_characteristic_contribution(std::int64_t function_type_id, std::int64_t characteristic_id,
                                                  const godot::Dictionary& amount_source);
    bool remove_function_characteristic_contribution(std::int64_t function_type_id, std::int64_t characteristic_id);
    bool add_genome_function(std::int64_t template_id, std::int64_t function_type_id);
    bool set_genome_parameter(std::int64_t template_id, std::int64_t index, std::int64_t parameter_id, double value);
    bool remove_genome_function(std::int64_t template_id, std::int64_t index);
    bool add_world_rule(std::int64_t source_key, std::int64_t end_buffer_key, std::int64_t target_key,
                        double target_per_source);
    bool change_world_rule(std::int64_t index, std::int64_t source_key, std::int64_t end_buffer_key,
                           std::int64_t target_key,
                           double target_per_source);
    bool remove_world_rule(std::int64_t index);
    bool add_host_binding(std::int64_t template_id, const godot::String& channel, std::int64_t direction,
                          const godot::Dictionary& source);
    bool change_host_binding(std::int64_t template_id, std::int64_t index, const godot::String& channel,
                             std::int64_t direction, const godot::Dictionary& source);
    bool remove_host_binding(std::int64_t template_id, std::int64_t index);
    bool set_object_construction(std::int64_t calculation_id, const godot::Array& inputs, const godot::Array& outputs);
    bool remove_object_construction();

    bool select_template(std::int64_t template_id);
    [[nodiscard]] std::int64_t get_selected_template_id() const;
    bool run();
    void stop();
    bool reset_runtime();
    bool play();
    void pause();
    bool step_once();
    void advance_time(double elapsed_seconds);
    [[nodiscard]] bool is_run_active() const noexcept;
    [[nodiscard]] bool is_playing() const noexcept;
    [[nodiscard]] std::int64_t get_tick() const noexcept;
    [[nodiscard]] double get_fixed_tick_seconds() const noexcept;
    [[nodiscard]] std::int64_t get_preview_object_id() const noexcept;
    [[nodiscard]] godot::Array get_runtime_values();
    [[nodiscard]] godot::Array get_runtime_functions();
    [[nodiscard]] godot::Array get_last_end_buffer();
    [[nodiscard]] godot::Array get_host_inputs();
    [[nodiscard]] godot::Array get_host_outputs();
    bool set_host_input(const godot::String& channel, double amount);
    bool set_preview_input(std::int64_t value_key, double amount);
    [[nodiscard]] godot::Dictionary export_world_snapshot();
    bool import_world_snapshot(const godot::Dictionary& snapshot);
    [[nodiscard]] godot::String get_last_error() const;
    void clear_last_error();

protected:
    static void _bind_methods();

private:
    template <typename Operation> bool edit(Operation&& operation)
    {
        try {
            require_edit_mode();
            operation();
            clear_error();
            return true;
        } catch (...) {
            capture_current_error();
            return false;
        }
    }

    void require_edit_mode() const;
    void capture_current_error() noexcept;
    void clear_error() noexcept;
    void rebuild_runtime_from_snapshot();
    void stage_inputs_and_step();
    void ensure_host_inputs(const world::WorldDefinition& definition, world::TemplateId template_id);

    world::WorldDefinition definition_;
    std::optional<world::TemplateId> selected_template_;
    std::optional<world::WorldDefinition> run_definition_;
    std::optional<world::TemplateId> run_template_;
    std::unique_ptr<world::RuntimeWorld> runtime_;
    std::optional<world::ObjectId> preview_object_;
    std::map<std::string, Amount> host_inputs_;
    double accumulator_{};
    std::uint64_t tick_{};
    bool playing_{};
    std::string last_error_;
};

} // namespace clife::godot_adapter
