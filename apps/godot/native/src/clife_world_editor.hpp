#pragma once

#include <clife/world/runtime.hpp>

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
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
    [[nodiscard]] godot::Array get_templates();
    [[nodiscard]] godot::Array get_function_types();
    [[nodiscard]] godot::Array get_initial_values(std::int64_t template_id);
    [[nodiscard]] godot::Array get_material_contributions(std::int64_t template_id);
    [[nodiscard]] godot::Array get_genome(std::int64_t template_id);
    [[nodiscard]] godot::Array get_world_rules();
    [[nodiscard]] godot::Array get_bindings(std::int64_t template_id);
    [[nodiscard]] godot::Array get_host_capabilities();

    [[nodiscard]] std::int64_t add_value(const godot::String& name);
    bool rename_value(std::int64_t key, const godot::String& name);
    bool remove_value(std::int64_t key);
    [[nodiscard]] std::int64_t add_template(const godot::String& name);
    bool rename_template(std::int64_t id, const godot::String& name);
    bool remove_template(std::int64_t id);
    bool set_initial_value(std::int64_t template_id, std::int64_t value_key, double amount);
    bool remove_initial_value(std::int64_t template_id, std::int64_t value_key);
    [[nodiscard]] std::int64_t add_derived_parameter(std::int64_t function_type_id, const godot::String& name,
                                                      const godot::String& expression);
    bool set_derived_parameter_expression(std::int64_t function_type_id, std::int64_t parameter_id,
                                          const godot::String& expression);
    bool set_function_material_contribution(std::int64_t function_type_id, std::int64_t value_key,
                                            const godot::String& expression);
    bool remove_function_material_contribution(std::int64_t function_type_id, std::int64_t value_key);
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
                          std::int64_t value_key);
    bool change_host_binding(std::int64_t template_id, std::int64_t index, const godot::String& channel,
                             std::int64_t direction, std::int64_t value_key);
    bool remove_host_binding(std::int64_t template_id, std::int64_t index);

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
