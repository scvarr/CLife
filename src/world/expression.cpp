#include <clife/world/expression.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace clife::world {
namespace {

class Parser final {
public:
    Parser(std::string_view source, std::span<const ParameterName> parameters)
        : source_{source}, parameters_{parameters}
    {}

    [[nodiscard]] Expression parse()
    {
        if (source_.empty()) {
            throw std::invalid_argument{"expression must not be empty"};
        }
        parse_additive();
        skip_space();
        if (position_ != source_.size()) {
            throw std::invalid_argument{"unexpected token in expression"};
        }
        return Expression{std::move(instructions_)};
    }

private:
    void parse_additive()
    {
        parse_multiplicative();
        while (true) {
            if (take('+')) {
                parse_multiplicative();
                emit(Expression::Operation::add);
            } else if (take('-')) {
                parse_multiplicative();
                emit(Expression::Operation::subtract);
            } else {
                return;
            }
        }
    }

    void parse_multiplicative()
    {
        parse_unary();
        while (true) {
            if (take('*')) {
                parse_unary();
                emit(Expression::Operation::multiply);
            } else if (take('/')) {
                parse_unary();
                emit(Expression::Operation::divide);
            } else {
                return;
            }
        }
    }

    void parse_unary()
    {
        if (take('+')) {
            parse_unary();
            return;
        }
        if (take('-')) {
            parse_unary();
            emit(Expression::Operation::negate);
            return;
        }
        parse_primary();
    }

    void parse_primary()
    {
        skip_space();
        if (take('(')) {
            parse_additive();
            require(')');
            return;
        }
        if (position_ < source_.size() &&
            (std::isdigit(static_cast<unsigned char>(source_[position_])) != 0 || source_[position_] == '.')) {
            parse_number();
            return;
        }
        const std::string_view identifier = parse_identifier();
        if (identifier == "min" || identifier == "max") {
            require('(');
            parse_additive();
            require(',');
            parse_additive();
            require(')');
            emit(identifier == "min" ? Expression::Operation::minimum : Expression::Operation::maximum);
            return;
        }
        const auto found = std::ranges::find(parameters_, identifier, &ParameterName::name);
        if (found == parameters_.end()) {
            throw std::invalid_argument{"expression references an unknown parameter"};
        }
        instructions_.push_back({.operation = Expression::Operation::parameter, .parameter = found->parameter});
    }

    void parse_number()
    {
        const char* begin = source_.data() + position_;
        const char* end = source_.data() + source_.size();
        Amount value{};
        const auto parsed = std::from_chars(begin, end, value, std::chars_format::general);
        if (parsed.ec != std::errc{} || parsed.ptr == begin || !std::isfinite(value)) {
            throw std::invalid_argument{"invalid numeric literal in expression"};
        }
        position_ = static_cast<std::size_t>(parsed.ptr - source_.data());
        instructions_.push_back({.operation = Expression::Operation::literal, .literal = value});
    }

    [[nodiscard]] std::string_view parse_identifier()
    {
        skip_space();
        const std::size_t begin = position_;
        if (position_ >= source_.size() ||
            (std::isalpha(static_cast<unsigned char>(source_[position_])) == 0 && source_[position_] != '_')) {
            throw std::invalid_argument{"expected expression value"};
        }
        ++position_;
        while (position_ < source_.size() &&
               (std::isalnum(static_cast<unsigned char>(source_[position_])) != 0 || source_[position_] == '_')) {
            ++position_;
        }
        return source_.substr(begin, position_ - begin);
    }

    void emit(Expression::Operation operation) { instructions_.push_back({.operation = operation}); }

    [[nodiscard]] bool take(char token)
    {
        skip_space();
        if (position_ >= source_.size() || source_[position_] != token) {
            return false;
        }
        ++position_;
        return true;
    }

    void require(char token)
    {
        if (!take(token)) {
            throw std::invalid_argument{std::string{"expected '"} + token + "' in expression"};
        }
    }

    void skip_space()
    {
        while (position_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[position_])) != 0) {
            ++position_;
        }
    }

    std::string_view source_;
    std::span<const ParameterName> parameters_;
    std::size_t position_{};
    std::vector<Expression::Instruction> instructions_;
};

[[nodiscard]] Amount pop(std::vector<Amount>& stack)
{
    if (stack.empty()) {
        throw std::invalid_argument{"invalid compiled expression"};
    }
    const Amount value = stack.back();
    stack.pop_back();
    return value;
}

void require_finite(Amount value)
{
    if (!std::isfinite(value)) {
        throw std::invalid_argument{"expression produced a non-finite result"};
    }
}

} // namespace

Expression compile_expression(std::string_view source, std::span<const ParameterName> parameters)
{
    return Parser{source, parameters}.parse();
}

Expression::Expression(std::vector<Instruction> instructions) : instructions_{std::move(instructions)} {}

Amount Expression::evaluate(std::span<const ParameterValue> parameters) const
{
    std::vector<Amount> stack;
    stack.reserve(instructions_.size());
    for (const Instruction& instruction : instructions_) {
        if (instruction.operation == Operation::literal) {
            stack.push_back(instruction.literal);
            continue;
        }
        if (instruction.operation == Operation::parameter) {
            const auto found = std::ranges::find(parameters, instruction.parameter, &ParameterValue::parameter);
            if (found == parameters.end()) {
                throw std::invalid_argument{"expression parameter value is missing"};
            }
            require_finite(found->value);
            stack.push_back(found->value);
            continue;
        }
        const Amount right = pop(stack);
        Amount result{};
        if (instruction.operation == Operation::negate) {
            result = -right;
        } else {
            const Amount left = pop(stack);
            switch (instruction.operation) {
            case Operation::add:
                result = left + right;
                break;
            case Operation::subtract:
                result = left - right;
                break;
            case Operation::multiply:
                result = left * right;
                break;
            case Operation::divide:
                if (right == 0.0) {
                    throw std::invalid_argument{"expression division by zero"};
                }
                result = left / right;
                break;
            case Operation::minimum:
                result = std::min(left, right);
                break;
            case Operation::maximum:
                result = std::max(left, right);
                break;
            default:
                throw std::invalid_argument{"invalid compiled expression operation"};
            }
        }
        require_finite(result);
        stack.push_back(result);
    }
    if (stack.size() != 1) {
        throw std::invalid_argument{"invalid compiled expression"};
    }
    return stack.front();
}

bool Expression::empty() const noexcept { return instructions_.empty(); }

} // namespace clife::world
