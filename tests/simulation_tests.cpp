#include <clife/core/cell.hpp>
#include <clife/core/simulation.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <utility>
#include <vector>

namespace {

constexpr clife::FieldType kFirstField{1};
constexpr clife::FieldType kSecondField{3};
constexpr clife::FieldType kSharedField{4};
constexpr clife::ResourceType kFirstResource{7};
constexpr clife::ResourceType kSecondResource{11};
constexpr clife::ResourceType kOtherResource{12};
constexpr clife::StateType kFirstState{2};
constexpr clife::StateType kSecondState{5};
constexpr clife::StateType kOtherState{9};

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

    constexpr clife::Tick additional_steps{99};
    for (clife::Tick i = 0; i < additional_steps; ++i) {
        simulation.step();
    }

    return expect_equal(simulation.tick(), 100, "tick after deterministic stepping");
}

bool test_cell_multiple_functions_and_stores()
{
    const clife::CellPhenotype phenotype{
        .transforms =
            {
                {.input = kFirstField, .output = kFirstResource, .throughput = 1.0},
                {.input = kSecondField, .output = kSecondResource, .throughput = 1.0},
            },
        .stores =
            {
                {.resource = kFirstResource, .capacity = 0.5},
                {.resource = kFirstResource, .capacity = 1.0},
                {.resource = kSecondResource, .capacity = 2.0},
            },
        .remainders =
            {
                {.resource = kFirstResource, .state = kFirstState},
                {.resource = kSecondResource, .state = kSecondState},
            },
    };

    clife::Cell cell{phenotype};

    std::array<clife::Amount, 4> fields{0.0, 0.25, 0.0, 2.0};
    cell.step({.fields = fields});
    if (!expect_near(cell.stored(kFirstResource), 0.25, "first resource after first tick") ||
        !expect_near(cell.stored(kSecondResource), 1.0, "second resource after first tick") ||
        !expect_near(cell.stored(kOtherResource), 0.0, "unconfigured resource remains absent") ||
        !expect_near(cell.state(kFirstState), 0.0, "first state starts without remainder") ||
        !expect_near(cell.state(kSecondState), 0.0, "second state starts without remainder") ||
        !expect_near(cell.state(kOtherState), 0.0, "unconfigured state remains absent")) {
        return false;
    }

    fields = {0.0, 2.0, 0.0, 2.0};
    cell.step({.fields = fields});
    if (!expect_near(cell.stored(kFirstResource), 1.25, "first resource uses combined store capacity") ||
        !expect_near(cell.stored(kSecondResource), 2.0, "second resource reaches its own capacity") ||
        !expect_near(cell.state(kFirstState), 0.0, "first state still has no remainder") ||
        !expect_near(cell.state(kSecondState), 0.0, "second state still has no remainder")) {
        return false;
    }

    cell.step({.fields = fields});
    return expect_near(cell.stored(kFirstResource), 1.5, "first resource remains independently capped") &&
           expect_near(cell.stored(kSecondResource), 2.0, "second resource remains independently capped") &&
           expect_near(cell.state(kFirstState), 0.75, "first remainder becomes first state") &&
           expect_near(cell.state(kSecondState), 1.0, "second remainder becomes second state");
}

bool test_unstored_tick_resource_becomes_state()
{
    const clife::CellPhenotype phenotype{
        .transforms =
            {
                {.input = kFirstField, .output = kFirstResource, .throughput = 1.0},
            },
        .stores =
            {
                {.resource = kFirstResource, .capacity = 0.25},
            },
        .remainders =
            {
                {.resource = kFirstResource, .state = kFirstState},
            },
    };

    clife::Cell cell{phenotype};

    std::array<clife::Amount, 2> fields{0.0, 1.0};
    cell.step({.fields = fields});
    if (!expect_near(cell.stored(kFirstResource), 0.25, "store keeps only its available capacity") ||
        !expect_near(cell.state(kFirstState), 0.75, "unstored remainder becomes persistent state")) {
        return false;
    }

    fields = {0.0, 0.0};
    cell.step({.fields = fields});
    if (!expect_near(cell.stored(kFirstResource), 0.25, "stored resource persists without new input") ||
        !expect_near(cell.state(kFirstState), 0.75, "state persists without new input")) {
        return false;
    }

    fields = {0.0, 1.0};
    cell.step({.fields = fields});
    return expect_near(cell.stored(kFirstResource), 0.25, "full store remains capped") &&
           expect_near(cell.state(kFirstState), 1.75, "full store sends whole new remainder to state");
}

bool test_competing_transforms_share_field()
{
    const std::vector<clife::FieldToResourceTransform> transforms{
        {.input = kSharedField, .output = kFirstResource, .throughput = 1.0},
        {.input = kSharedField, .output = kSecondResource, .throughput = 1.0},
    };

    const std::vector<clife::Store> stores{
        {.resource = kFirstResource, .capacity = 10.0},
        {.resource = kSecondResource, .capacity = 10.0},
    };

    const std::vector<clife::RemainderToState> remainders{
        {.resource = kFirstResource, .state = kFirstState},
        {.resource = kSecondResource, .state = kSecondState},
    };

    const auto run = [&stores, &remainders](std::vector<clife::FieldToResourceTransform> ordered_transforms) {
        clife::Cell cell{
            {.transforms = std::move(ordered_transforms), .stores = stores, .remainders = remainders}};
        std::array<clife::Amount, 5> fields{0.0, 0.0, 0.0, 0.0, 1.0};
        cell.step({.fields = fields});
        return std::array<clife::Amount, 2>{cell.stored(kFirstResource), cell.stored(kSecondResource)};
    };

    const auto forward = run(transforms);
    auto reversed_transforms = transforms;
    std::swap(reversed_transforms[0], reversed_transforms[1]);
    const auto reversed = run(std::move(reversed_transforms));

    return expect_near(forward[0], 0.5, "first competing transform gets equal load fraction") &&
           expect_near(forward[1], 0.5, "second competing transform gets equal load fraction") &&
           expect_near(reversed[0], forward[0], "transform order does not change first result") &&
           expect_near(reversed[1], forward[1], "transform order does not change second result");
}

} // namespace

int main()
{
    if (!test_simulation_lifecycle()) {
        return 1;
    }

    if (!test_cell_multiple_functions_and_stores()) {
        return 1;
    }

    if (!test_unstored_tick_resource_becomes_state()) {
        return 1;
    }

    if (!test_competing_transforms_share_field()) {
        return 1;
    }

    return 0;
}
