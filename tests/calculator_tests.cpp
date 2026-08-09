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
    if (!expect_near(calculator.value(kUsedEnergy), 0.5, "tick 1 Energy Use receives normal demand") ||
        !expect_near(first.stored_amount, 0.5, "tick 1 storage amount") ||
        !expect_near(first.received_last_tick, 0.5, "tick 1 storage receives surplus") ||
        !expect_near(first.supplied_last_tick, 0.0, "tick 1 storage supplied") ||
        !expect_near(calculator.end_value(heat), 0.0, "tick 1 end Heat") ||
        !expect_near(calculator.value(temperature), 0.2, "tick 1 Temperature")) {
        return false;
    }

    calculator.step(external);
    const clife::BufferState second = calculator.buffer_state(0);
    if (!expect_near(calculator.value(kUsedEnergy), 0.5, "tick 2 Energy Use receives normal demand") ||
        !expect_near(second.stored_amount, 1.0, "tick 2 storage amount") ||
        !expect_near(second.received_last_tick, 0.5, "tick 2 storage receives surplus") ||
        !expect_near(second.supplied_last_tick, 0.0, "tick 2 storage supplied")) {
        return false;
    }

    const std::array no_light{clife::ValueAmount{.value = kLight, .amount = 0.0}};
    calculator.step(no_light);
    const clife::BufferState third = calculator.buffer_state(0);
    return expect_near(calculator.value(kUsedEnergy), 0.5, "storage covers Energy deficit") &&
           expect_near(third.stored_amount, 0.5, "storage loses only discharged deficit") &&
           expect_near(third.received_last_tick, 0.0, "discharging storage does not receive") &&
           expect_near(third.supplied_last_tick, 0.5, "storage supplies Energy deficit") &&
           expect_near(calculator.end_value(heat), 0.0, "no surplus reaches END Heat");
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

bool test_buffer_surplus_limits_and_end_buffer()
{
    constexpr clife::ValueId flow{0};
    constexpr clife::ValueId output{1};
    constexpr clife::ValueId heat{2};
    clife::Calculator capacity_limited{{
        .value_count = 3,
        .functions = {{.input = flow, .output = output, .throughput = 0.5}},
        .buffers = {{.value = flow, .capacity = 0.1, .throughput = 0.25}},
        .end_buffer_transfers = {{.source = flow, .target = heat}},
    }};
    const std::array external{clife::ValueAmount{.value = flow, .amount = 1.0}};
    capacity_limited.step(external);
    const clife::BufferState state = capacity_limited.buffer_state(0);
    if (!expect_near(capacity_limited.value(output), 0.5, "normal consumer is satisfied before charging") ||
        !expect_near(state.received_last_tick, 0.1, "buffer charge respects capacity") ||
        !expect_near(state.stored_amount, 0.1, "buffer remains within capacity") ||
        !expect_near(capacity_limited.end_value(heat), 0.4, "unaccepted surplus reaches END Heat") ||
        !expect_true(state.received_last_tick == 0.0 || state.supplied_last_tick == 0.0,
                     "buffer never receives and supplies in one tick")) {
        return false;
    }

    clife::Calculator throughput_limited{{
        .value_count = 2,
        .buffers = {{.value = flow, .capacity = 1.0, .throughput = 0.25}},
        .end_buffer_transfers = {{.source = flow, .target = output}},
    }};
    throughput_limited.step(external);
    return expect_near(throughput_limited.buffer_state(0).received_last_tick, 0.25,
                       "buffer charge respects throughput") &&
           expect_near(throughput_limited.end_value(output), 0.75, "throughput-limited surplus reaches END Heat");
}

