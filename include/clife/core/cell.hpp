#pragma once

#include <cstdint>
#include <span>
#include <vector>

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
    std::vector<FieldToResourceTransform> transforms;
    std::vector<Store> stores;
};

struct CellInputs final {
    std::span<const Amount> fields{};

    [[nodiscard]] Amount field(FieldType type) const noexcept;
};

class Cell final {
public:
    explicit Cell(CellPhenotype phenotype);

    void step(CellInputs inputs) noexcept;

    [[nodiscard]] Amount stored(ResourceType resource) const noexcept;

private:
    struct ResourceStorage final {
        ResourceType resource;
        Amount capacity;
        Amount amount{0.0};
    };

    [[nodiscard]] Amount total_demand(FieldType field) const noexcept;
    [[nodiscard]] ResourceStorage* find_storage(ResourceType resource) noexcept;
    [[nodiscard]] const ResourceStorage* find_storage(ResourceType resource) const noexcept;
    void store(ResourceType resource, Amount amount) noexcept;

    CellPhenotype phenotype_;
    std::vector<ResourceStorage> storage_;
};

} // namespace clife
