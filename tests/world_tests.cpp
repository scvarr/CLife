#include <clife/world/definition.hpp>
#include <clife/world/phenotype.hpp>
#include <clife/world/runtime.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {

using clife::world::FunctionTypeId;
using clife::world::HostBinding;
using clife::world::HostChannelDirection;
using clife::world::ObjectId;
using clife::world::ParameterId;
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
    ValueKey heat;
    ValueKey temperature;
    TemplateId cell;
};

[[nodiscard]] CellWorld make_cell_world()
{
    WorldDefinition definition;
    const ValueKey light = definition.add_value("Light");
    const ValueKey energy = definition.add_value("Energy");
    const ValueKey used_energy = definition.add_value("UsedEnergy");
    const ValueKey heat = definition.add_value("Heat");
    const ValueKey temperature = definition.add_value("Temperature");
    const TemplateId cell = definition.add_template("Cell");

    const FunctionTypeId absorption = definition.add_function_type("Absorption");
    const ParameterId absorption_rate = definition.add_genome_parameter(absorption, "rate", 1.0);
    const ParameterId absorption_result = definition.add_derived_parameter(absorption, "result", "1");
    definition.set_function_process(absorption, {
                                                    .input = light,
                                                    .output = energy,
                                                    .throughput = absorption_rate,
                                                    .result_per_input = absorption_result,
                                                });
    const FunctionTypeId use = definition.add_function_type("Use");
    const ParameterId use_rate = definition.add_genome_parameter(use, "rate", 0.25);
    const ParameterId use_result = definition.add_derived_parameter(use, "result", "1");
    definition.set_function_process(use, {
                                             .input = energy,
                                             .output = used_energy,
                                             .throughput = use_rate,
                                             .result_per_input = use_result,
                                         });
    (void)definition.add_genome_function(cell, absorption);
    (void)definition.add_genome_function(cell, use);
    (void)definition.add_world_rule({
        .source = energy,
        .end_buffer = heat,
        .target = temperature,
        .target_per_source = 0.1,
    });
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
        .heat = heat,
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
    const FunctionTypeId process = definition.add_function_type("Process");
    const ParameterId rate = definition.add_genome_parameter(process, "rate", 1.0);
    const ParameterId result = definition.add_derived_parameter(process, "result", "1");
    if (!expect_throws(
            [&] {
                definition.set_function_process(process, {
                                                             .input = value,
                                                             .output = missing,
                                                             .throughput = rate,
                                                             .result_per_input = result,
                                                         });
            },
            "missing process ValueKey") ||
        !expect_throws(
            [&] {
                (void)definition.add_world_rule({
                    .source = missing,
                    .end_buffer = value,
                    .target = value,
                    .target_per_source = 1.0,
                });
            },
            "missing world rule ValueKey") ||
        !expect_throws([&] { (void)definition.add_value("Value"); }, "duplicate value name") ||
        !expect_throws([&] { (void)definition.add_template("Object"); }, "duplicate template name") ||
        !expect_throws(
            [&] {
                (void)definition.add_host_binding(object, {
                                                              .channel = "",
                                                              .direction = HostChannelDirection::input,
                                                              .value = value,
                                                          });
            },
            "empty host channel")) {
        return false;
    }
    (void)definition.add_host_binding(object, {
                                                  .channel = "world.value",
                                                  .direction = HostChannelDirection::input,
                                                  .value = value,
                                              });
    if (!expect_throws(
            [&] {
                (void)definition.add_host_binding(object, {
                                                              .channel = "world.value",
                                                              .direction = HostChannelDirection::input,
                                                              .value = value,
                                                          });
            },
            "duplicate host channel")) {
        return false;
    }
    return expect_throws(
        [&] {
            (void)definition.add_host_binding(object, {
                                                          .channel = "world.same_value",
                                                          .direction = HostChannelDirection::input,
                                                          .value = value,
                                                      });
        },
        "duplicate input value binding");
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

bool test_removed_template_ids_are_not_reused()
{
    WorldDefinition definition;
    const TemplateId removed = definition.add_template("Removed");
    definition.remove_template(removed);
    const TemplateId replacement = definition.add_template("Replacement");

    return expect_true(replacement.value > removed.value, "removed TemplateId is not reused") &&
           expect_throws([&] { (void)definition.object_template(removed); }, "removed TemplateId remains invalid");
}

bool test_value_storage_order_does_not_define_semantics()
{
    CellWorld forward = make_cell_world();
    CellWorld reversed = make_cell_world();
    reversed.definition.reorder_values(
        std::array{reversed.temperature, reversed.heat, reversed.used_energy, reversed.energy, reversed.light});
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
    const FunctionTypeId type = definition.add_function_type("Process");
    const ParameterId rate = definition.add_genome_parameter(type, "rate", 1.0);
    const ParameterId result = definition.add_genome_parameter(type, "result", 1.0);
    definition.set_function_process(type, {
                                              .input = input,
                                              .output = output,
                                              .throughput = rate,
                                              .result_per_input = result,
                                          });
    const std::size_t function = definition.add_genome_function(object, type);
    definition.set_genome_parameter(object, function, rate, 2.0);
    definition.set_genome_parameter(object, function, result, 0.5);
    const std::size_t rule = definition.add_world_rule({
        .source = output,
        .end_buffer = state,
        .target = state,
        .target_per_source = 1.0,
    });
    definition.change_world_rule(rule, {
                                           .source = output,
                                           .end_buffer = state,
                                           .target = state,
                                           .target_per_source = 2.0,
                                       });
    const std::size_t binding = definition.add_host_binding(object, {
                                                                        .channel = "in",
                                                                        .direction = HostChannelDirection::input,
                                                                        .value = input,
                                                                    });
    definition.change_host_binding(object, binding,
                                   {
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

bool test_genotype_compiles_to_derived_phenotype()
{
    WorldDefinition definition;
    const TemplateId cell = definition.add_template("Cell");
    const FunctionTypeId storage = definition.add_function_type("Energy Storage");
    const ParameterId capacity = definition.add_genome_parameter(storage, "capacity", 5.0);
    const ParameterId organic_size = definition.add_derived_parameter(storage, "organic_size", "capacity / 5");
    const std::size_t instance = definition.add_genome_function(cell, storage);

    const auto& genome = definition.object_template(cell).genome[instance];
    if (!expect_true(storage.value != 0 && genome.type == storage, "function type has stable identity") ||
        !expect_true(genome.parameters.size() == 1 && genome.parameters.front().parameter == capacity,
                     "genome stores only independent capacity") ||
        !expect_true(!std::ranges::any_of(genome.parameters,
                                          [organic_size](const clife::world::ParameterValue& item) {
                                              return item.parameter == organic_size;
                                          }),
                     "derived organic_size is absent from genome")) {
        return false;
    }

    const clife::world::CompiledPhenotype initial = clife::world::compile_phenotype(definition, cell);
    if (!expect_near(initial.function(instance).parameter(capacity), 5.0, "compiled capacity") ||
        !expect_near(initial.function(instance).parameter(organic_size), 1.0, "compiled organic size")) {
        return false;
    }

    definition.set_genome_parameter(cell, instance, capacity, 10.0);
    const clife::world::CompiledPhenotype changed = clife::world::compile_phenotype(definition, cell);
    if (!expect_near(changed.function(instance).parameter(organic_size), 2.0,
                     "phenotype recompiles from changed capacity")) {
        return false;
    }

    definition.rename_function_type(storage, "Renamed storage");
    definition.rename_parameter(storage, capacity, "renamed_capacity");
    definition.rename_parameter(storage, organic_size, "renamed_size");
    const clife::world::CompiledPhenotype renamed = clife::world::compile_phenotype(definition, cell);
    const FunctionTypeId another = definition.add_function_type("Another");
    return expect_near(renamed.function(instance).parameter(organic_size), 2.0,
                       "renaming metadata does not change expression identity") &&
           expect_true(another.value > storage.value, "FunctionTypeId is not rebound after rename");
}

bool test_editable_phenotype_formula_definitions()
{
    WorldDefinition definition;
    const ValueKey material = definition.add_value("Material");
    const TemplateId object = definition.add_template("Object");
    const FunctionTypeId type = definition.add_function_type("Function");
    (void)definition.add_genome_parameter(type, "input", 2.0);
    const ParameterId size = definition.add_derived_parameter(type, "size", "input");
    (void)definition.add_derived_parameter(type, "later", "size + 1");
    const std::size_t instance = definition.add_genome_function(object, type);

    if (!expect_true(definition.function_type(type).derived_parameters[0].expression_source == "input",
                     "derived parameter keeps expression source") ||
        !expect_throws([&] { definition.set_derived_parameter_expression(type, size, "size"); },
                       "derived parameter cannot reference itself") ||
        !expect_throws([&] { definition.set_derived_parameter_expression(type, size, "later"); },
                       "derived parameter cannot reference a later parameter") ||
        !expect_throws([&] { definition.set_derived_parameter_expression(type, size, "missing"); },
                       "invalid derived replacement is rejected")) {
        return false;
    }

    const auto initial = clife::world::compile_phenotype(definition, object);
    if (!expect_near(initial.function(instance).parameter(size), 2.0, "initial derived formula compiles") ||
        !expect_true(definition.function_type(type).derived_parameters[0].expression_source == "input",
                     "invalid derived replacement keeps source")) {
        return false;
    }

    definition.set_derived_parameter_expression(type, size, "input * 3");
    if (!expect_near(clife::world::compile_phenotype(definition, object).function(instance).parameter(size), 6.0,
                     "replaced derived formula changes phenotype") ||
        !expect_throws([&] { definition.set_derived_parameter_expression(type, size, "unknown + 1"); },
                       "invalid derived formula keeps prior behavior") ||
        !expect_true(definition.function_type(type).derived_parameters[0].expression_source == "input * 3",
                     "replaced derived formula keeps source") ||
        !expect_near(clife::world::compile_phenotype(definition, object).function(instance).parameter(size), 6.0,
                     "invalid derived formula leaves phenotype unchanged")) {
        return false;
    }

    definition.set_function_material_contribution(type, material, "size");
    if (!expect_true(definition.function_type(type).material_contributions.size() == 1 &&
                         definition.function_type(type).material_contributions[0].expression_source == "size",
                     "material contribution is created with source") ||
        !expect_near(clife::world::compile_phenotype(definition, object).material_amount(material), 6.0,
                     "created material expression affects phenotype")) {
        return false;
    }

    definition.set_function_material_contribution(type, material, "size * 2");
    if (!expect_true(definition.function_type(type).material_contributions.size() == 1 &&
                         definition.function_type(type).material_contributions[0].expression_source == "size * 2",
                     "material contribution is replaced without duplication") ||
        !expect_near(clife::world::compile_phenotype(definition, object).material_amount(material), 12.0,
                     "replaced material expression affects phenotype") ||
        !expect_throws([&] { definition.set_function_material_contribution(type, material, "missing"); },
                       "invalid material formula is rejected") ||
        !expect_true(definition.function_type(type).material_contributions[0].expression_source == "size * 2",
                     "invalid material formula keeps source") ||
        !expect_near(clife::world::compile_phenotype(definition, object).material_amount(material), 12.0,
                     "invalid material formula leaves phenotype unchanged")) {
        return false;
    }

    definition.remove_function_material_contribution(type, material);
    return expect_true(definition.function_type(type).material_contributions.empty(),
                       "material contribution is removed") &&
           expect_near(clife::world::compile_phenotype(definition, object).material_amount(material), 0.0,
                       "removed material contribution leaves no phenotype material") &&
           expect_throws([&] { definition.remove_function_material_contribution(type, material); },
                         "removing a missing material contribution is rejected");
}

bool test_expression_operations_and_validation()
{
    const ParameterId input{42};
    const std::array names{clife::world::ParameterName{.parameter = input, .name = "input"}};
    const std::array values{clife::world::ParameterValue{.parameter = input, .value = 4.0}};
    const clife::world::Expression expression =
        clife::world::compile_expression("max(2 + input * 3, min(20, (input - 1) / 2))", names);
    if (!expect_near(expression.evaluate(values), 14.0, "expression arithmetic and min/max")) {
        return false;
    }
    WorldDefinition definition;
    const FunctionTypeId type = definition.add_function_type("Invalid expressions");
    (void)definition.add_genome_parameter(type, "input", 1.0);
    return expect_throws([&] { (void)definition.add_derived_parameter(type, "unknown", "missing + 1"); },
                         "unknown expression parameter") &&
           expect_throws([&] { (void)definition.add_derived_parameter(type, "malformed", "min(input)"); },
                         "invalid expression syntax");
}

bool test_invalid_derived_results_are_rejected()
{
    WorldDefinition division;
    const TemplateId division_template = division.add_template("Division");
    const FunctionTypeId division_type = division.add_function_type("Division function");
    (void)division.add_genome_parameter(division_type, "capacity", 0.0);
    (void)division.add_derived_parameter(division_type, "derived", "1 / capacity");
    (void)division.add_genome_function(division_template, division_type);

    WorldDefinition overflow;
    const TemplateId overflow_template = overflow.add_template("Overflow");
    const FunctionTypeId overflow_type = overflow.add_function_type("Overflow function");
    (void)overflow.add_genome_parameter(overflow_type, "capacity", 10.0);
    (void)overflow.add_derived_parameter(overflow_type, "derived", "capacity * 1e308");
    (void)overflow.add_genome_function(overflow_template, overflow_type);

    return expect_throws([&] { (void)clife::world::compile_phenotype(division, division_template); },
                         "division by zero phenotype") &&
           expect_throws([&] { (void)clife::world::compile_phenotype(overflow, overflow_template); },
                         "non-finite phenotype");
}

} // namespace

int main()
{
    return test_stable_keys_survive_reorder_and_deletion() && test_names_do_not_affect_runtime() &&
                   test_sparse_keys_compile_to_dense_ids() && test_cell_world_vertical_slice() &&
                   test_runtime_objects_are_independent() && test_invalid_references_and_definitions_are_rejected() &&
                   test_world_rules_are_distinct_and_compile() && test_removed_template_ids_are_not_reused() &&
                   test_value_storage_order_does_not_define_semantics() && test_mutation_api_and_runtime_validation() &&
                   test_genotype_compiles_to_derived_phenotype() && test_editable_phenotype_formula_definitions() &&
                   test_expression_operations_and_validation() &&
                   test_invalid_derived_results_are_rejected()
               ? 0
               : 1;
}
