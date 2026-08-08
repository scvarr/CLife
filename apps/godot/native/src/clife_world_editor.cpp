#include "clife_world_editor.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <array>
#include <cmath>
#include <exception>
#include <limits>
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
    std::string_view display_name;
};

constexpr std::array kHostCapabilities{
    HostCapability{
        .channel = "world.light",
        .direction = world::HostChannelDirection::input,
        .display_name = "World Light",
    },
    HostCapability{
        .channel = "geometry.volume",
        .direction = world::HostChannelDirection::output,
        .display_name = "Cell Volume",
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

world::TemplateId template_id(std::int64_t raw)
{
    if (raw <= 0 || raw > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument{"invalid TemplateId"};
    }
    return {static_cast<std::uint32_t>(raw)};
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

} // namespace

CLifeWorldEditor::CLifeWorldEditor()
{
    presets::FirstWorldPreset preset = presets::make_first_world_preset();
    selected_template_ = preset.cell;
    definition_ = std::move(preset.definition);
    host_inputs_.emplace(std::string{presets::kLightInputChannel}, 1.0);
    ensure_host_inputs(definition_, *selected_template_);
}

CLifeWorldEditor::~CLifeWorldEditor() = default;

godot::Array CLifeWorldEditor::get_values()
{
    godot::Array result;
    try {
        for (const world::ValueDefinition& value : definition_.values()) {
            godot::Dictionary item;
            item["key"] = static_cast<std::int64_t>(value.key.value);
            item["name"] = to_godot_string(value.name);
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

godot::Array CLifeWorldEditor::get_genome(std::int64_t raw_template_id)
{
    godot::Array result;
    try {
        const world::ObjectTemplate& object = definition_.object_template(template_id(raw_template_id));
        for (std::size_t index = 0; index < object.genome.size(); ++index) {
            const world::GenomeFunctionDefinition& function = object.genome[index];
            godot::Dictionary item;
            item["index"] = static_cast<std::int64_t>(index);
            item["input_key"] = static_cast<std::int64_t>(function.input.value);
            item["output_key"] = static_cast<std::int64_t>(function.output.value);
            item["throughput"] = function.throughput;
            item["result_per_input"] = function.result_per_input;
            result.push_back(item);
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
            item["direction_id"] = binding.direction == world::HostChannelDirection::input ? kInputDirection
                                                                                            : kOutputDirection;
            item["value_key"] = static_cast<std::int64_t>(binding.value.value);
            result.push_back(item);
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
            item["direction_id"] = capability.direction == world::HostChannelDirection::input ? kInputDirection
                                                                                               : kOutputDirection;
            item["display_name"] = to_godot_string(capability.display_name);
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
            selected_template_ = definition_.templates().empty()
                                     ? std::optional<world::TemplateId>{}
                                     : std::optional<world::TemplateId>{definition_.templates().front().id};
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

bool CLifeWorldEditor::add_genome_function(std::int64_t raw_template_id, std::int64_t input_key,
                                           std::int64_t output_key, double throughput, double result_per_input)
{
    return edit([&] {
        (void)definition_.add_genome_function(template_id(raw_template_id), {
                                                                                 .input = value_key(input_key),
                                                                                 .output = value_key(output_key),
                                                                                 .throughput = throughput,
                                                                                 .result_per_input = result_per_input,
                                                                             });
    });
}

bool CLifeWorldEditor::change_genome_function(std::int64_t raw_template_id, std::int64_t index,
                                              std::int64_t input_key, std::int64_t output_key, double throughput,
                                              double result_per_input)
{
    return edit([&] {
        definition_.change_genome_function(template_id(raw_template_id), item_index(index), {
                                                                                                 .input = value_key(input_key),
                                                                                                 .output = value_key(output_key),
                                                                                                 .throughput = throughput,
                                                                                                 .result_per_input = result_per_input,
                                                                                             });
    });
}

bool CLifeWorldEditor::remove_genome_function(std::int64_t raw_template_id, std::int64_t index)
{
    return edit([&] { definition_.remove_genome_function(template_id(raw_template_id), item_index(index)); });
}

bool CLifeWorldEditor::add_world_rule(std::int64_t source_key, std::int64_t target_key, double target_per_source)
{
    return edit([&] {
        (void)definition_.add_world_rule({
            .source = value_key(source_key),
            .target = value_key(target_key),
            .target_per_source = target_per_source,
        });
    });
}

bool CLifeWorldEditor::change_world_rule(std::int64_t index, std::int64_t source_key, std::int64_t target_key,
                                         double target_per_source)
{
    return edit([&] {
        definition_.change_world_rule(item_index(index), {
                                                               .source = value_key(source_key),
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
                                        std::int64_t direction, std::int64_t raw_value_key)
{
    return edit([&] {
        const world::TemplateId id = template_id(raw_template_id);
        (void)definition_.add_host_binding(id, {
                                                  .channel = to_std_string(channel),
                                                  .direction = host_direction(direction),
                                                  .value = value_key(raw_value_key),
                                              });
        ensure_host_inputs(definition_, id);
    });
}

bool CLifeWorldEditor::change_host_binding(std::int64_t raw_template_id, std::int64_t index,
                                           const godot::String& channel, std::int64_t direction,
                                           std::int64_t raw_value_key)
{
    return edit([&] {
        const world::TemplateId id = template_id(raw_template_id);
        definition_.change_host_binding(id, item_index(index), {
                                                                   .channel = to_std_string(channel),
                                                                   .direction = host_direction(direction),
                                                                   .value = value_key(raw_value_key),
                                                               });
        ensure_host_inputs(definition_, id);
    });
}

bool CLifeWorldEditor::remove_host_binding(std::int64_t raw_template_id, std::int64_t index)
{
    return edit([&] { definition_.remove_host_binding(template_id(raw_template_id), item_index(index)); });
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
    godot::ClassDB::bind_method(godot::D_METHOD("get_templates"), &CLifeWorldEditor::get_templates);
    godot::ClassDB::bind_method(godot::D_METHOD("get_initial_values", "template_id"), &CLifeWorldEditor::get_initial_values);
    godot::ClassDB::bind_method(godot::D_METHOD("get_genome", "template_id"), &CLifeWorldEditor::get_genome);
    godot::ClassDB::bind_method(godot::D_METHOD("get_world_rules"), &CLifeWorldEditor::get_world_rules);
    godot::ClassDB::bind_method(godot::D_METHOD("get_bindings", "template_id"), &CLifeWorldEditor::get_bindings);
    godot::ClassDB::bind_method(godot::D_METHOD("get_host_capabilities"),
                                &CLifeWorldEditor::get_host_capabilities);
    godot::ClassDB::bind_method(godot::D_METHOD("add_value", "name"), &CLifeWorldEditor::add_value);
    godot::ClassDB::bind_method(godot::D_METHOD("rename_value", "key", "name"), &CLifeWorldEditor::rename_value);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_value", "key"), &CLifeWorldEditor::remove_value);
    godot::ClassDB::bind_method(godot::D_METHOD("add_template", "name"), &CLifeWorldEditor::add_template);
    godot::ClassDB::bind_method(godot::D_METHOD("rename_template", "id", "name"), &CLifeWorldEditor::rename_template);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_template", "id"), &CLifeWorldEditor::remove_template);
    godot::ClassDB::bind_method(godot::D_METHOD("set_initial_value", "template_id", "value_key", "amount"),
                                &CLifeWorldEditor::set_initial_value);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_initial_value", "template_id", "value_key"),
                                &CLifeWorldEditor::remove_initial_value);
    godot::ClassDB::bind_method(godot::D_METHOD("add_genome_function", "template_id", "input_key", "output_key",
                                                "throughput", "result_per_input"),
                                &CLifeWorldEditor::add_genome_function);
    godot::ClassDB::bind_method(godot::D_METHOD("change_genome_function", "template_id", "index", "input_key",
                                                "output_key", "throughput", "result_per_input"),
                                &CLifeWorldEditor::change_genome_function);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_genome_function", "template_id", "index"),
                                &CLifeWorldEditor::remove_genome_function);
    godot::ClassDB::bind_method(godot::D_METHOD("add_world_rule", "source_key", "target_key", "target_per_source"),
                                &CLifeWorldEditor::add_world_rule);
    godot::ClassDB::bind_method(godot::D_METHOD("change_world_rule", "index", "source_key", "target_key",
                                                "target_per_source"),
                                &CLifeWorldEditor::change_world_rule);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_world_rule", "index"), &CLifeWorldEditor::remove_world_rule);
    godot::ClassDB::bind_method(godot::D_METHOD("add_host_binding", "template_id", "channel", "direction",
                                                "value_key"),
                                &CLifeWorldEditor::add_host_binding);
    godot::ClassDB::bind_method(godot::D_METHOD("change_host_binding", "template_id", "index", "channel",
                                                "direction", "value_key"),
                                &CLifeWorldEditor::change_host_binding);
    godot::ClassDB::bind_method(godot::D_METHOD("remove_host_binding", "template_id", "index"),
                                &CLifeWorldEditor::remove_host_binding);
    godot::ClassDB::bind_method(godot::D_METHOD("select_template", "template_id"), &CLifeWorldEditor::select_template);
    godot::ClassDB::bind_method(godot::D_METHOD("get_selected_template_id"), &CLifeWorldEditor::get_selected_template_id);
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
    godot::ClassDB::bind_method(godot::D_METHOD("get_host_inputs"), &CLifeWorldEditor::get_host_inputs);
    godot::ClassDB::bind_method(godot::D_METHOD("get_host_outputs"), &CLifeWorldEditor::get_host_outputs);
    godot::ClassDB::bind_method(godot::D_METHOD("set_host_input", "channel", "amount"),
                                &CLifeWorldEditor::set_host_input);
    godot::ClassDB::bind_method(godot::D_METHOD("get_last_error"), &CLifeWorldEditor::get_last_error);
    godot::ClassDB::bind_method(godot::D_METHOD("clear_last_error"), &CLifeWorldEditor::clear_last_error);
}

} // namespace clife::godot_adapter
