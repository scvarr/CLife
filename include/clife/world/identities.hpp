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

} // namespace clife::world
