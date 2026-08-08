#pragma once

#include <clife/core/simulation.hpp>
#include <clife/presets/first_world.hpp>
#include <clife/world/runtime.hpp>

#include <memory>

namespace clife::presets {

class DemoSession final {
public:
    static constexpr double simulation_ticks_per_second{10.0};
    static constexpr double fixed_tick_seconds{1.0 / simulation_ticks_per_second};
    static constexpr Amount default_light{1.0};

    DemoSession();
    explicit DemoSession(FirstWorldPreset preset);
    ~DemoSession();

    DemoSession(const DemoSession&) = delete;
    DemoSession& operator=(const DemoSession&) = delete;
    DemoSession(DemoSession&&) noexcept;
    DemoSession& operator=(DemoSession&&) noexcept;

    void reset();
    void set_running(bool running) noexcept;
    void set_light(Amount amount);
    void advance_time(double elapsed_seconds);
    void step();

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] Tick tick() const noexcept;
    [[nodiscard]] Amount light() const noexcept;
    [[nodiscard]] Amount energy() const;
    [[nodiscard]] Amount used_energy() const;
    [[nodiscard]] Amount temperature() const;
    [[nodiscard]] world::ObjectId cell_object_id() const noexcept;
    [[nodiscard]] const FirstWorldPreset& preset() const noexcept;

private:
    FirstWorldPreset preset_;
    std::unique_ptr<world::RuntimeWorld> runtime_;
    world::ObjectId cell_object_{};
    Amount light_{default_light};
    double accumulator_seconds_{0.0};
    Tick tick_{0};
    bool running_{false};
};

} // namespace clife::presets
