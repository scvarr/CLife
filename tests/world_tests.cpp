#include <clife/world/calculation.hpp>
#include <clife/world/definition.hpp>
#include <clife/world/phenotype.hpp>
#include <clife/world/runtime.hpp>
#include <clife/world/runtime_rules.hpp>
#include <clife/world/shape.hpp>

#include <cmath>
#include <array>
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

bool test_runtime_rule_executor()
{
    WorldDefinition definition;
    const ValueKey energy = definition.add_value("Energy");
    const ValueKey temperature = definition.add_value("Temperature");
    const ValueKey exposure = definition.add_value("Exposure");
    const ObjectCharacteristicId heat_capacity = definition.add_object_characteristic("HeatCapacity");
    const TemplateId cell = definition.add_template("Cell");
    const CalculationId construction = definition.add_calculation("Construction");
    const CalculationPortId construction_output = definition.add_calculation_output(construction, "HeatCapacity", "5");
    definition.set_object_construction({.calculation = construction,
        .outputs = {{.output = construction_output, .characteristic = heat_capacity}}});
    const CalculationId calculation = definition.add_calculation("WorldRule");
    const CalculationPortId residual = definition.add_calculation_input(calculation, "residual");
    const CalculationPortId current = definition.add_calculation_input(calculation, "current");
    const CalculationPortId capacity = definition.add_calculation_input(calculation, "capacity");
    const CalculationPortId temperature_delta = definition.add_calculation_output(calculation, "temperature_delta", "residual / capacity");
    const CalculationPortId exposure_delta = definition.add_calculation_output(calculation, "exposure_delta", "current + residual * 0.05");
    const CompiledPhenotype phenotype = compile_phenotype(definition, cell);
    clife::Calculator calculator{{
        .value_count = 3,
        .initial_values = {{.value = clife::ValueId{0}, .amount = 1.0},
                           {.value = clife::ValueId{1}, .amount = 1.0}},
    }};
    const std::array<clife::ValueAmount, 0> no_input{};
    calculator.step(no_input);
    RuntimeRuleExecutor executor{{{
        .source = clife::ValueId{0},
        .calculation = definition.calculation(calculation),
        .inputs = {{.input = residual, .kind = RuntimeRuleInputKind::end_residual, .value = clife::ValueId{0}},
                   {.input = current, .kind = RuntimeRuleInputKind::runtime_value, .value = clife::ValueId{1}},
                   {.input = capacity, .kind = RuntimeRuleInputKind::object_characteristic, .characteristic = heat_capacity}},
        .outputs = {{.output = temperature_delta, .target = clife::ValueId{1}},
                    {.output = exposure_delta, .target = clife::ValueId{2}}},
    }}};
    executor.apply(calculator, phenotype);
    return near(calculator.value(clife::ValueId{0}), 0.0, "finalized source ordinary value is consumed") &&
           near(calculator.end_value(clife::ValueId{0}), 1.0, "ordinary residual needs no end transfer") &&
           near(calculator.value(clife::ValueId{1}), 1.2, "rule reads residual, characteristic and current runtime value") &&
           near(calculator.value(clife::ValueId{2}), 1.05, "one rule applies multiple output deltas");
}

bool test_finalize_residual_combines_ordinary_value_and_leakage()
{
    constexpr clife::ValueId energy{0};
    clife::Calculator calculator{{
        .value_count = 1,
        .buffers = {{.value = energy, .capacity = 1.0, .throughput = 1.0, .leakage = 0.5, .initial_amount = 1.0}},
    }};
    const std::array input{clife::ValueAmount{.value = energy, .amount = 2.0}};
    calculator.step(input);
    const clife::Amount residual = calculator.finalize_residual(energy);
    return near(residual, 2.5, "ordinary residual and leakage combine") &&
           near(calculator.end_value(energy), 2.5, "combined residual remains observable") &&
           near(calculator.value(energy), 0.0, "finalization clears ordinary value");
}

bool test_runtime_rule_executor_rejects_duplicate_source()
{
    const RuntimeWorldRule rule{.source = clife::ValueId{0}};
    return rejects([&] { RuntimeRuleExecutor{{rule, rule}}; }, "duplicate runtime rule source must be rejected");
}

