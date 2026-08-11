#pragma once

#include <clife/world/calculation.hpp>
#include <clife/world/phenotype.hpp>

#include <vector>

namespace clife::world {

enum class RuntimeRuleInputKind { end_residual, runtime_value, object_characteristic };

struct RuntimeRuleInputBinding final {
    CalculationPortId input;
    RuntimeRuleInputKind kind;
    ValueId value{};
    ObjectCharacteristicId characteristic{};
};

struct RuntimeRuleOutputBinding final {
    CalculationPortId output;
    ValueId target;
};

struct RuntimeWorldRule final {
    ValueId source;
    CalculationDefinition calculation;
    std::vector<RuntimeRuleInputBinding> inputs;
    std::vector<RuntimeRuleOutputBinding> outputs;
};

class RuntimeRuleExecutor final {
public:
    RuntimeRuleExecutor() = default;
    explicit RuntimeRuleExecutor(std::vector<RuntimeWorldRule> rules);

    void apply(Calculator& calculator, const CompiledPhenotype& phenotype) const;

private:
    std::vector<RuntimeWorldRule> rules_;
};

} // namespace clife::world
