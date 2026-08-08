#include <clife/world/definition.hpp>
#include <clife/world/runtime.hpp>

#include <array>
#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {

using clife::world::GenomeFunctionDefinition;
using clife::world::HostBinding;
using clife::world::HostChannelDirection;
using clife::world::ObjectId;
using clife::world::RuntimeWorld;
using clife::world::TemplateId;
using clife::world::ValueKey;
using clife::world::WorldDefinition;
using clife::world::WorldRuleDefinition;

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

template <typename Exception = std::invalid_argument, typename Function>
bool expect_throws(Function&& function, const char* message)
{
    try {
        std::forward<Function>(function)();
    } catch (const Exception&) {
        return true;
    } catch (const std::exception& error) {
        std::cerr << message << ": wrong exception: " << error.what() << '\n';
        return false;
    }
    std::cerr << message << ": expected exception\n";
    return false;
}

struct CellWorld final {
    WorldDefinition definition;
    ValueKey light;
    ValueKey energy;
    ValueKey used_energy;
    ValueKey temperature;
    TemplateId cell;
};

[[nodiscard]] CellWorld make_cell_world()
{
    WorldDefinition definition;
    const ValueKey light = definition.add_value("Light");
    const ValueKey energy = definition.add_value("Energy");
    const ValueKey used_energy = definition.add_value("UsedEnergy");
    const ValueKey temperature = definition.add_value("Temperature");
    const TemplateId cell = definition.add_template("Cell");

    (void)definition.add_genome_function(cell, {
                                                           .input = light,
                                                           .output = energy,
                                                           .throughput = 1.0,
                                                           .result_per_input = 1.0,
                                                       });
    (void)definition.add_genome_function(cell, {
                                                           .input = energy,
                                                           .output = used_energy,
                                                           .throughput = 0.25,
                                                           .result_per_input = 1.0,
                                                       });
    (void)definition.add_world_rule({.source = energy, .target = temperature, .target_per_source = 0.1});
    definition.set_initial_value(cell, temperature, 0.2);
    (void)definition.add_host_binding(cell, {
                                                    .channel = "world.light",
                                                    .direction = HostChannelDirection::input,
                                                    .value = light,
                                                });
    (void)definition.add_host_binding(cell, {
                                                    .channel = "cell.used_energy",
                                                    .direction = HostChannelDirection::output,
                                                    .value = used_energy,
                                                });
    (void)definition.add_host_binding(cell, {
                                                    .channel = "cell.temperature",
                                                    .direction = HostChannelDirection::output,
                                                    .value = temperature,
                                                });
    return {
        .definition = std::move(definition),
        .light = light,
        .energy = energy,
        .used_energy = used_energy,
        .temperature = temperature,
        .cell = cell,
    };
}

bool test_stable_keys_survive_reorder_and_deletion()
{
    WorldDefinition definition;
    const ValueKey first = definition.add_value("First");
    const ValueKey second = definition.add_value("Second");
    const ValueKey third = definition.add_value("Third");
    definition.reorder_values(std::array{third, first, second});
    if (!expect_true(definition.values()[0].key == third && definition.value(first).name == "First",
                     "value keys survive storage reorder")) {
        return false;
    }
    definition.remove_value(second);
    const ValueKey replacement = definition.add_value("Replacement");
    return expect_true(replacement != second && replacement.value > third.value, "deleted ValueKey is not rebound");
}

bool test_names_do_not_affect_runtime()
{
    CellWorld original = make_cell_world();
    CellWorld renamed = make_cell_world();
    renamed.definition.rename_value(renamed.light, "Photon flux");
    renamed.definition.rename_value(renamed.temperature, "Heat accumulator");
    renamed.definition.rename_template(renamed.cell, "Renamed cell");

    RuntimeWorld first{original.definition};
    RuntimeWorld second{renamed.definition};
    const ObjectId first_object = first.instantiate(original.cell);
    const ObjectId second_object = second.instantiate(renamed.cell);
    first.set_input(first_object, "world.light", 1.0);
    second.set_input(second_object, "world.light", 1.0);
    first.step();
    second.step();
    return expect_near(first.value(first_object, original.temperature),
                       second.value(second_object, renamed.temperature), "names do not affect calculation");
}

