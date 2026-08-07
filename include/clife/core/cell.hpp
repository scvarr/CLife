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

struct StateType final {
    TypeIndex index;

    friend constexpr bool operator==(StateType, StateType) noexcept = default;
};

struct MatterType final {
    TypeIndex index;

    friend constexpr bool operator==(MatterType, MatterType) noexcept = default;
};

struct MatterDefinition final {
    MatterType type;
    Amount volume_per_unit;
    Amount heat_capacity_per_unit;
};

struct MatterAmount final {
    MatterType type;
    Amount amount;
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

struct RemainderToState final {
    ResourceType resource;
    StateType state;
};

struct CellPhenotype final {
    std::vector<FieldToResourceTransform> transforms;
    std::vector<Store> stores;
    std::vector<RemainderToState> remainders;
    std::vector<MatterAmount> composition;
};

struct CellInputs final {
    std::span<const Amount> fields{};

    [[nodiscard]] Amount field(FieldType type) const noexcept;
};

class Cell final {
public:
    explicit Cell(CellPhenotype phenotype, std::span<const MatterDefinition> matter_definitions = {});

    void step(CellInputs inputs) noexcept;

    [[nodiscard]] Amount stored(ResourceType resource) const noexcept;
    [[nodiscard]] Amount state(StateType state) const noexcept;
    [[nodiscard]] Amount matter(MatterType type) const noexcept;
    [[nodiscard]] Amount volume() const noexcept;
    [[nodiscard]] Amount heat_capacity() const noexcept;

private:
    struct ResourceStorage final {
        ResourceType resource;
        Amount capacity;
        Amount amount{0.0};
    };

    struct TickResource final {
        ResourceType resource;
        Amount amount{0.0};
    };

    struct StateValue final {
        StateType state;
        Amount amount{0.0};
    };

    [[nodiscard]] Amount total_demand(FieldType field) const noexcept;
    [[nodiscard]] const RemainderToState* find_remainder(ResourceType resource) const noexcept;
    [[nodiscard]] TickResource* find_tick_resource(ResourceType resource) noexcept;
    [[nodiscard]] StateValue* find_state(StateType state) noexcept;
    [[nodiscard]] const StateValue* find_state(StateType state) const noexcept;
    void produce(ResourceType resource, Amount amount) noexcept;
    [[nodiscard]] Amount take(ResourceType resource, Amount requested) noexcept;
    void add_state(StateType state, Amount amount) noexcept;

    CellPhenotype phenotype_;
    std::vector<ResourceStorage> storage_;
    std::vector<TickResource> tick_resources_;
    std::vector<StateValue> states_;
    Amount volume_{0.0};
    Amount heat_capacity_{0.0};
};

} // namespace clife
