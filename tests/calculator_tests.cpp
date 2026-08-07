#include <clife/core/calculator.hpp>
#include <clife/core/simulation.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

constexpr clife::ValueId kLight{0};
constexpr clife::ValueId kEnergy{1};
constexpr clife::ValueId kUsedEnergy{2};
constexpr clife::ValueId kTemperature{3};
constexpr clife::ValueId kOther{4};

bool expect_equal(clife::Tick actual, clife::Tick expected, const char* message)
{
    if (actual == expected) {
        return true;
    }
    std::cerr << message << ": expected " << expected << ", got " << actual << '\n';
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

bool expect_invalid_program(clife::Program program, const char* message)
{
    try {
        const clife::Calculator calculator{std::move(program)};
        (void)calculator;
    } catch (const std::invalid_argument&) {
        return true;
    }
    std::cerr << message << ": expected std::invalid_argument\n";
    return false;
}

bool test_simulation_lifecycle()
{
    clife::Simulation simulation;
    if (!expect_equal(simulation.tick(), 0, "new simulation tick")) {
        return false;
    }
    simulation.step();
    if (!expect_equal(simulation.tick(), 1, "tick after one step")) {
        return false;
    }
    for (clife::Tick i = 0; i < 99; ++i) {
        simulation.step();
    }
    return expect_equal(simulation.tick(), 100, "tick after deterministic stepping");
}

bool test_cell_world_slice()
{
    clife::Calculator calculator{{
        .value_count = 5,
        .functions =
            {
                {.input = kLight, .output = kEnergy, .throughput = 1.0},
                {.input = kEnergy, .output = kUsedEnergy, .throughput = 0.25},
            },
        .end_rules = {{.source = kEnergy, .target = kTemperature, .target_per_source = 0.1}},
        .initial_values = {{.value = kTemperature, .amount = 0.2}},
    }};

    const std::array external{clife::ValueAmount{.value = kLight, .amount = 1.0}};
    calculator.step(external);
    if (!expect_near(calculator.value(kUsedEnergy), 0.25, "genome uses quarter energy") ||
        !expect_near(calculator.value(kTemperature), 0.275, "unused energy heats cell by world rule") ||
        !expect_near(calculator.value(kEnergy), 0.0, "end rule consumes unused energy") ||
        !expect_near(calculator.value(kOther), 0.0, "unconfigured value stays zero")) {
        return false;
    }

    calculator.step(external);
    return expect_near(calculator.value(kUsedEnergy), 0.25, "genome output is recomputed each tick") &&
           expect_near(calculator.value(kTemperature), 0.35, "world result persists between ticks");
}

bool test_declaration_order_does_not_change_pipeline()
{
    const std::vector<clife::Function> forward{
        {.input = kLight, .output = kEnergy, .throughput = 1.0},
        {.input = kEnergy, .output = kUsedEnergy, .throughput = 0.25},
    };
    auto reversed = forward;
    std::swap(reversed[0], reversed[1]);

    const auto run = [](std::vector<clife::Function> functions) {
        clife::Calculator calculator{{
            .value_count = 4,
            .functions = std::move(functions),
            .end_rules = {{.source = kEnergy, .target = kTemperature, .target_per_source = 0.1}},
        }};
        const std::array external{clife::ValueAmount{.value = kLight, .amount = 1.0}};
        calculator.step(external);
        return std::array<clife::Amount, 2>{calculator.value(kUsedEnergy), calculator.value(kTemperature)};
    };

    const auto first = run(forward);
    const auto second = run(std::move(reversed));
    return expect_near(first[0], second[0], "function declaration order keeps used energy") &&
           expect_near(first[1], second[1], "function declaration order keeps temperature");
}

bool test_competing_functions_share_input_proportionally()
{
    constexpr clife::ValueId first_output{1};
    constexpr clife::ValueId second_output{2};

    const std::vector<clife::Function> forward{
        {.input = kLight, .output = first_output, .throughput = 1.0},
        {.input = kLight, .output = second_output, .throughput = 2.0},
    };
    auto reversed = forward;
    std::swap(reversed[0], reversed[1]);

    const auto run = [first_output, second_output](std::vector<clife::Function> functions) {
        clife::Calculator calculator{{.value_count = 3, .functions = std::move(functions)}};
        const std::array external{clife::ValueAmount{.value = kLight, .amount = 1.0}};
        calculator.step(external);
        return std::array<clife::Amount, 2>{calculator.value(first_output), calculator.value(second_output)};
    };

    const auto a = run(forward);
    const auto b = run(std::move(reversed));
    return expect_near(a[0], 1.0 / 3.0, "first consumer gets proportional share") &&
           expect_near(a[1], 2.0 / 3.0, "second consumer gets proportional share") &&
           expect_near(a[0], b[0], "competition ignores declaration order for first consumer") &&
           expect_near(a[1], b[1], "competition ignores declaration order for second consumer");
}

bool test_multiple_producers_feed_downstream_value()
{
    constexpr clife::ValueId first_input{0};
    constexpr clife::ValueId second_input{1};
    constexpr clife::ValueId combined{2};
    constexpr clife::ValueId output{3};

    clife::Calculator calculator{{
        .value_count = 4,
        .functions =
            {
                {.input = combined, .output = output, .throughput = 10.0},
                {.input = first_input, .output = combined, .throughput = 1.0},
                {.input = second_input, .output = combined, .throughput = 1.0},
            },
    }};

    const std::array external{
        clife::ValueAmount{.value = first_input, .amount = 1.0},
        clife::ValueAmount{.value = second_input, .amount = 2.0},
    };
    calculator.step(external);

    return expect_near(calculator.value(output), 2.0, "downstream waits for all upstream producers");
}

bool test_end_rules_are_simultaneous()
{
    constexpr clife::ValueId first{0};
    constexpr clife::ValueId second{1};
    constexpr clife::ValueId third{2};

    const std::vector<clife::EndRule> forward{
        {.source = first, .target = second, .target_per_source = 2.0},
        {.source = second, .target = third, .target_per_source = 3.0},
    };
    auto reversed = forward;
    std::swap(reversed[0], reversed[1]);

    const auto run = [first, second, third](std::vector<clife::EndRule> rules) {
        clife::Calculator calculator{{
            .value_count = 3,
            .end_rules = std::move(rules),
            .initial_values = {{.value = first, .amount = 1.0}, {.value = second, .amount = 4.0}},
        }};
        calculator.step({});
        return std::array<clife::Amount, 3>{calculator.value(first), calculator.value(second), calculator.value(third)};
    };

    const auto a = run(forward);
    const auto b = run(std::move(reversed));
    return expect_near(a[0], 0.0, "first source is consumed") &&
           expect_near(a[1], 2.0, "second receives only pre-rule first value") &&
           expect_near(a[2], 12.0, "third receives pre-rule second value") &&
           expect_near(a[0], b[0], "end rule order keeps first value") &&
           expect_near(a[1], b[1], "end rule order keeps second value") &&
           expect_near(a[2], b[2], "end rule order keeps third value");
}

bool test_same_tick_cycle_is_rejected()
{
    return expect_invalid_program(
        {
            .value_count = 2,
            .functions =
                {
                    {.input = clife::ValueId{0}, .output = clife::ValueId{1}, .throughput = 1.0},
                    {.input = clife::ValueId{1}, .output = clife::ValueId{0}, .throughput = 1.0},
                },
        },
        "same-tick cycle");
}

} // namespace

int main()
{
    if (!test_simulation_lifecycle()) {
        return 1;
    }
    if (!test_cell_world_slice()) {
        return 1;
    }
    if (!test_declaration_order_does_not_change_pipeline()) {
        return 1;
    }
    if (!test_competing_functions_share_input_proportionally()) {
        return 1;
    }
    if (!test_multiple_producers_feed_downstream_value()) {
        return 1;
    }
    if (!test_end_rules_are_simultaneous()) {
        return 1;
    }
    if (!test_same_tick_cycle_is_rejected()) {
        return 1;
    }
    return 0;
}