bool test_sparse_keys_compile_to_dense_ids()
{
    WorldDefinition definition;
    const ValueKey first = definition.add_value("First");
    const ValueKey removed = definition.add_value("Removed");
    const ValueKey third = definition.add_value("Third");
    definition.remove_value(removed);
    (void)definition.add_template("Object");
    RuntimeWorld runtime{definition};
    const auto first_id = runtime.runtime_value_id(first);
    const auto third_id = runtime.runtime_value_id(third);
    return expect_true(first_id && third_id && first_id->index == 0 && third_id->index == 1,
                       "sparse ValueKeys map to dense ValueIds") &&
           expect_true(!runtime.runtime_value_id(removed), "deleted key is absent from runtime map");
}

bool test_cell_world_vertical_slice()
{
    CellWorld world = make_cell_world();
    RuntimeWorld runtime{world.definition};
    const ObjectId cell = runtime.instantiate(world.cell);
    runtime.set_input(cell, "world.light", 1.0);
    runtime.step();
    if (!expect_near(runtime.output(cell, "cell.used_energy"), 0.25, "first tick UsedEnergy") ||
        !expect_near(runtime.value(cell, world.energy), 0.0, "world rule consumes remaining Energy") ||
        !expect_near(runtime.output(cell, "cell.temperature"), 0.275, "first tick Temperature")) {
        return false;
    }
    runtime.set_input(cell, world.light, 1.0);
    runtime.step();
    return expect_near(runtime.output(cell, "cell.used_energy"), 0.25, "second tick UsedEnergy") &&
           expect_near(runtime.output(cell, "cell.temperature"), 0.35, "second tick Temperature persists");
}

bool test_runtime_objects_are_independent()
{
    CellWorld world = make_cell_world();
    RuntimeWorld runtime{world.definition};
    const ObjectId first = runtime.instantiate(world.cell);
    const ObjectId second = runtime.instantiate(world.cell);
    runtime.set_input(first, world.light, 1.0);
    runtime.step();
    if (!expect_near(runtime.value(first, world.temperature), 0.275, "first object advances") ||
        !expect_near(runtime.value(second, world.temperature), 0.2, "second object remains independent")) {
        return false;
    }
    runtime.set_input(second, world.light, 1.0);
    runtime.step();
    return expect_near(runtime.value(first, world.temperature), 0.275, "unstaged host input is zero next tick") &&
           expect_near(runtime.value(second, world.temperature), 0.275, "second object advances independently") &&
           expect_true(runtime.source_template(first) == world.cell && runtime.object_count() == 2,
                       "runtime retains object and template identities");
}

bool test_invalid_references_and_definitions_are_rejected()
{
    WorldDefinition definition;
    const ValueKey value = definition.add_value("Value");
    const TemplateId object = definition.add_template("Object");
    const ValueKey missing{999};
    if (!expect_throws([&] {
            (void)definition.add_genome_function(object, {
                                                              .input = value,
                                                              .output = missing,
                                                              .throughput = 1.0,
                                                          });
        }, "missing genome ValueKey") ||
        !expect_throws([&] { (void)definition.add_world_rule({.source = missing, .target = value, .target_per_source = 1.0}); },
                       "missing world rule ValueKey") ||
        !expect_throws([&] { (void)definition.add_value("Value"); }, "duplicate value name") ||
        !expect_throws([&] { (void)definition.add_template("Object"); }, "duplicate template name") ||
        !expect_throws([&] {
            (void)definition.add_host_binding(object, {
                                                          .channel = "",
                                                          .direction = HostChannelDirection::input,
                                                          .value = value,
                                                      });
        }, "empty host channel")) {
        return false;
    }
    (void)definition.add_host_binding(object, {
                                                    .channel = "world.value",
                                                    .direction = HostChannelDirection::input,
                                                    .value = value,
                                                });
    if (!expect_throws([&] {
        (void)definition.add_host_binding(object, {
                                                      .channel = "world.value",
                                                      .direction = HostChannelDirection::input,
                                                      .value = value,
                                                  });
    }, "duplicate host channel")) {
        return false;
    }
    return expect_throws([&] {
        (void)definition.add_host_binding(object, {
                                                      .channel = "world.same_value",
                                                      .direction = HostChannelDirection::input,
                                                      .value = value,
                                                  });
    }, "duplicate input value binding");
}

