#include "clife_demo_runtime.hpp"

#include <godot_cpp/core/class_db.hpp>

#include <cstdint>
#include <string_view>

namespace clife::godot_adapter {
namespace {

godot::String to_godot_string(std::string_view value)
{
    return godot::String::utf8(value.data(), static_cast<std::int64_t>(value.size()));
}

} // namespace

CLifeDemoRuntime::CLifeDemoRuntime() : session_(std::make_unique<presets::DemoSession>()) {}
CLifeDemoRuntime::~CLifeDemoRuntime() = default;

void CLifeDemoRuntime::reset() { session_->reset(); }
void CLifeDemoRuntime::set_running(bool running) { session_->set_running(running); }
bool CLifeDemoRuntime::is_running() const { return session_->running(); }
void CLifeDemoRuntime::set_light(double light) { session_->set_light(light); }
void CLifeDemoRuntime::advance_time(double elapsed_seconds) { session_->advance_time(elapsed_seconds); }
void CLifeDemoRuntime::step_once() { session_->step(); }

std::int64_t CLifeDemoRuntime::get_tick() const { return static_cast<std::int64_t>(session_->tick()); }
std::int64_t CLifeDemoRuntime::get_cell_object_id() const
{
    return static_cast<std::int64_t>(session_->cell_object_id().value);
}
double CLifeDemoRuntime::get_light() const { return session_->light(); }
double CLifeDemoRuntime::get_energy() const { return session_->energy(); }
double CLifeDemoRuntime::get_used_energy() const { return session_->used_energy(); }
double CLifeDemoRuntime::get_temperature() const { return session_->temperature(); }
double CLifeDemoRuntime::get_fixed_tick_seconds() const { return presets::DemoSession::fixed_tick_seconds; }
godot::String CLifeDemoRuntime::get_values_summary() const { return to_godot_string(presets::kValuesSummary); }
godot::String CLifeDemoRuntime::get_genome_summary() const { return to_godot_string(presets::kGenomeSummary); }
godot::String CLifeDemoRuntime::get_world_rule_summary() const { return to_godot_string(presets::kWorldRuleSummary); }
godot::String CLifeDemoRuntime::get_binding_summary() const { return to_godot_string(presets::kBindingSummary); }

void CLifeDemoRuntime::_bind_methods()
{
    godot::ClassDB::bind_method(godot::D_METHOD("reset"), &CLifeDemoRuntime::reset);
    godot::ClassDB::bind_method(godot::D_METHOD("set_running", "running"), &CLifeDemoRuntime::set_running);
    godot::ClassDB::bind_method(godot::D_METHOD("is_running"), &CLifeDemoRuntime::is_running);
    godot::ClassDB::bind_method(godot::D_METHOD("set_light", "light"), &CLifeDemoRuntime::set_light);
    godot::ClassDB::bind_method(godot::D_METHOD("advance_time", "elapsed_seconds"), &CLifeDemoRuntime::advance_time);
    godot::ClassDB::bind_method(godot::D_METHOD("step_once"), &CLifeDemoRuntime::step_once);
    godot::ClassDB::bind_method(godot::D_METHOD("get_tick"), &CLifeDemoRuntime::get_tick);
    godot::ClassDB::bind_method(godot::D_METHOD("get_cell_object_id"), &CLifeDemoRuntime::get_cell_object_id);
    godot::ClassDB::bind_method(godot::D_METHOD("get_light"), &CLifeDemoRuntime::get_light);
    godot::ClassDB::bind_method(godot::D_METHOD("get_energy"), &CLifeDemoRuntime::get_energy);
    godot::ClassDB::bind_method(godot::D_METHOD("get_used_energy"), &CLifeDemoRuntime::get_used_energy);
    godot::ClassDB::bind_method(godot::D_METHOD("get_temperature"), &CLifeDemoRuntime::get_temperature);
    godot::ClassDB::bind_method(godot::D_METHOD("get_fixed_tick_seconds"), &CLifeDemoRuntime::get_fixed_tick_seconds);
    godot::ClassDB::bind_method(godot::D_METHOD("get_values_summary"), &CLifeDemoRuntime::get_values_summary);
    godot::ClassDB::bind_method(godot::D_METHOD("get_genome_summary"), &CLifeDemoRuntime::get_genome_summary);
    godot::ClassDB::bind_method(godot::D_METHOD("get_world_rule_summary"), &CLifeDemoRuntime::get_world_rule_summary);
    godot::ClassDB::bind_method(godot::D_METHOD("get_binding_summary"), &CLifeDemoRuntime::get_binding_summary);
}

} // namespace clife::godot_adapter
