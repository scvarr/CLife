#include "clife_world_editor_internal.hpp"

namespace clife::godot_adapter {

using namespace detail;

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

bool CLifeWorldEditor::set_preview_input(std::int64_t raw_value_key, double amount)
{
    try {
        if (!runtime_ || !preview_object_) {
            throw std::logic_error{"setting a preview input requires an active runtime"};
        }
        runtime_->set_external_input(*preview_object_, value_key(raw_value_key), amount);
        clear_error();
        return true;
    } catch (...) {
        capture_current_error();
        return false;
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

} // namespace clife::godot_adapter