bool test_calculation_world_rule_authoring_and_runtime()
{
    WorldDefinition definition;
    const ValueKey energy = definition.add_value("Energy");
    const ValueKey temperature = definition.add_value("Temperature");
    const ValueKey exposure = definition.add_value("Exposure");
    const ObjectCharacteristicId heat_capacity = definition.add_object_characteristic("HeatCapacity");
    const TemplateId cell = definition.add_template("Cell");
    definition.set_initial_value(cell, energy, 1.0);
    const CalculationId construction = definition.add_calculation("Construction");
    const CalculationPortId construction_output = definition.add_calculation_output(construction, "HeatCapacity", "5");
    definition.set_object_construction({.calculation = construction,
        .outputs = {{.output = construction_output, .characteristic = heat_capacity}}});
    const CalculationId calculation = definition.add_calculation("Rule");
    const CalculationPortId residual = definition.add_calculation_input(calculation, "residual");
    const CalculationPortId current = definition.add_calculation_input(calculation, "current");
    const CalculationPortId capacity = definition.add_calculation_input(calculation, "capacity");
    const CalculationPortId temperature_delta = definition.add_calculation_output(calculation, "temperature", "residual / capacity");
    const CalculationPortId exposure_delta = definition.add_calculation_output(calculation, "exposure", "current + residual * 0.05");
    const CalculationWorldRuleDefinition rule{.source = energy, .calculation = calculation,
        .inputs = {{.input = residual, .kind = CalculationWorldRuleInputSourceKind::source_residual, .value = energy},
                   {.input = current, .kind = CalculationWorldRuleInputSourceKind::runtime_value, .value = temperature},
                   {.input = capacity, .kind = CalculationWorldRuleInputSourceKind::object_characteristic, .characteristic = heat_capacity}},
        .outputs = {{.output = temperature_delta, .target = temperature}, {.output = exposure_delta, .target = exposure}}};
    (void)definition.add_calculation_world_rule(rule);
    const bool duplicate_rejected = rejects([&] { (void)definition.add_calculation_world_rule(rule); }, "duplicate calculation world rule source must fail");
    const bool missing_rejected = rejects([&] {
        auto invalid = rule; invalid.inputs.pop_back(); (void)definition.change_calculation_world_rule(0, invalid);
    }, "missing calculation world rule input must fail");
    RuntimeWorld runtime{definition};
    const ObjectId object = runtime.instantiate(cell);
    runtime.step();
    return duplicate_rejected && missing_rejected && near(runtime.value(object, energy), 0.0, "runtime rule consumes ordinary residual") &&
           near(runtime.last_end_value(object, energy), 1.0, "ordinary residual reaches runtime rule") &&
           near(runtime.value(object, temperature), 0.2, "runtime rule uses HeatCapacity") &&
           near(runtime.value(object, exposure), 0.05, "runtime rule supports multiple outputs");
}

bool test_world_rule_families_are_exclusive()
{
    WorldDefinition definition;
    const ValueKey source = definition.add_value("Source");
    const ValueKey other_source = definition.add_value("Other source");
    const ValueKey end_buffer = definition.add_value("End buffer");
    const ValueKey target = definition.add_value("Target");
    const CalculationId calculation = definition.add_calculation("Calculation rule");
    const CalculationPortId residual = definition.add_calculation_input(calculation, "residual");
    const CalculationPortId delta = definition.add_calculation_output(calculation, "delta", "residual");
    const WorldRuleDefinition legacy{.source = source, .end_buffer = end_buffer, .target = target,
                                     .target_per_source = 1.0};
    const CalculationWorldRuleDefinition modern{
        .source = other_source,
        .calculation = calculation,
        .inputs = {{.input = residual,
                    .kind = CalculationWorldRuleInputSourceKind::source_residual,
                    .value = other_source}},
        .outputs = {{.output = delta, .target = target}},
    };

    (void)definition.add_calculation_world_rule(modern);
    const bool legacy_rejected = rejects(
        [&] { (void)definition.add_world_rule(legacy); },
        "legacy world rule must be rejected while calculation world rules exist");
    definition.remove_calculation_world_rule(0);
    const std::size_t legacy_index = definition.add_world_rule(legacy);
    const bool calculation_rejected = rejects(
        [&] { (void)definition.add_calculation_world_rule(modern); },
        "calculation world rule must be rejected while legacy world rules exist");
    definition.remove_world_rule(legacy_index);
    (void)definition.add_calculation_world_rule(modern);
    const bool switched_after_removal = definition.calculation_world_rules().size() == 1;
    return legacy_rejected && calculation_rejected && switched_after_removal;
}

