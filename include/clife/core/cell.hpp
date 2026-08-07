#pragma once

#include <cstdint>
#include <span>

namespace clife {

using Amount = double;
using TypeIndex = std::uint32_t;

struct FieldType final {
    TypeIndex index;

    friend constexpr bool operator==(FieldType, FieldType) noexcept = default;
};

struct ResourceType final {
    TypeIndex index;

    friend constexpr bool operator==(ResourceType, ResourceType) noexcept = default;
};

struct FieldToResourceTransform final {
    FieldType input;
    ResourceType output;
    Amount throughput;
};

struct Store final {
    ResourceType resource;
    Amount capacity;
};

struct CellPhenotype final {
    FieldToResourceTransform transform;
    Store store;
};

struct CellInputs final {
    std::span<const Amount> fields{};

    [[nodiscard]] Amount field(FieldType type) const noexcept;
};

class Cell final {
public:
    explicit Cell(CellPhenotype phenotype) noexcept;

    void step(CellInputs inputs) noexcept;

    [[nodiscard]] Amount stored(ResourceType resource) const noexcept;

private:
    CellPhenotype phenotype_;
    Amount stored_amount_{0.0};
};

} // namespace clife
