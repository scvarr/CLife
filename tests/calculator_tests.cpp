#include <clife/core/calculator.hpp>
#include <clife/core/simulation.hpp>

#include <algorithm>
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
        .end_buffer_transfers = {{.source = kEnergy, .target = kOther}},
        .end_rules = {{.source = kOther, .target = kTemperature, .target_per_source = 0.1}},
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
            .value_count = 5,
            .functions = std::move(functions),
            .end_buffer_transfers = {{.source = kEnergy, .target = kOther}},
            .end_rules = {{.source = kOther, .target = kTemperature, .target_per_source = 0.1}},
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

bool test_buffer_flow_and_end_buffer()
{
    constexpr clife::ValueId heat{3};
    constexpr clife::ValueId temperature{4};
    clife::Calculator calculator{{
        .value_count = 5,
        .functions = {
            {.input = kLight, .output = kEnergy, .throughput = 1.0},
            {.input = kEnergy, .output = kUsedEnergy, .throughput = 0.5},
        },
        .buffers = {{.value = kEnergy, .capacity = 5.0, .throughput = 1.5}},
        .end_buffer_transfers = {{.source = kEnergy, .target = heat}},
        .end_rules = {{.source = heat, .target = temperature, .target_per_source = 0.1}},
        .initial_values = {{.value = temperature, .amount = 0.2}},
    }};
    const std::array external{clife::ValueAmount{.value = kLight, .amount = 1.0}};

    calculator.step(external);
    const clife::BufferState first = calculator.buffer_state(0);
    if (!expect_near(calculator.value(kUsedEnergy), 0.25, "tick 1 proportional Energy Use") ||
        !expect_near(first.stored_amount, 0.75, "tick 1 storage amount") ||
        !expect_near(first.received_last_tick, 0.75, "tick 1 storage received") ||
        !expect_near(first.supplied_last_tick, 0.0, "tick 1 storage supplied") ||
        !expect_near(calculator.end_value(heat), 0.0, "tick 1 end Heat") ||
        !expect_near(calculator.value(temperature), 0.2, "tick 1 Temperature")) {
        return false;
    }

    calculator.step(external);
    const clife::BufferState second = calculator.buffer_state(0);
    if (!expect_near(calculator.value(kUsedEnergy), 0.4375, "tick 2 proportional Energy Use") ||
        !expect_near(second.stored_amount, 1.3125, "tick 2 storage amount") ||
        !expect_near(second.received_last_tick, 1.3125, "tick 2 storage received") ||
        !expect_near(second.supplied_last_tick, 0.75, "tick 2 storage supplied")) {
        return false;
    }

    calculator.step(external);
    const clife::BufferState third = calculator.buffer_state(0);
    const clife::Amount expected_storage = 1.3125 - (1.3125 * 2.0 / 2.3125) + 1.5;
    const clife::Amount expected_heat = 1.0 * (1.0 - 2.0 / 2.3125);
    return expect_near(third.stored_amount, expected_storage, "tick 3 unused buffer offer remains stored") &&
           expect_near(third.supplied_last_tick, 1.3125 * 2.0 / 2.3125,
                       "tick 3 buffer source scales proportionally") &&
           expect_near(calculator.end_value(heat), expected_heat, "tick 3 unused fresh Energy becomes END Heat") &&
           expect_near(calculator.value(temperature), 0.2 + expected_heat * 0.1,
                       "tick 3 END Heat changes Temperature");
}

bool expect_true(bool condition, const char* message)
{
    if (condition) {
        return true;
    }
    std::cerr << message << ": expected true\n";
    return false;
}

bool test_end_buffer_is_not_pipeline_input_and_clears()
{
    constexpr clife::ValueId source{0};
    constexpr clife::ValueId heat{1};
    constexpr clife::ValueId pipeline_output{2};
    clife::Calculator calculator{{
        .value_count = 3,
        .functions = {{.input = heat, .output = pipeline_output, .throughput = 10.0}},
        .end_buffer_transfers = {{.source = source, .target = heat}},
    }};
    const std::array first_input{clife::ValueAmount{.value = source, .amount = 1.0}};
    calculator.step(first_input);
    if (!expect_near(calculator.value(pipeline_output), 0.0, "pipeline cannot read current END Heat") ||
        !expect_near(calculator.end_value(heat), 1.0, "end buffer records current tick")) {
        return false;
    }
    const std::array second_input{clife::ValueAmount{.value = source, .amount = 0.0}};
    calculator.step(second_input);
    return expect_near(calculator.end_value(heat), 0.0, "end buffer clears between ticks");
}

bool test_buffer_limits_and_declaration_order()
{
    constexpr clife::ValueId flow{0};
    constexpr clife::ValueId output{1};
    const std::vector<clife::BufferProcess> forward{
        {.value = flow, .capacity = 5.0, .throughput = 1.5, .initial_amount = 0.75},
        {.value = flow, .capacity = 3.0, .throughput = 0.5, .initial_amount = 0.25},
    };
    auto reversed = forward;
    std::reverse(reversed.begin(), reversed.end());

    const auto run = [flow, output](std::vector<clife::BufferProcess> buffers) {
        clife::Calculator calculator{{
            .value_count = 2,
            .functions = {{.input = flow, .output = output, .throughput = 0.5}},
            .buffers = std::move(buffers),
        }};
        const std::array external{clife::ValueAmount{.value = flow, .amount = 1.0}};
        calculator.step(external);
        std::array<clife::Amount, 2> stored{
            calculator.buffer_state(0).stored_amount,
            calculator.buffer_state(1).stored_amount,
        };
        std::sort(stored.begin(), stored.end());
        return std::array<clife::Amount, 3>{calculator.value(output), stored[0], stored[1]};
    };

    const auto first = run(forward);
    const auto second = run(std::move(reversed));
    if (!expect_near(first[0], second[0], "buffer order keeps consumer result") ||
        !expect_near(first[1], second[1], "buffer order keeps first state") ||
        !expect_near(first[2], second[2], "buffer order keeps second state")) {
        return false;
    }

    clife::Calculator capacity_limited{{
        .value_count = 1,
        .buffers = {{.value = flow, .capacity = 1.0, .throughput = 1.5, .initial_amount = 0.75}},
    }};
    const std::array external{clife::ValueAmount{.value = flow, .amount = 1.0}};
    capacity_limited.step(external);
    const clife::BufferState state = capacity_limited.buffer_state(0);
    return expect_near(state.received_last_tick, 0.25, "buffer demand is limited by remaining capacity") &&
           expect_true(state.supplied_last_tick <= 0.75, "buffer offer is limited by stored amount") &&
           expect_true(state.stored_amount >= 0.0 && state.stored_amount <= 1.0, "buffer remains within capacity");
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
    if (!test_buffer_flow_and_end_buffer()) {
        return 1;
    }
    if (!test_end_buffer_is_not_pipeline_input_and_clears()) {
        return 1;
    }
    if (!test_buffer_limits_and_declaration_order()) {
        return 1;
    }
    if (!test_same_tick_cycle_is_rejected()) {
        return 1;
    }
    return 0;
}