bool test_calculation_world_rule_consumes_function_residual()
{
    SynthesisWorld world = make_synthesis_world();
    const ValueKey temperature = world.definition.add_value("Temperature");
    const ObjectCharacteristicId heat_capacity = world.definition.add_object_characteristic("HeatCapacity");
    const CalculationId construction = world.definition.add_calculation("Construction");
    const CalculationPortId construction_output =
        world.definition.add_calculation_output(construction, "HeatCapacity", "5");
    world.definition.set_object_construction({
        .calculation = construction,
        .outputs = {{.output = construction_output, .characteristic = heat_capacity}},
    });
    const CalculationId calculation = world.definition.add_calculation("Function residual rule");
    const CalculationPortId residual = world.definition.add_calculation_input(calculation, "residual");
    const CalculationPortId capacity = world.definition.add_calculation_input(calculation, "capacity");
    const CalculationPortId delta =
        world.definition.add_calculation_output(calculation, "temperature_delta", "residual / capacity");
    (void)world.definition.add_calculation_world_rule({
        .source = world.useful,
        .calculation = calculation,
        .inputs = {{.input = residual,
                    .kind = CalculationWorldRuleInputSourceKind::source_residual,
                    .value = world.useful},
                   {.input = capacity,
                    .kind = CalculationWorldRuleInputSourceKind::object_characteristic,
                    .characteristic = heat_capacity}},
        .outputs = {{.output = delta, .target = temperature}},
    });
    RuntimeWorld runtime{world.definition};
    const ObjectId object = runtime.instantiate(world.cell);
    runtime.step();
    return near(runtime.value(object, world.useful), 0.0, "function residual is finalized for the rule") &&
           near(runtime.last_end_value(object, world.useful), 0.1,
                "function output reaches the calculation world rule without an end transfer") &&
           near(runtime.value(object, temperature), 0.02,
                "function residual calculation rule applies its delta");
}

bool test_calculation_world_rule_consumes_buffer_leakage()
{
    WorldDefinition definition;
    const ValueKey energy = definition.add_value("Energy");
    const ValueKey temperature = definition.add_value("Temperature");
    const ObjectCharacteristicId heat_capacity = definition.add_object_characteristic("HeatCapacity");
    const FunctionTypeId storage = definition.add_function_type("Storage");
    const ParameterId capacity = definition.add_genome_parameter(storage, "capacity", 1.0);
    const ParameterId leakage = definition.add_genome_parameter(storage, "leakage", 0.5);
    definition.set_buffer_process(storage, {
        .value = energy,
        .capacity = genome(capacity),
        .throughput = genome(capacity),
        .leakage = genome(leakage),
    });
    const TemplateId cell = definition.add_template("Cell");
    (void)definition.add_genome_function(cell, storage);
    definition.set_initial_value(cell, energy, 1.0);
    const CalculationId construction = definition.add_calculation("Construction");
    const CalculationPortId construction_output =
        definition.add_calculation_output(construction, "HeatCapacity", "5");
    definition.set_object_construction({
        .calculation = construction,
        .outputs = {{.output = construction_output, .characteristic = heat_capacity}},
    });
    const CalculationId calculation = definition.add_calculation("Leakage rule");
    const CalculationPortId residual = definition.add_calculation_input(calculation, "residual");
    const CalculationPortId capacity_input = definition.add_calculation_input(calculation, "capacity");
    const CalculationPortId delta =
        definition.add_calculation_output(calculation, "temperature_delta", "residual / capacity");
    (void)definition.add_calculation_world_rule({
        .source = energy,
        .calculation = calculation,
        .inputs = {{.input = residual,
                    .kind = CalculationWorldRuleInputSourceKind::source_residual,
                    .value = energy},
                   {.input = capacity_input,
                    .kind = CalculationWorldRuleInputSourceKind::object_characteristic,
                    .characteristic = heat_capacity}},
        .outputs = {{.output = delta, .target = temperature}},
    });
    RuntimeWorld runtime{definition};
    const ObjectId object = runtime.instantiate(cell);
    runtime.step();
    return near(runtime.last_end_value(object, energy), 0.5,
                "buffer leakage reaches the calculation world rule as end residual") &&
           near(runtime.value(object, temperature), 0.1,
                "buffer leakage calculation rule applies its delta");
}

