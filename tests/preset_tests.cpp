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
    if (!expect_true(preset.definition.values().size() == 6 && preset.definition.templates().size() == 1 &&
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
    if (!expect_near(session.used_energy(), 0.5, "first tick UsedEnergy") ||
        !expect_near(session.energy(), 0.0, "first tick Energy") ||
        !expect_near(session.temperature(), 0.2, "first tick Temperature")) {
        return false;
    }
    session.step();
    return expect_near(session.used_energy(), 0.5, "second tick UsedEnergy") &&
           expect_near(session.temperature(), 0.2, "second tick Temperature");
}

bool test_preset_material_volume_binding()
{
    clife::presets::FirstWorldPreset preset = clife::presets::make_first_world_preset();
    const clife::world::ObjectTemplate& cell = preset.definition.object_template(preset.cell);
    const auto binding = std::ranges::find_if(cell.host_bindings, [&](const clife::world::HostBinding& item) {
        return item.direction == clife::world::HostChannelDirection::output &&
               item.channel == clife::presets::kGeometryVolumeOutputChannel;
    });
    if (!expect_true(std::ranges::none_of(cell.initial_values, [&](const clife::world::InitialValueDefinition& item) {
                         return item.value == preset.organic;
                     }) &&
                         binding != cell.host_bindings.end(),
                     "first-world material volume definitions")) {
        return false;
    }
    const clife::world::CompiledPhenotype phenotype =
        clife::world::compile_phenotype(preset.definition, preset.cell);
    clife::world::RuntimeWorld runtime{preset.definition};
    const clife::world::ObjectId runtime_cell = runtime.instantiate(preset.cell);
    if (!expect_near(phenotype.material_amount(preset.structural_organic), 5.0, "compiled structural material amount") ||
        !expect_near(runtime.value(runtime_cell, preset.organic), 0.0, "runtime Organic is independent from material") ||
        !expect_true(binding->value == preset.organic, "geometry.volume binds Organic ValueKey")) {
        return false;
    }

    preset.definition.rename_value(preset.organic, "Biomass");
    const clife::world::ObjectTemplate& renamed_cell = preset.definition.object_template(preset.cell);
    const auto renamed_binding =
        std::ranges::find_if(renamed_cell.host_bindings, [&](const clife::world::HostBinding& item) {
            return item.channel == clife::presets::kGeometryVolumeOutputChannel;
        });
    const clife::world::CompiledPhenotype renamed_phenotype =
        clife::world::compile_phenotype(preset.definition, preset.cell);
    return expect_true(renamed_binding != renamed_cell.host_bindings.end() && renamed_binding->value == preset.organic,
                       "renaming material value preserves geometry binding identity") &&
           expect_near(renamed_phenotype.material_amount(preset.structural_organic), 5.0,
                       "renaming runtime value preserves material identity");
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
        !expect_near(initial.function(index).calculation_output(preset.storage_calculation,
                                                                preset.storage_organic_size), 1.0,
                     "storage organic size phenotype") ||
        !expect_near(initial.function(index).calculation_output(preset.storage_calculation,
                                                                preset.storage_throughput), 1.5,
                     "storage throughput phenotype") ||
        !expect_near(initial.function(index).calculation_output(preset.storage_calculation,
                                                                preset.storage_leakage), 0.0,
                     "storage leakage phenotype") ||
        !expect_near(initial.material_amount(preset.structural_organic), 5.0, "capacity 5 material total")) {
        return false;
    }
    preset.definition.set_genome_parameter(preset.cell, index, preset.storage_capacity, 10.0);
    preset.definition.rename_parameter(preset.energy_storage, preset.storage_capacity, "RenamedCapacity");
    const clife::world::CompiledPhenotype changed = clife::world::compile_phenotype(preset.definition, preset.cell);
    clife::world::RuntimeWorld changed_runtime{preset.definition};
    const clife::world::ObjectId changed_cell = changed_runtime.instantiate(preset.cell);
    return expect_near(changed.function(index).calculation_output(preset.storage_calculation,
                                                                  preset.storage_organic_size), 2.0,
                       "changed storage capacity recompiles organic size") &&
           expect_near(changed.function(index).calculation_output(preset.storage_calculation,
                                                                  preset.storage_throughput), 3.0,
                       "changed storage capacity recompiles throughput") &&
           expect_near(changed.material_amount(preset.structural_organic), 6.0, "capacity 10 material total") &&
           expect_near(changed_runtime.value(changed_cell, preset.organic), 0.0,
                       "runtime Organic remains independent from material");
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
    return expect_near(first.used_energy(), 0.5, "first session advances") &&
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

    return expect_near(runtime.value(cell, preset.used_energy), 0.5, "edited definition UsedEnergy") &&
           expect_near(runtime.value(cell, preset.temperature), 0.2, "edited definition Temperature") &&
           expect_near(runtime.value(cell, extra), 0.0, "new editable value participates in runtime mapping");
}

