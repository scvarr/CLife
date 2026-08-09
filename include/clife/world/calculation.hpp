#pragma once

#include <clife/core/calculator.hpp>
#include <clife/world/definition.hpp>

#include <span>
#include <vector>

namespace clife::world {

struct CalculationPortAmount final {
    CalculationPortId port;
    Amount amount;
};

[[nodiscard]] std::vector<CalculationPortAmount> evaluate_calculation(
    const CalculationDefinition& calculation, std::span<const CalculationPortAmount> inputs);

} // namespace clife::world