bool test_runtime_rule_executor_is_order_independent()
{
    WorldDefinition definition;
    const TemplateId cell = definition.add_template("Cell");
    const CalculationId first_calculation = definition.add_calculation("First");
    const CalculationPortId first_input = definition.add_calculation_input(first_calculation, "residual");
    const CalculationPortId first_output = definition.add_calculation_output(first_calculation, "delta", "residual");
    const CalculationId second_calculation = definition.add_calculation("Second");
    const CalculationPortId second_residual = definition.add_calculation_input(second_calculation, "residual");
    const CalculationPortId second_current = definition.add_calculation_input(second_calculation, "current");
    const CalculationPortId second_output = definition.add_calculation_output(second_calculation, "delta", "current");
    const CompiledPhenotype phenotype = compile_phenotype(definition, cell);
    const RuntimeWorldRule first{.source = clife::ValueId{0}, .calculation = definition.calculation(first_calculation),
        .inputs = {{.input = first_input, .kind = RuntimeRuleInputKind::end_residual, .value = clife::ValueId{0}}},
        .outputs = {{.output = first_output, .target = clife::ValueId{1}}}};
    const RuntimeWorldRule second{.source = clife::ValueId{2}, .calculation = definition.calculation(second_calculation),
        .inputs = {{.input = second_residual, .kind = RuntimeRuleInputKind::end_residual, .value = clife::ValueId{2}},
                   {.input = second_current, .kind = RuntimeRuleInputKind::runtime_value, .value = clife::ValueId{1}}},
        .outputs = {{.output = second_output, .target = clife::ValueId{1}}}};
    const auto run = [&](std::vector<RuntimeWorldRule> rules) {
        clife::Calculator calculator{{.value_count = 3,
            .initial_values = {{.value = clife::ValueId{0}, .amount = 1.0}, {.value = clife::ValueId{2}, .amount = 1.0}}}};
        const std::array<clife::ValueAmount, 0> no_input{};
        calculator.step(no_input);
        RuntimeRuleExecutor{std::move(rules)}.apply(calculator, phenotype);
        return calculator.value(clife::ValueId{1});
    };
    return near(run({first, second}), 1.0, "rules read the pre-delta runtime state") &&
           near(run({second, first}), 1.0, "runtime rule list order does not change result");
}

bool test_direct_external_input_without_host_binding()
{
    SynthesisWorld world = make_synthesis_world();
    world.definition.remove_initial_value(world.cell, world.light);
    RuntimeWorld runtime{world.definition};
    const ObjectId object = runtime.instantiate(world.cell);
    const bool host_bound_api_rejects = rejects(
        [&] { runtime.set_input(object, world.light, 2.0); },
        "host-bound input API must reject an unbound value");
    runtime.set_external_input(object, world.light, 2.0);
    runtime.step();
    return host_bound_api_rejects &&
           near(runtime.value(object, world.useful), 0.1,
                "direct external input must reach the function process without a host binding") &&
           near(runtime.value(object, world.loss), 0.0,
                "direct external input must preserve process allocations");
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
    const CompiledPhenotype phenotype = compile_phenotype(definition, object);
    const auto& buffer = *phenotype.function(0).buffer_parameters();
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
    (void)original.definition.add_world_rule(
        {.source = original.useful, .end_buffer = original.loss, .target = original.organic, .target_per_source = 0.5});
    const WorldDefinitionSnapshot snapshot = original.definition.snapshot();
    const WorldDefinition restored = WorldDefinition::from_snapshot(snapshot);
    const FunctionTypeDefinition& type = restored.function_type(original.type);
    const CompiledPhenotype phenotype = compile_phenotype(restored, original.cell);
    const CompiledFunctionPhenotype& function = phenotype.function(0);
    WorldDefinitionSnapshot old = snapshot;
    old.schema_version = 6;
    return expect(snapshot.schema_version == 8, "current snapshot schema") &&
           expect(type.calculations.size() == 1, "calculation binding round trip") &&
           expect(type.process->outputs.size() == 2, "process round trip") &&
           expect(type.material_contributions.size() == 1, "material source round trip") &&
           expect(restored.world_rules().size() == 1, "legacy world rule round trip") &&
           expect(restored.world_rules().front().target_per_source == 0.5, "legacy world rule semantics round trip") &&
           near(function.calculation_output(original.calculation, original.efficiency), 1.0,
                "compiled output after restore") &&
           rejects([&] { (void)WorldDefinition::from_snapshot(old); }, "old schema must fail clearly");
}

