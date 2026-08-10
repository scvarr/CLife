#include <clife/world/calculation.hpp>
#include <clife/world/definition.hpp>
#include <clife/world/phenotype.hpp>
#include <clife/world/runtime.hpp>

#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

using namespace clife::world;
using clife::Amount;

bool expect(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

bool near(Amount actual, Amount expected, std::string_view message)
{
    return expect(std::abs(actual - expected) <= 1e-12, message);
}

template <typename Function> bool rejects(Function&& function, std::string_view message)
{
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    std::cerr << message << '\n';
    return false;
}

FunctionValueSource genome(ParameterId parameter)
{
    return {.kind = FunctionValueSourceKind::genome_parameter, .genome_parameter = parameter};
}

FunctionValueSource calculated(CalculationId calculation, CalculationPortId output)
{
    return {.kind = FunctionValueSourceKind::calculation_output,
            .calculation = calculation,
            .calculation_output = output};
}

struct SynthesisWorld final {
    WorldDefinition definition;
    ValueKey light;
    ValueKey useful;
    ValueKey loss;
    ValueKey organic;
    FunctionTypeId type;
    ParameterId channel;
    CalculationId calculation;
    CalculationPortId efficiency;
    CalculationPortId losses;
    TemplateId cell;
};

SynthesisWorld make_synthesis_world(Amount channel_default = 1.0)
{
    WorldDefinition definition;
    const UnitId light_unit = definition.add_unit("L");
    const UnitId energy_unit = definition.add_unit("EE");
    const ValueKey light = definition.add_value("Свет");
    const ValueKey useful = definition.add_value("ПолезнаяЭнергия");
    const ValueKey loss = definition.add_value("ПотериЭнергии");
    const ValueKey organic = definition.add_value("Органика");
    definition.set_value_unit(light, {{{light_unit, 1}}});
    definition.set_value_unit(useful, {{{energy_unit, 1}}});
    definition.set_value_unit(loss, {{{energy_unit, 1}}});
    const UnitConversionId conversion = definition.add_unit_conversion({{{light_unit, 1}}}, 1.0,
                                                                        {{{energy_unit, 1}}}, 0.1);

    const CalculationId calculation = definition.add_calculation("Характеристики энергосинтеза");
    const CalculationPortId input = definition.add_calculation_input(calculation, "Канал");
    const CalculationPortId size = definition.add_calculation_output(calculation, "Размер", "Канал");
    const CalculationPortId efficiency = definition.add_calculation_output(
        calculation, "КПД", "1 / (1 + 0.2 * max(0, Канал - 1))");
    const CalculationPortId losses = definition.add_calculation_output(calculation, "Потери", "1 - КПД");

    const FunctionTypeId type = definition.add_function_type("Энергосинтез");
    const ParameterId channel = definition.add_genome_parameter(type, "Канал", channel_default);
    definition.set_function_calculation_binding(type, {
        .calculation = calculation,
        .inputs = {{.input = input, .genome_parameter = channel}},
    });
    definition.set_function_process(type, {
        .input = light,
        .throughput = genome(channel),
        .conversion = conversion,
        .outputs = {{.output = useful, .allocation = calculated(calculation, efficiency)},
                    {.output = loss, .allocation = calculated(calculation, losses)}},
    });
    definition.set_function_material_contribution(type, organic, calculated(calculation, size));
    const TemplateId cell = definition.add_template("Клетка");
    (void)definition.add_genome_function(cell, type);
    definition.set_initial_value(cell, light, 1.0);
    return {.definition = std::move(definition), .light = light, .useful = useful, .loss = loss,
            .organic = organic, .type = type, .channel = channel, .calculation = calculation,
            .efficiency = efficiency, .losses = losses, .cell = cell};
}

bool test_calculation_binding_and_sources()
{
    SynthesisWorld world = make_synthesis_world();
    const CompiledPhenotype phenotype = compile_phenotype(world.definition, world.cell);
    const CompiledFunctionPhenotype& function = phenotype.function(0);
    return near(function.parameter(world.channel), 1.0, "genome source must resolve") &&
           near(function.calculation_output(world.calculation, world.efficiency), 1.0,
                "calculation output source must resolve") &&
           near(function.calculation_output(world.calculation, world.losses), 0.0,
                "later calculation output must use previous output") &&
           near(function.process_parameters()->throughput, 1.0, "process throughput must use source") &&
           near(function.process_parameters()->outputs[0].result_per_input, 0.1,
                "conversion and efficiency must compile") &&
           near(function.process_parameters()->outputs[1].result_per_input, 0.0,
                "loss allocation must compile") &&
           near(phenotype.material_amount(world.organic), 1.0,
                "material contribution must use calculation output");
}

bool test_runtime_multi_output_scenario()
{
    SynthesisWorld world = make_synthesis_world();
    RuntimeWorld runtime{world.definition};
    const ObjectId object = runtime.instantiate(world.cell);
    runtime.step();
    return near(runtime.value(object, world.useful), 0.1, "runtime useful output") &&
           near(runtime.value(object, world.loss), 0.0, "runtime loss output");
}

bool test_allocation_validation()
{
    SynthesisWorld valid = make_synthesis_world(2.0);
    (void)compile_phenotype(valid.definition, valid.cell);

    SynthesisWorld over = make_synthesis_world();
    const ParameterId extra = over.definition.add_genome_parameter(over.type, "Extra", 0.1);
    over.definition.change_function_process_output(
        over.type, over.loss, {.output = over.loss, .allocation = genome(extra)});
    return rejects([&] { (void)compile_phenotype(over.definition, over.cell); },
                   "allocation sum above one must be rejected");
}

bool test_binding_validation_and_removal()
{
    WorldDefinition definition;
    const CalculationId calculation = definition.add_calculation("f");
    const CalculationPortId a = definition.add_calculation_input(calculation, "a");
    const CalculationPortId b = definition.add_calculation_output(calculation, "b", "a * 2");
    const FunctionTypeId type = definition.add_function_type("type");
    const ParameterId x = definition.add_genome_parameter(type, "x", 3.0);
    const FunctionTypeId other = definition.add_function_type("other");
    const ParameterId foreign = definition.add_genome_parameter(other, "foreign", 1.0);
    const bool missing = rejects(
        [&] { definition.set_function_calculation_binding(type, {.calculation = calculation}); },
        "missing calculation input binding must be rejected");
    const bool wrong = rejects([&] {
        definition.set_function_calculation_binding(type, {
            .calculation = calculation, .inputs = {{.input = a, .genome_parameter = foreign}}});
    }, "foreign genome parameter must be rejected");
    definition.set_function_calculation_binding(type, {
        .calculation = calculation, .inputs = {{.input = a, .genome_parameter = x}}});
    definition.set_function_material_contribution(type, definition.add_value("material"), calculated(calculation, b));
    const bool referenced = rejects([&] { definition.remove_function_calculation_binding(type, calculation); },
                                    "referenced calculation binding must not be removed");
    return missing && wrong && referenced;
}

bool test_buffer_calculation_sources()
{
    WorldDefinition definition;
    const ValueKey value = definition.add_value("Energy");
    const CalculationId calculation = definition.add_calculation("storage");
    const CalculationPortId input = definition.add_calculation_input(calculation, "size");
    const CalculationPortId capacity = definition.add_calculation_output(calculation, "capacity", "size * 5");
    const CalculationPortId throughput = definition.add_calculation_output(calculation, "throughput", "capacity * 0.3");
    const CalculationPortId leakage = definition.add_calculation_output(calculation, "leakage", "0");
    const FunctionTypeId type = definition.add_function_type("Storage");
    const ParameterId size = definition.add_genome_parameter(type, "size", 2.0);
    definition.set_function_calculation_binding(type, {
        .calculation = calculation, .inputs = {{.input = input, .genome_parameter = size}}});
    definition.set_buffer_process(type, {
        .value = value,
        .capacity = calculated(calculation, capacity),
        .throughput = calculated(calculation, throughput),
        .leakage = calculated(calculation, leakage),
    });
    const TemplateId object = definition.add_template("Cell");
    (void)definition.add_genome_function(object, type);
    const auto& buffer = *compile_phenotype(definition, object).function(0).buffer_parameters();
    return near(buffer.capacity, 10.0, "buffer capacity source") &&
           near(buffer.throughput, 3.0, "buffer throughput source") &&
           near(buffer.leakage, 0.0, "buffer leakage source");
}

bool test_buffer_process_removal()
{
    WorldDefinition definition;
    const ValueKey value = definition.add_value("Energy");
    const FunctionTypeId type = definition.add_function_type("Storage");
    const ParameterId capacity = definition.add_genome_parameter(type, "capacity", 1.0);
    definition.set_buffer_process(type, {
        .value = value,
        .capacity = genome(capacity),
        .throughput = genome(capacity),
        .leakage = genome(capacity),
    });
    definition.remove_buffer_process(type);
    return expect(!definition.function_type(type).buffer_process.has_value(), "buffer process must be removable") &&
           rejects([&] { definition.remove_buffer_process(type); }, "missing buffer process removal must fail");
}

bool test_function_type_removal()
{
    WorldDefinition definition;
    const FunctionTypeId unused = definition.add_function_type("unused");
    definition.remove_function_type(unused);
    const FunctionTypeId used = definition.add_function_type("used");
    const TemplateId cell = definition.add_template("cell");
    (void)definition.add_genome_function(cell, used);
    return expect(used.value == unused.value + 1, "removed FunctionTypeId must not be reused") &&
           expect(definition.function_types().empty() == false, "used type must exist") &&
           rejects([&] { definition.remove_function_type(used); }, "referenced type removal must fail");
}

bool test_calculation_lifecycle_and_stable_ids()
{
    WorldDefinition definition;
    const CalculationId removed = definition.add_calculation("removed");
    definition.remove_calculation(removed);
    const CalculationId calculation = definition.add_calculation("remaining");
    const CalculationPortId unused_input = definition.add_calculation_input(calculation, "unused");
    definition.remove_calculation_input(calculation, unused_input);
    const CalculationPortId input = definition.add_calculation_input(calculation, "a");
    const CalculationPortId output = definition.add_calculation_output(calculation, "b", "a * 2");
    definition.remove_calculation_output(calculation, output);
    const CalculationPortId later_port = definition.add_calculation_input(calculation, "x");
    return expect(calculation.value == removed.value + 1, "removed CalculationId must not be reused") &&
           expect(later_port.value > output.value, "removed CalculationPortId must not be reused") &&
           expect(definition.calculation(calculation).inputs.size() == 2, "remaining inputs must preserve order") &&
           expect(definition.calculation(calculation).outputs.empty(), "unused output removal must work");
}

bool test_calculation_dependency_safe_edits()
{
    WorldDefinition definition;
    const CalculationId calculation = definition.add_calculation("f");
    const CalculationPortId input = definition.add_calculation_input(calculation, "a");
    const CalculationPortId first = definition.add_calculation_output(calculation, "b", "a * 2");
    const CalculationPortId second = definition.add_calculation_output(calculation, "c", "b + 1");
    const bool input_referenced = rejects([&] { definition.remove_calculation_input(calculation, input); },
                                          "referenced input removal must fail");
    const bool output_referenced = rejects([&] { definition.remove_calculation_output(calculation, first); },
                                           "output used by later output removal must fail");
    definition.set_calculation_output_expression(calculation, first, "a * 3");
    const auto values = evaluate_calculation(definition.calculation(calculation), {{{input, 2.0}}});
    const bool changed = near(values[0].amount, 6.0, "output expression update must take effect") &&
                         near(values[1].amount, 7.0, "later output must recompile after update");
    const bool atomic = rejects([&] {
                            definition.set_calculation_output_expression(calculation, first, "missing + 1");
                        }, "invalid output expression update must fail") &&
                        definition.calculation(calculation).outputs[0].expression_source == "a * 3";
    definition.remove_calculation_output(calculation, second);
    const WorldDefinition restored = WorldDefinition::from_snapshot(definition.snapshot());
    const bool snapshot = restored.calculation(calculation).outputs.size() == 1 &&
                          restored.calculation(calculation).outputs[0].id == first &&
                          restored.calculation(calculation).outputs[0].expression_source == "a * 3";
    definition.remove_calculation_output(calculation, first);
    definition.remove_calculation_input(calculation, input);
    return input_referenced && output_referenced && changed && atomic && snapshot &&
           expect(definition.calculation(calculation).inputs.empty(), "input is removable after dependencies are removed");
}

bool test_calculation_references_prevent_removal()
{
    SynthesisWorld world = make_synthesis_world();
    const CalculationPortId input = world.definition.calculation(world.calculation).inputs[0].id;
    const bool calculation_referenced = rejects([&] { world.definition.remove_calculation(world.calculation); },
                                                "bound calculation removal must fail");
    const bool input_referenced = rejects([&] {
        world.definition.remove_calculation_input(world.calculation, input);
    }, "bound calculation input removal must fail");
    const bool output_referenced = rejects([&] {
        world.definition.remove_calculation_output(world.calculation, world.efficiency);
    }, "FunctionValueSource output removal must fail");
    return calculation_referenced && input_referenced && output_referenced;
}

bool test_snapshot_round_trip()
{
    SynthesisWorld original = make_synthesis_world();
    const WorldDefinitionSnapshot snapshot = original.definition.snapshot();
    const WorldDefinition restored = WorldDefinition::from_snapshot(snapshot);
    const FunctionTypeDefinition& type = restored.function_type(original.type);
    const CompiledPhenotype phenotype = compile_phenotype(restored, original.cell);
    const CompiledFunctionPhenotype& function = phenotype.function(0);
    WorldDefinitionSnapshot old = snapshot;
    old.schema_version = 4;
    return expect(snapshot.schema_version == 5, "current snapshot schema") &&
           expect(type.calculations.size() == 1, "calculation binding round trip") &&
           expect(type.process->outputs.size() == 2, "process round trip") &&
           expect(type.material_contributions.size() == 1, "material source round trip") &&
           near(function.calculation_output(original.calculation, original.efficiency), 1.0,
                "compiled output after restore") &&
           rejects([&] { (void)WorldDefinition::from_snapshot(old); }, "old schema must fail clearly");
}

bool test_snapshot_invalid_source_and_next_ids()
{
    SynthesisWorld world = make_synthesis_world();
    WorldDefinitionSnapshot snapshot = world.definition.snapshot();
    snapshot.function_types[0].process->throughput = calculated({999}, {999});
    const bool invalid = rejects([&] { (void)WorldDefinition::from_snapshot(snapshot); },
                                 "invalid calculation source must be rejected");
    snapshot = world.definition.snapshot();
    snapshot.next_function_type_id = world.type.value;
    return invalid && rejects([&] { (void)WorldDefinition::from_snapshot(snapshot); },
                              "reused next function type id must be rejected");
}

} // namespace

int main()
{
    bool success = true;
    const auto run = [&](auto test, const char* name) {
        bool passed = false;
        try {
            passed = test();
        } catch (const std::exception& error) {
            std::cerr << "exception in " << name << ": " << error.what() << '\n';
        }
        if (!passed) {
            std::cerr << "failed: " << name << '\n';
        }
        success = passed && success;
    };
    run(test_calculation_binding_and_sources, "calculation binding and sources");
    run(test_runtime_multi_output_scenario, "runtime multi-output scenario");
    run(test_allocation_validation, "allocation validation");
    run(test_binding_validation_and_removal, "binding validation and removal");
    run(test_buffer_calculation_sources, "buffer calculation sources");
    run(test_buffer_process_removal, "buffer process removal");
    run(test_function_type_removal, "function type removal");
    run(test_calculation_lifecycle_and_stable_ids, "calculation lifecycle and stable IDs");
    run(test_calculation_dependency_safe_edits, "calculation dependency-safe edits");
    run(test_calculation_references_prevent_removal, "calculation references prevent removal");
    run(test_snapshot_round_trip, "snapshot round trip");
    run(test_snapshot_invalid_source_and_next_ids, "snapshot invalid source and next IDs");
    return success ? 0 : 1;
}
