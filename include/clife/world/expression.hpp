#pragma once

#include <clife/core/calculator.hpp>
#include <clife/world/identities.hpp>

#include <span>
#include <string_view>
#include <vector>

namespace clife::world {

struct ParameterValue final {
    ParameterId parameter;
    Amount value;
};

struct ParameterName final {
    ParameterId parameter;
    std::string_view name;
};

class Expression final {
public:
    enum class Operation {
        literal,
        parameter,
        add,
        subtract,
        multiply,
        divide,
        minimum,
        maximum,
        negate,
    };

    struct Instruction final {
        Operation operation;
        Amount literal{};
        ParameterId parameter{};
    };

    Expression() = default;
    explicit Expression(std::vector<Instruction> instructions);

    [[nodiscard]] Amount evaluate(std::span<const ParameterValue> parameters) const;
    [[nodiscard]] bool empty() const noexcept;

private:
    std::vector<Instruction> instructions_;
};

[[nodiscard]] Expression compile_expression(std::string_view source, std::span<const ParameterName> parameters);

} // namespace clife::world
