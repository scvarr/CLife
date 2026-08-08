#include <clife/presets/demo_session.hpp>
#include <clife/presets/first_world.hpp>
#include <clife/world/phenotype.hpp>
#include <clife/world/runtime.hpp>

#include <algorithm>
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
    if (!expect_true(preset.definition.values().size() == 5 && preset.definition.templates().size() == 1 &&
                         preset.definition.function_types().size() == 3 && preset.definition.world_rules().size() == 1,
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

bool test_preset_material_volume_binding()
{
    clife::presets::FirstWorldPreset preset = clife::presets::make_first_world_preset();
    const clife::world::ObjectTemplate& cell = preset.definition.object_template(preset.cell);
    const auto initial =
        std::ranges::find(cell.initial_values, preset.organic, &clife::world::InitialValueDefinition::value);
    const auto binding = std::ranges::find_if(cell.host_bindings, [&](const clife::world::HostBinding& item) {
        return item.direction == clife::world::HostChannelDirection::output &&
               item.channel == clife::presets::kGeometryVolumeOutputChannel;
    });
    if (!expect_true(initial != cell.initial_values.end() && binding != cell.host_bindings.end(),
                     "first-world material volume definitions")) {
        return false;
    }
    if (!expect_near(initial->amount, 10.0, "initial Organic amount") ||
        !expect_true(binding->value == preset.organic, "geometry.volume binds Organic ValueKey")) {
        return false;
    }

    preset.definition.rename_value(preset.organic, "Biomass");
    const clife::world::ObjectTemplate& renamed_cell = preset.definition.object_template(preset.cell);
    const auto renamed_binding =
        std::ranges::find_if(renamed_cell.host_bindings, [&](const clife::world::HostBinding& item) {
            return item.channel == clife::presets::kGeometryVolumeOutputChannel;
        });
    return expect_true(renamed_binding != renamed_cell.host_bindings.end() && renamed_binding->value == preset.organic,
                       "renaming material value preserves geometry binding identity");
}

bool test_first_world_storage_genotype_and_phenotype()
{
    clife::presets::FirstWorldPreset preset = clife::presets::make_first_world_preset();
    const auto& genome = preset.definition.object_template(preset.cell).genome;
    const auto storage = std::ranges::find(genome, preset.energy_storage, &clife::world::GenomeFunctionInstance::type);
    if (!expect_true(storage != genome.end() && storage->parameters.size() == 1,
                     "Energy Storage genome has one independent parameter")) {
        return false;
    }
    const std::size_t index = static_cast<std::size_t>(std::distance(genome.begin(), storage));
    const clife::world::CompiledPhenotype initial = clife::world::compile_phenotype(preset.definition, preset.cell);
    if (!expect_near(initial.function(index).parameter(preset.storage_capacity), 5.0, "storage capacity genotype") ||
        !expect_near(initial.function(index).parameter(preset.storage_organic_size), 1.0,
                     "storage organic size phenotype")) {
        return false;
    }
    preset.definition.set_genome_parameter(preset.cell, index, preset.storage_capacity, 10.0);
    const clife::world::CompiledPhenotype changed = clife::world::compile_phenotype(preset.definition, preset.cell);
    return expect_near(changed.function(index).parameter(preset.storage_organic_size), 2.0,
                       "changed storage capacity recompiles organic size");
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

bool test_modified_editable_definition_compiles()
{
    clife::presets::FirstWorldPreset preset = clife::presets::make_first_world_preset();
    const clife::world::ValueKey extra = preset.definition.add_value("Extra");
    preset.definition.rename_value(preset.light, "Solar flux");

    clife::world::RuntimeWorld runtime{preset.definition};
    const clife::world::ObjectId cell = runtime.instantiate(preset.cell);
    runtime.set_input(cell, preset.light, 1.0);
    runtime.step();

    return expect_near(runtime.value(cell, preset.used_energy), 0.25, "edited definition UsedEnergy") &&
           expect_near(runtime.value(cell, preset.temperature), 0.275, "edited definition Temperature") &&
           expect_near(runtime.value(cell, extra), 0.0, "new editable value participates in runtime mapping");
}

} // namespace

int main()
{
    return test_preset_definition_and_ticks() && test_preset_material_volume_binding() &&
                   test_first_world_storage_genotype_and_phenotype() && test_reset_reconstructs_runtime() &&
                   test_sessions_are_independent() && test_names_do_not_change_preset_semantics() &&
                   test_fixed_tick_is_frame_rate_independent() && test_modified_editable_definition_compiles()
               ? 0
               : 1;
}
