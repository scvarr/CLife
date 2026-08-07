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
    [[nodiscard]] Amount state(StateType state) const noexcept;

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
};

} // namespace clife
