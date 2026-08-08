#include <clife/presets/demo_session.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace clife::presets {

DemoSession::DemoSession() : DemoSession{make_first_world_preset()} {}

DemoSession::DemoSession(FirstWorldPreset preset) : preset_(std::move(preset))
{
    reset();
}

DemoSession::~DemoSession() = default;
DemoSession::DemoSession(DemoSession&&) noexcept = default;
DemoSession& DemoSession::operator=(DemoSession&&) noexcept = default;

void DemoSession::reset()
{
    runtime_ = std::make_unique<world::RuntimeWorld>(preset_.definition);
    cell_object_ = runtime_->instantiate(preset_.cell);
    light_ = default_light;
    accumulator_seconds_ = 0.0;
    tick_ = 0;
    running_ = false;
}

void DemoSession::set_running(bool running) noexcept { running_ = running; }

void DemoSession::set_light(Amount amount)
{
    if (!std::isfinite(amount) || amount < 0.0) {
        throw std::invalid_argument{"demo light must be finite and non-negative"};
    }
    light_ = amount;
}

void DemoSession::advance_time(double elapsed_seconds)
{
    if (!std::isfinite(elapsed_seconds) || elapsed_seconds < 0.0) {
        throw std::invalid_argument{"elapsed time must be finite and non-negative"};
    }
    if (!running_) {
        return;
    }

    accumulator_seconds_ += elapsed_seconds;
    constexpr double comparison_epsilon{1e-12};
    while (accumulator_seconds_ + comparison_epsilon >= fixed_tick_seconds) {
        step();
        accumulator_seconds_ = std::max(0.0, accumulator_seconds_ - fixed_tick_seconds);
    }
}

void DemoSession::step()
{
    runtime_->set_input(cell_object_, kLightInputChannel, light_);
    runtime_->step();
    ++tick_;
}

bool DemoSession::running() const noexcept { return running_; }
Tick DemoSession::tick() const noexcept { return tick_; }
Amount DemoSession::light() const noexcept { return light_; }
Amount DemoSession::energy() const { return runtime_->value(cell_object_, preset_.energy); }
Amount DemoSession::used_energy() const { return runtime_->output(cell_object_, kUsedEnergyOutputChannel); }
Amount DemoSession::temperature() const { return runtime_->output(cell_object_, kTemperatureOutputChannel); }
world::ObjectId DemoSession::cell_object_id() const noexcept { return cell_object_; }
const FirstWorldPreset& DemoSession::preset() const noexcept { return preset_; }

} // namespace clife::presets