bool test_first_world_runtime_storage_and_reset()
{
    const clife::presets::FirstWorldPreset preset = clife::presets::make_first_world_preset();
    clife::world::RuntimeWorld runtime{preset.definition};
    const clife::world::ObjectId cell = runtime.instantiate(preset.cell);
    const clife::world::ObjectId idle_cell = runtime.instantiate(preset.cell);
    runtime.set_input(cell, preset.light, 1.0);
    runtime.step();
    const auto first = runtime.function_states(cell);
    const auto first_storage = std::ranges::find(first, preset.energy_storage,
                                                 &clife::world::RuntimeFunctionState::type);
    const auto idle = runtime.function_states(idle_cell);
    const auto idle_storage = std::ranges::find(idle, preset.energy_storage,
                                                &clife::world::RuntimeFunctionState::type);
    if (!expect_true(first_storage != first.end() && first_storage->buffer.has_value(), "storage runtime state") ||
        !expect_near(first_storage->buffer->stored_amount, 0.5, "tick 1 stored") ||
        !expect_near(first_storage->buffer->received_last_tick, 0.5, "tick 1 received") ||
        !expect_near(first_storage->buffer->supplied_last_tick, 0.0, "tick 1 supplied") ||
        !expect_near(idle_storage->buffer->stored_amount, 0.0, "independent object storage") ||
        !expect_near(runtime.last_end_value(cell, preset.heat), 0.0, "tick 1 END Heat")) {
        return false;
    }
    runtime.set_input(cell, preset.light, 1.0);
    runtime.step();
    const auto second = runtime.function_states(cell);
    const auto second_storage = std::ranges::find(second, preset.energy_storage,
                                                  &clife::world::RuntimeFunctionState::type);
    if (!expect_near(runtime.value(cell, preset.used_energy), 0.5, "tick 2 UsedEnergy") ||
        !expect_near(second_storage->buffer->stored_amount, 1.0, "tick 2 stored") ||
        !expect_near(second_storage->buffer->received_last_tick, 0.5, "tick 2 received") ||
        !expect_near(second_storage->buffer->supplied_last_tick, 0.0, "tick 2 supplied")) {
        return false;
    }
    runtime.set_input(cell, preset.light, 0.0);
    runtime.step();
    const auto third = runtime.function_states(cell);
    const auto third_storage = std::ranges::find(third, preset.energy_storage,
                                                 &clife::world::RuntimeFunctionState::type);
    if (!expect_near(runtime.value(cell, preset.used_energy), 0.5, "tick 3 UsedEnergy") ||
        !expect_near(third_storage->buffer->stored_amount, 0.5, "tick 3 stored") ||
        !expect_near(third_storage->buffer->received_last_tick, 0.0, "tick 3 received") ||
        !expect_near(third_storage->buffer->supplied_last_tick, 0.5, "tick 3 supplied")) {
        return false;
    }
    clife::world::RuntimeWorld reset{preset.definition};
    const clife::world::ObjectId reset_cell = reset.instantiate(preset.cell);
    const auto reset_functions = reset.function_states(reset_cell);
    const auto reset_storage = std::ranges::find(reset_functions, preset.energy_storage,
                                                 &clife::world::RuntimeFunctionState::type);
    return expect_near(reset_storage->buffer->stored_amount, 0.0, "recreated runtime resets storage");
}

} // namespace

int main()
{
    return test_preset_definition_and_ticks() && test_preset_material_volume_binding() &&
                   test_first_world_storage_genotype_and_phenotype() && test_reset_reconstructs_runtime() &&
                   test_sessions_are_independent() && test_names_do_not_change_preset_semantics() &&
                   test_fixed_tick_is_frame_rate_independent() && test_modified_editable_definition_compiles() &&
                   test_first_world_runtime_storage_and_reset()
               ? 0
               : 1;
}