bool test_calculation_world_rule_snapshot_round_trip()
{
    WorldDefinition definition;
    const ValueKey energy = definition.add_value("Energy");
    const ValueKey temperature = definition.add_value("Temperature");
    const ValueKey exposure = definition.add_value("Exposure");
    const ObjectCharacteristicId heat_capacity = definition.add_object_characteristic("HeatCapacity");
    const TemplateId cell = definition.add_template("Cell");
    definition.set_initial_value(cell, energy, 1.0);
    const CalculationId construction = definition.add_calculation("Construction");
    const CalculationPortId construction_output = definition.add_calculation_output(construction, "HeatCapacity", "5");
    definition.set_object_construction({.calculation = construction,
        .outputs = {{.output = construction_output, .characteristic = heat_capacity}}});
    const CalculationId calculation = definition.add_calculation("Rule");
    const CalculationPortId residual = definition.add_calculation_input(calculation, "residual");
    const CalculationPortId current = definition.add_calculation_input(calculation, "current");
    const CalculationPortId capacity = definition.add_calculation_input(calculation, "capacity");
    const CalculationPortId temperature_delta =
        definition.add_calculation_output(calculation, "temperature", "residual / capacity");
    const CalculationPortId exposure_delta =
        definition.add_calculation_output(calculation, "exposure", "current + residual");
    (void)definition.add_calculation_world_rule({
        .source = energy,
        .calculation = calculation,
        .inputs = {{.input = residual,
                    .kind = CalculationWorldRuleInputSourceKind::source_residual,
                    .value = energy},
                   {.input = current,
                    .kind = CalculationWorldRuleInputSourceKind::runtime_value,
                    .value = exposure},
                   {.input = capacity,
                    .kind = CalculationWorldRuleInputSourceKind::object_characteristic,
                    .characteristic = heat_capacity}},
        .outputs = {{.output = temperature_delta, .target = temperature},
                    {.output = exposure_delta, .target = exposure}},
    });

    WorldDefinitionSnapshot snapshot = definition.snapshot();
    const WorldDefinition restored = WorldDefinition::from_snapshot(snapshot);
    const auto& rule = restored.calculation_world_rules().front();
    snapshot.world_rules.push_back({.source = temperature,
                                    .end_buffer = exposure,
                                    .target = energy,
                                    .target_per_source = 1.0});
    const bool mixed_rejected = rejects([&] { (void)WorldDefinition::from_snapshot(snapshot); },
                                        "mixed snapshot world rule families must be rejected");
    RuntimeWorld runtime{restored};
    const ObjectId object = runtime.instantiate(cell);
    runtime.step();
    return expect(restored.calculation_world_rules().size() == 1, "calculation world rule snapshot round trip") &&
           expect(rule.source == energy && rule.calculation == calculation, "calculation world rule identity round trip") &&
           expect(rule.inputs.size() == 3 &&
                      rule.inputs[0].input == residual &&
                      rule.inputs[0].kind == CalculationWorldRuleInputSourceKind::source_residual &&
                      rule.inputs[0].value == energy && rule.inputs[1].input == current &&
                      rule.inputs[1].kind == CalculationWorldRuleInputSourceKind::runtime_value &&
                      rule.inputs[1].value == exposure && rule.inputs[2].input == capacity &&
                      rule.inputs[2].kind == CalculationWorldRuleInputSourceKind::object_characteristic &&
                      rule.inputs[2].characteristic == heat_capacity,
                  "all calculation world rule input kinds round trip") &&
           expect(rule.outputs.size() == 2 && rule.outputs[0].output == temperature_delta &&
                      rule.outputs[0].target == temperature && rule.outputs[1].output == exposure_delta &&
                      rule.outputs[1].target == exposure,
                  "calculation world rule output bindings round trip") &&
           mixed_rejected && near(runtime.value(object, temperature), 0.2,
                                  "restored calculation world rule compiles and executes") &&
           near(runtime.value(object, exposure), 1.0, "restored multiple output bindings execute");
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

bool test_object_characteristic_construction()
{
    WorldDefinition definition;
    const auto volume = definition.add_object_characteristic("Volume");
    const auto calculation = definition.add_calculation("Construction");
    const auto base = definition.add_calculation_input(calculation, "Base");
    const auto functions = definition.add_calculation_input(calculation, "Functions");
    const auto output = definition.add_calculation_output(calculation, "Volume", "Base + Functions");
    const auto type = definition.add_function_type("Size");
    const auto parameter = definition.add_genome_parameter(type, "Size", 2.0);
    definition.set_function_characteristic_contribution(type, volume, genome(parameter));
    const auto cell = definition.add_template("Cell");
    definition.set_template_base_characteristic(cell, volume, 5.0);
    (void)definition.add_genome_function(cell, type);
    definition.set_object_construction({.calculation = calculation,
        .inputs = {{.input = base, .source = {.kind = ObjectConstructionSourceKind::base_characteristic, .characteristic = volume}},
                   {.input = functions, .source = {.kind = ObjectConstructionSourceKind::function_contribution_sum, .characteristic = volume}}},
        .outputs = {{.output = output, .characteristic = volume}}});
    const auto phenotype = compile_phenotype(definition, cell);
    const bool result = near(phenotype.function_contribution_sum(volume), 2.0, "function characteristic sum") &&
                        near(phenotype.characteristic(volume), 7.0, "construction characteristic result");
    const auto restored = WorldDefinition::from_snapshot(definition.snapshot());
    return result && near(compile_phenotype(restored, cell).characteristic(volume), 7.0, "characteristic snapshot round trip") &&
           rejects([&] { definition.remove_object_characteristic(volume); }, "referenced characteristic removal must fail");
}

bool test_material_construction_source()
{
    WorldDefinition definition;
    const ValueKey organic = definition.add_value("StructuralOrganic");
    const ObjectCharacteristicId volume = definition.add_object_characteristic("Volume");
    const CalculationId calculation = definition.add_calculation("Volume from material");
    const CalculationPortId input = definition.add_calculation_input(calculation, "StructuralOrganic");
    const CalculationPortId output = definition.add_calculation_output(calculation, "Volume", "StructuralOrganic");
    const FunctionTypeId type = definition.add_function_type("Channel");
    const ParameterId channel = definition.add_genome_parameter(type, "Channel", 2.0);
    definition.set_function_material_contribution(type, organic, genome(channel));
    const TemplateId object = definition.add_template("Object");
    (void)definition.add_genome_function(object, type);
    (void)definition.add_genome_function(object, type);
    definition.set_genome_parameter(object, 1, channel, 3.0);
    definition.set_object_construction({.calculation = calculation,
        .inputs = {{.input = input, .source = {.kind = ObjectConstructionSourceKind::material_amount, .value = organic}}},
        .outputs = {{.output = output, .characteristic = volume}}});
    const CompiledPhenotype phenotype = compile_phenotype(definition, object);
    const bool compiled = near(phenotype.material_amount(organic), 5.0, "genome material contributions must aggregate") &&
                          near(phenotype.characteristic(volume), 5.0, "material construction source must resolve") &&
                          rejects([&] { definition.remove_value(organic); },
                                  "construction material source must prevent value removal");
    const WorldDefinition restored = WorldDefinition::from_snapshot(definition.snapshot());
    return compiled && near(compile_phenotype(restored, object).material_amount(organic), 5.0,
                            "material construction snapshot material round trip") &&
           near(compile_phenotype(restored, object).characteristic(volume), 5.0,
                "material construction snapshot characteristic round trip");
}

bool test_unit_lifecycle()
{
    WorldDefinition definition;
    const UnitId unit = definition.add_unit("L", "light unit");
    definition.update_unit(unit, "LU", "updated");
    const bool updated = definition.unit(unit).symbol == "LU" && definition.unit(unit).description == "updated";
    const WorldDefinition restored = WorldDefinition::from_snapshot(definition.snapshot());
    const bool round_trip = restored.unit(unit).description == "updated";
    const ValueKey light = definition.add_value("Light");
    definition.set_value_unit(light, {{{unit, 1}}});
    const bool referenced = rejects([&] { definition.remove_unit(unit); }, "referenced unit removal must fail");
    definition.clear_value_unit(light);
    const bool cleared = !definition.value(light).unit.has_value();
    WorldDefinition unused;
    const UnitId removable = unused.add_unit("E", "energy");
    unused.remove_unit(removable);
    return updated && round_trip && referenced && cleared && rejects([&] { (void)unused.unit(removable); }, "unused unit removal must work");
}

bool test_genome_parameter_lifecycle()
{
    WorldDefinition definition;
    const FunctionTypeId type = definition.add_function_type("Transform");
    const ParameterId parameter = definition.add_genome_parameter(type, "Rate", 1.0);
    definition.update_genome_parameter(type, parameter, "Throughput", 2.0);
    const bool updated = definition.function_type(type).genome_parameters[0].name == "Throughput" &&
                         near(definition.function_type(type).genome_parameters[0].default_value, 2.0,
                              "updated genome parameter default");
    definition.remove_genome_parameter(type, parameter);
    const bool removed = definition.function_type(type).genome_parameters.empty();

    const ParameterId referenced = definition.add_genome_parameter(type, "Allocation", 1.0);
    const ValueKey input = definition.add_value("Input");
    const ValueKey output = definition.add_value("Output");
    const UnitId unit = definition.add_unit("U");
    const UnitConversionId conversion = definition.add_unit_conversion({{{unit, 1}}}, 1.0, {{{unit, 1}}}, 1.0);
    definition.set_function_process(type, {.input = input,
                                           .throughput = genome(referenced),
                                           .conversion = conversion,
                                           .outputs = {{.output = output, .allocation = genome(referenced)}}});
    return updated && removed && rejects([&] { definition.remove_genome_parameter(type, referenced); },
                                         "referenced genome parameter removal must fail");
}

bool test_unit_conversion_lifecycle()
{
    WorldDefinition definition;
    const UnitId source = definition.add_unit("L");
    const UnitId target = definition.add_unit("EE");
    const UnitConversionId removable = definition.add_unit_conversion({{{source, 1}}}, 1.0, {{{target, 1}}}, 0.1);
    definition.remove_unit_conversion(removable);
    const bool removed = rejects([&] { (void)definition.unit_conversion(removable); }, "unused conversion removal must work");

    const UnitConversionId referenced = definition.add_unit_conversion({{{source, 1}}}, 1.0, {{{target, 1}}}, 0.1);
    const ValueKey input = definition.add_value("Input");
    const ValueKey output = definition.add_value("Output");
    const FunctionTypeId type = definition.add_function_type("Transform");
    const ParameterId throughput = definition.add_genome_parameter(type, "Throughput", 1.0);
    const ParameterId allocation = definition.add_genome_parameter(type, "Allocation", 1.0);
    definition.set_function_process(type, {.input = input,
                                           .throughput = genome(throughput),
                                           .conversion = referenced,
                                           .outputs = {{.output = output, .allocation = genome(allocation)}}});
    return removed && rejects([&] { definition.remove_unit_conversion(referenced); },
                              "referenced conversion removal must fail");
}

Amount shape_volume(const ShapePhenotype& shape, std::size_t latitudes = 160, std::size_t longitudes = 320)
{
    constexpr Amount pi = 3.141592653589793238462643383279502884;
    const Amount z_step = 2.0 / static_cast<Amount>(latitudes);
    const Amount phi_step = 2.0 * pi / static_cast<Amount>(longitudes);
    Amount integral{};
    for (std::size_t latitude = 0; latitude < latitudes; ++latitude) {
        const Amount z = -1.0 + (static_cast<Amount>(latitude) + 0.5) * z_step;
        const Amount radial = std::sqrt(1.0 - z * z);
        for (std::size_t longitude = 0; longitude < longitudes; ++longitude) {
            const Amount phi = (static_cast<Amount>(longitude) + 0.5) * phi_step;
            const Amount radius = shape.radius(radial * std::cos(phi), z, radial * std::sin(phi));
            integral += radius * radius * radius;
        }
    }
    return integral * z_step * phi_step / 3.0;
}

struct ShapeWorld final {
    WorldDefinition definition;
    TemplateId empty;
    TemplateId first;
    TemplateId reversed;
    FunctionTypeId alpha;
    FunctionTypeId beta;
    ParameterId alpha_parameter;
    ParameterId beta_parameter;
};

ShapeWorld make_shape_world()
{
    WorldDefinition definition;
    const TemplateId empty = definition.add_template("empty");
    const FunctionTypeId alpha = definition.add_function_type("alpha");
    const ParameterId alpha_parameter = definition.add_genome_parameter(alpha, "alpha parameter", 1.0);
    const FunctionTypeId beta = definition.add_function_type("beta");
    const ParameterId beta_parameter = definition.add_genome_parameter(beta, "beta parameter", 2.0);
    const TemplateId first = definition.add_template("first");
    (void)definition.add_genome_function(first, alpha);
    (void)definition.add_genome_function(first, beta);
    definition.set_genome_parameter(first, 0, alpha_parameter, 1.25);
    definition.set_genome_parameter(first, 1, beta_parameter, -0.75);
    const TemplateId reversed = definition.add_template("reversed");
    (void)definition.add_genome_function(reversed, beta);
    (void)definition.add_genome_function(reversed, alpha);
    definition.set_genome_parameter(reversed, 0, beta_parameter, -0.75);
    definition.set_genome_parameter(reversed, 1, alpha_parameter, 1.25);
    return {.definition = std::move(definition), .empty = empty, .first = first, .reversed = reversed,
            .alpha = alpha, .beta = beta, .alpha_parameter = alpha_parameter, .beta_parameter = beta_parameter};
}

bool shapes_differ(const ShapePhenotype& left, const ShapePhenotype& right)
{
    for (std::size_t index = 0; index < ShapePhenotype::coefficient_count; ++index) {
        if (std::abs(left.coefficients()[index] - right.coefficients()[index]) > 1e-12) {
            return true;
        }
    }
    return false;
}

bool test_semantic_shape_empty_genome()
{
    ShapeWorld world = make_shape_world();
    const ShapePhenotype shape = compile_semantic_shape_phenotype(world.definition, world.empty);
    constexpr Amount sphere_radius = 0.6203504908994001;
    for (const Amount coefficient : shape.coefficients()) {
        if (!near(coefficient, 0.0, "empty genome must produce zero shape coefficients")) {
            return false;
        }
    }
    return near(shape.radius(1.0, 0.0, 0.0), sphere_radius, "empty genome radius must be unit-volume sphere") &&
           near(shape.radius(0.0, 1.0, 0.0), sphere_radius, "empty genome must be direction-independent") &&
           expect(std::abs(shape_volume(shape) - 1.0) <= 1e-8, "empty genome canonical volume must be one");
}

bool test_semantic_shape_determinism_and_sensitivity()
{
    ShapeWorld world = make_shape_world();
    const ShapePhenotype first = compile_semantic_shape_phenotype(world.definition, world.first);
    const ShapePhenotype repeated = compile_semantic_shape_phenotype(world.definition, world.first);
    for (std::size_t index = 0; index < ShapePhenotype::coefficient_count; ++index) {
        if (!near(first.coefficients()[index], repeated.coefficients()[index], "same genome shape coefficients must be deterministic")) {
            return false;
        }
    }
    const ShapePhenotype reversed = compile_semantic_shape_phenotype(world.definition, world.reversed);
    if (!expect(shapes_differ(first, reversed), "genome entry order must normally affect shape")) {
        return false;
    }
    world.definition.set_genome_parameter(world.first, 0, world.alpha_parameter, 3.5);
    const ShapePhenotype changed_parameter = compile_semantic_shape_phenotype(world.definition, world.first);
    if (!expect(shapes_differ(first, changed_parameter), "genome parameter value must normally affect shape")) {
        return false;
    }
    return expect(std::abs(shape_volume(changed_parameter) - 1.0) <= 2e-5,
                  "normalized semantic shape volume must be one");
}

bool test_semantic_shape_rename_invariance_and_valid_field()
{
    ShapeWorld world = make_shape_world();
    const ShapePhenotype before = compile_semantic_shape_phenotype(world.definition, world.first);
    world.definition.rename_template(world.first, "renamed template");
    world.definition.rename_function_type(world.alpha, "renamed function");
    world.definition.update_genome_parameter(world.alpha, world.alpha_parameter, "renamed parameter", 1.0);
    const ShapePhenotype after = compile_semantic_shape_phenotype(world.definition, world.first);
    if (!expect(!shapes_differ(before, after), "authoring names must not affect semantic shape")) {
        return false;
    }
    for (std::size_t latitude = 1; latitude < 20; ++latitude) {
        for (std::size_t longitude = 0; longitude < 40; ++longitude) {
            constexpr Amount pi = 3.141592653589793238462643383279502884;
            const Amount theta = pi * static_cast<Amount>(latitude) / 20.0;
            const Amount phi = 2.0 * pi * static_cast<Amount>(longitude) / 40.0;
            const Amount radius = after.radius(std::sin(theta) * std::cos(phi), std::cos(theta),
                                               std::sin(theta) * std::sin(phi));
            if (!expect(std::isfinite(radius) && radius > 0.0, "semantic shape field must stay finite and positive")) {
                return false;
            }
        }
    }
    return true;
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
    run(test_runtime_rule_executor, "runtime rule executor");
    run(test_finalize_residual_combines_ordinary_value_and_leakage, "finalize residual combines leakage");
    run(test_runtime_rule_executor_rejects_duplicate_source, "runtime rule executor duplicate source");
    run(test_calculation_world_rule_authoring_and_runtime, "calculation world rule authoring and runtime");
    run(test_world_rule_families_are_exclusive, "world rule families are exclusive");
    run(test_calculation_world_rule_consumes_function_residual, "calculation world rule function residual");
    run(test_calculation_world_rule_consumes_buffer_leakage, "calculation world rule buffer leakage");
    run(test_runtime_rule_executor_is_order_independent, "runtime rule executor order independence");
    run(test_direct_external_input_without_host_binding, "direct external input without host binding");
    run(test_allocation_validation, "allocation validation");
    run(test_binding_validation_and_removal, "binding validation and removal");
    run(test_buffer_calculation_sources, "buffer calculation sources");
    run(test_buffer_process_removal, "buffer process removal");
    run(test_function_type_removal, "function type removal");
    run(test_calculation_lifecycle_and_stable_ids, "calculation lifecycle and stable IDs");
    run(test_calculation_dependency_safe_edits, "calculation dependency-safe edits");
    run(test_calculation_references_prevent_removal, "calculation references prevent removal");
    run(test_snapshot_round_trip, "snapshot round trip");
    run(test_calculation_world_rule_snapshot_round_trip, "calculation world rule snapshot round trip");
    run(test_snapshot_invalid_source_and_next_ids, "snapshot invalid source and next IDs");
    run(test_object_characteristic_construction, "object characteristic construction");
    run(test_material_construction_source, "material construction source");
    run(test_unit_lifecycle, "unit lifecycle");
    run(test_genome_parameter_lifecycle, "genome parameter lifecycle");
    run(test_unit_conversion_lifecycle, "unit conversion lifecycle");
    run(test_semantic_shape_empty_genome, "semantic shape empty genome");
    run(test_semantic_shape_determinism_and_sensitivity, "semantic shape determinism and sensitivity");
    run(test_semantic_shape_rename_invariance_and_valid_field, "semantic shape rename invariance and valid field");
    return success ? 0 : 1;
}
