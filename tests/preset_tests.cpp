#include <clife/presets/demo_session.hpp>
#include <clife/presets/first_world.hpp>
#include <clife/world/runtime.hpp>

#include <cmath>
#include <iostream>
#include <utility>

namespace {

bool expect_true(bool condition, const char* message)
{
    if (condition) {
        return true;
    }
    std::cerr << message << ": expected true\n";
    return false;
}

bool expect_near(clife::Amount actual, clife::Amount expected, const char* message)
{
    constexpr clife::Amount tolerance{1e-12};
    if (std::abs(actual - expected) <= tolerance) {
        return true;
    }
    std::cerr << message << ": expected " << expected << ", got " << actual << '\n';
    return false;
}

bool test_preset_definition_and_ticks()
{
    const clife::presets::FirstWorldPreset preset = clife::presets::make_first_world_preset();
    if (!expect_true(preset.definition.values().size() == 4 && preset.definition.templates().size() == 1 &&
                         preset.definition.world_rules().size() == 1,
                     "first-world preset shape")) {
        return false;
    }

    clife::presets::DemoSession session;
    if (!expect_near(session.temperature(), 0.2, "initial Temperature") ||
        !expect_true(session.tick() == 0, "initial tick")) {
        return false;
    }
    session.step();
    if (!expect_near(session.used_energy(), 0.25, "first tick UsedEnergy") ||
        !expect_near(session.energy(), 0.0, "first tick Energy") ||
        !expect_near(session.temperature(), 0.275, "first tick Temperature")) {
        return false;
    }
    session.step();
    return expect_near(session.temperature(), 0.35, "second tick Temperature");
}

bool test_reset_reconstructs_runtime()
{
    clife::presets::DemoSession session;
    session.step();
    session.step();
    session.set_light(0.5);
    session.set_running(true);
    session.reset();
    return expect_true(session.tick() == 0 && !session.running(), "reset lifecycle") &&
           expect_near(session.light(), clife::presets::DemoSession::default_light, "reset Light") &&
           expect_near(session.temperature(), 0.2, "reset Temperature");
}

bool test_sessions_are_independent()
{
    clife::presets::DemoSession first;
    clife::presets::DemoSession second;
    first.step();
    return expect_near(first.temperature(), 0.275, "first session advances") &&
           expect_near(second.temperature(), 0.2, "second session stays initial");
}

bool test_names_do_not_change_preset_semantics()
{
    clife::presets::FirstWorldPreset original = clife::presets::make_first_world_preset();
    clife::presets::FirstWorldPreset renamed = clife::presets::make_first_world_preset();
    renamed.definition.rename_value(renamed.light, "External photons");
    renamed.definition.rename_value(renamed.temperature, "Thermal state");
    renamed.definition.rename_template(renamed.cell, "Renamed cell");

    clife::presets::DemoSession first{std::move(original)};
    clife::presets::DemoSession second{std::move(renamed)};
    first.step();
    second.step();
    return expect_near(first.used_energy(), second.used_energy(), "renamed UsedEnergy semantics") &&
           expect_near(first.temperature(), second.temperature(), "renamed Temperature semantics");
}

bool test_fixed_tick_is_frame_rate_independent()
{
    clife::presets::DemoSession coarse;
    clife::presets::DemoSession fine;
    coarse.set_running(true);
    fine.set_running(true);
    coarse.advance_time(0.2);
    for (int frame = 0; frame < 20; ++frame) {
        fine.advance_time(0.01);
    }
    return expect_true(coarse.tick() == 2 && fine.tick() == 2, "fixed tick count ignores frame partition") &&
           expect_near(coarse.temperature(), fine.temperature(), "fixed tick values ignore frame partition");
}

} // namespace

int main()
{
    return test_preset_definition_and_ticks() && test_reset_reconstructs_runtime() &&
                   test_sessions_are_independent() && test_names_do_not_change_preset_semantics() &&
                   test_fixed_tick_is_frame_rate_independent()
               ? 0
               : 1;
}