bool test_world_rules_are_distinct_and_compile()
{
    CellWorld world = make_cell_world();
    const auto& object = world.definition.object_template(world.cell);
    if (!expect_true(object.genome.size() == 2 && world.definition.world_rules().size() == 1,
                     "world rules are stored separately from genome")) {
        return false;
    }
    RuntimeWorld runtime{world.definition};
    const ObjectId id = runtime.instantiate(world.cell);
    runtime.set_input(id, world.light, 1.0);
    runtime.step();
    return expect_near(runtime.value(id, world.temperature), 0.275, "separate world rule compiles correctly");
}

bool test_value_storage_order_does_not_define_semantics()
{
    CellWorld forward = make_cell_world();
    CellWorld reversed = make_cell_world();
    reversed.definition.reorder_values(
        std::array{reversed.temperature, reversed.used_energy, reversed.energy, reversed.light});
    RuntimeWorld first{forward.definition};
    RuntimeWorld second{reversed.definition};
    const ObjectId a = first.instantiate(forward.cell);
    const ObjectId b = second.instantiate(reversed.cell);
    first.set_input(a, forward.light, 1.0);
    second.set_input(b, reversed.light, 1.0);
    first.step();
    second.step();
    return expect_near(first.value(a, forward.used_energy), second.value(b, reversed.used_energy),
                       "value storage order keeps genome result") &&
           expect_near(first.value(a, forward.temperature), second.value(b, reversed.temperature),
                       "value storage order keeps world-rule result");
}

bool test_mutation_api_and_runtime_validation()
{
    WorldDefinition definition;
    const ValueKey input = definition.add_value("Input");
    const ValueKey output = definition.add_value("Output");
    const ValueKey state = definition.add_value("State");
    const TemplateId object = definition.add_template("Object");
    const std::size_t function = definition.add_genome_function(object, {
                                                                             .input = input,
                                                                             .output = output,
                                                                             .throughput = 1.0,
                                                                         });
    definition.change_genome_function(object, function, {
                                                            .input = input,
                                                            .output = output,
                                                            .throughput = 2.0,
                                                            .result_per_input = 0.5,
                                                        });
    const std::size_t rule = definition.add_world_rule({.source = output, .target = state, .target_per_source = 1.0});
    definition.change_world_rule(rule, {.source = output, .target = state, .target_per_source = 2.0});
    const std::size_t binding = definition.add_host_binding(object, {
                                                                        .channel = "in",
                                                                        .direction = HostChannelDirection::input,
                                                                        .value = input,
                                                                    });
    definition.change_host_binding(object, binding, {
                                                        .channel = "world.input",
                                                        .direction = HostChannelDirection::input,
                                                        .value = input,
                                                    });
    definition.set_initial_value(object, state, 1.0);
    definition.remove_initial_value(object, state);

    RuntimeWorld runtime{definition};
    const ObjectId id = runtime.instantiate(object);
    if (!expect_throws([&] { runtime.set_input(id, output, 1.0); }, "unbound input is rejected")) {
        return false;
    }
    definition.remove_host_binding(object, binding);
    definition.remove_world_rule(rule);
    definition.remove_genome_function(object, function);
    return expect_true(definition.object_template(object).genome.empty() && definition.world_rules().empty() &&
                           definition.object_template(object).host_bindings.empty(),
                       "explicit remove operations update definitions");
}

} // namespace

int main()
{
    return test_stable_keys_survive_reorder_and_deletion() && test_names_do_not_affect_runtime() &&
                   test_sparse_keys_compile_to_dense_ids() && test_cell_world_vertical_slice() &&
                   test_runtime_objects_are_independent() && test_invalid_references_and_definitions_are_rejected() &&
                   test_world_rules_are_distinct_and_compile() && test_value_storage_order_does_not_define_semantics() &&
                   test_mutation_api_and_runtime_validation()
               ? 0
               : 1;
}
