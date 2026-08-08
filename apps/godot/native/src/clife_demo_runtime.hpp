#pragma once

#include <clife/presets/demo_session.hpp>

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>

#include <memory>

namespace clife::godot_adapter {

class CLifeDemoRuntime final : public godot::RefCounted {
    GDCLASS(CLifeDemoRuntime, godot::RefCounted)

public:
    CLifeDemoRuntime();
    ~CLifeDemoRuntime() override;

    void reset();
    void set_running(bool running);
    [[nodiscard]] bool is_running() const;
    void set_light(double light);
    void advance_time(double elapsed_seconds);
    void step_once();

    [[nodiscard]] std::int64_t get_tick() const;
    [[nodiscard]] std::int64_t get_cell_object_id() const;
    [[nodiscard]] double get_light() const;
    [[nodiscard]] double get_energy() const;
    [[nodiscard]] double get_used_energy() const;
    [[nodiscard]] double get_temperature() const;
    [[nodiscard]] double get_fixed_tick_seconds() const;
    [[nodiscard]] godot::String get_values_summary() const;
    [[nodiscard]] godot::String get_genome_summary() const;
    [[nodiscard]] godot::String get_world_rule_summary() const;
    [[nodiscard]] godot::String get_binding_summary() const;

protected:
    static void _bind_methods();

private:
    std::unique_ptr<presets::DemoSession> session_;
};

} // namespace clife::godot_adapter