bool test_multiple_buffers_share_surplus_and_deficit_proportionally()
{
    constexpr clife::ValueId flow{0};
    constexpr clife::ValueId output{1};
    std::vector<clife::BufferProcess> forward{
        {.value = flow, .capacity = 2.0, .throughput = 0.5},
        {.value = flow, .capacity = 3.0, .throughput = 1.0},
    };
    auto reversed = forward;
    std::reverse(reversed.begin(), reversed.end());

    const auto charge = [flow, output](std::vector<clife::BufferProcess> buffers) {
        clife::Calculator calculator{{
            .value_count = 2,
            .functions = {{.input = flow, .output = output, .throughput = 0.2}},
            .buffers = std::move(buffers),
        }};
        const std::array external{clife::ValueAmount{.value = flow, .amount = 1.0}};
        calculator.step(external);
        std::array<clife::Amount, 2> received{
            calculator.buffer_state(0).received_last_tick, calculator.buffer_state(1).received_last_tick};
        std::sort(received.begin(), received.end());
        return received;
    };
    const auto charged_forward = charge(forward);
    const auto charged_reversed = charge(std::move(reversed));
    if (!expect_near(charged_forward[0], 0.8 / 3.0, "smaller charge demand gets proportional share") ||
        !expect_near(charged_forward[1], 0.8 * 2.0 / 3.0, "larger charge demand gets proportional share") ||
        !expect_near(charged_forward[0], charged_reversed[0], "charging ignores declaration order") ||
        !expect_near(charged_forward[1], charged_reversed[1], "charging ignores declaration order")) {
        return false;
    }

    forward[0].initial_amount = 0.5;
    forward[1].initial_amount = 1.0;
    reversed = forward;
    std::reverse(reversed.begin(), reversed.end());
    const auto discharge = [flow, output](std::vector<clife::BufferProcess> buffers) {
        clife::Calculator calculator{{
            .value_count = 2,
            .functions = {{.input = flow, .output = output, .throughput = 1.0}},
            .buffers = std::move(buffers),
        }};
        const std::array external{clife::ValueAmount{.value = flow, .amount = 0.0}};
        calculator.step(external);
        std::array<clife::Amount, 2> supplied{
            calculator.buffer_state(0).supplied_last_tick, calculator.buffer_state(1).supplied_last_tick};
        std::sort(supplied.begin(), supplied.end());
        return std::array<clife::Amount, 3>{calculator.value(output), supplied[0], supplied[1]};
    };
    const auto discharged_forward = discharge(forward);
    const auto discharged_reversed = discharge(std::move(reversed));
    return expect_near(discharged_forward[0], 1.0, "buffers cover only the normal deficit") &&
           expect_near(discharged_forward[1], 1.0 / 3.0, "smaller discharge offer gets proportional share") &&
           expect_near(discharged_forward[2], 2.0 / 3.0, "larger discharge offer gets proportional share") &&
           expect_near(discharged_forward[0], discharged_reversed[0], "discharging ignores declaration order") &&
           expect_near(discharged_forward[1], discharged_reversed[1], "discharging ignores declaration order") &&
           expect_near(discharged_forward[2], discharged_reversed[2], "discharging ignores declaration order");
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

bool test_multi_output_function_flow()
{
    constexpr clife::ValueId input{0};
    constexpr clife::ValueId useful{1};
    constexpr clife::ValueId loss{2};
    constexpr clife::ValueId downstream{3};
    constexpr clife::ValueId competing{4};
    clife::Calculator calculator{{
        .value_count = 5,
        .functions = {
            {.input = input, .throughput = 1.0,
             .outputs = {{.value = useful, .result_per_input = 0.08}, {.value = loss, .result_per_input = 0.02}}},
            {.input = input, .throughput = 1.0, .outputs = {{.value = competing, .result_per_input = 1.0}}},
            {.input = useful, .throughput = 1.0, .outputs = {{.value = downstream, .result_per_input = 1.0}}},
        },
    }};
    calculator.step(std::array{clife::ValueAmount{.value = input, .amount = 1.0}});
    return expect_near(calculator.value(useful), 0.0, "downstream consumes one multi-output result") &&
           expect_near(calculator.value(loss), 0.01, "multi-output loss is based on one proportional input share") &&
           expect_near(calculator.value(downstream), 0.04, "downstream reads multi-output value") &&
           expect_near(calculator.value(competing), 0.5, "multi-output function competes once for input");
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
    if (!test_buffer_surplus_limits_and_end_buffer()) {
        return 1;
    }
    if (!test_multiple_buffers_share_surplus_and_deficit_proportionally()) {
        return 1;
    }
    if (!test_same_tick_cycle_is_rejected()) {
        return 1;
    }
    if (!test_multi_output_function_flow()) {
        return 1;
    }
    return 0;
}
