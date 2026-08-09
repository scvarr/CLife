#pragma once

#include <compare>
#include <cstdint>

namespace clife::world {

struct ValueKey final {
    std::uint32_t value{};

    friend constexpr auto operator<=>(ValueKey, ValueKey) noexcept = default;
};

struct TemplateId final {
    std::uint32_t value{};

    friend constexpr auto operator<=>(TemplateId, TemplateId) noexcept = default;
};

struct FunctionTypeId final {
    std::uint32_t value{};

    friend constexpr auto operator<=>(FunctionTypeId, FunctionTypeId) noexcept = default;
};

struct ParameterId final {
    std::uint32_t value{};

    friend constexpr auto operator<=>(ParameterId, ParameterId) noexcept = default;
};

struct CalculationId final {
    std::uint32_t value{};

    friend constexpr auto operator<=>(CalculationId, CalculationId) noexcept = default;
};

struct CalculationPortId final {
    std::uint32_t value{};

    friend constexpr auto operator<=>(CalculationPortId, CalculationPortId) noexcept = default;
};

struct UnitId final {
    std::uint32_t value{};

    friend constexpr auto operator<=>(UnitId, UnitId) noexcept = default;
};

struct UnitConversionId final {
    std::uint32_t value{};

    friend constexpr auto operator<=>(UnitConversionId, UnitConversionId) noexcept = default;
};

} // namespace clife::world
