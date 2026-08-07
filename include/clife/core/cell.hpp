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

struct PropertyType final {
    TypeIndex index;

    friend constexpr bool operator==(PropertyType, PropertyType) noexcept = default;
};

struct MeasureType final {
    TypeIndex index;

    friend constexpr bool operator==(MeasureType, MeasureType) noexcept = default;
};

struct UnitScale final {
    MeasureType measure;
    Amount measure_per_unit;
};

struct TypeProperty final {
    PropertyType property;
    Amount value;
};

struct FieldDefinition final {
    FieldType type;
    UnitScale unit;
    std::vector<TypeProperty> properties{};
};

struct ResourceDefinition final {
    ResourceType type;
    UnitScale unit;
    std::vector<TypeProperty> properties{};
};

struct StateDefinition final {
    StateType type;
    UnitScale unit;
    std::vector<TypeProperty> properties{};
};

struct MatterDefinition final {
    MatterType type;
    UnitScale unit;
    std::vector<TypeProperty> properties{};
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
    std::vector<FieldToResourceTransform> transforms{};
    std::vector<Store> stores{};
    std::vector<RemainderToState> remainders{};
    std::vector<MatterAmount> composition{};
};

struct TypeDefinitions final {
    std::span<const FieldDefinition> fields{};
    std::span<const ResourceDefinition> resources{};
    std::span<const StateDefinition> states{};
    std::span<const MatterDefinition> matters{};
};

struct CellInputs final {
    std::span<const Amount> fields{};

    [[nodiscard]] Amount field(FieldType type) const noexcept;
};

class Cell final {
public:
    explicit Cell(CellPhenotype phenotype, TypeDefinitions definitions = {});

    void step(CellInputs inputs) noexcept;

    [[nodiscard]] Amount stored(ResourceType resource) const noexcept;
    [[nodiscard]] Amount state(StateType state) const noexcept;
    [[nodiscard]] Amount matter(MatterType type) const noexcept;

private:
    struct RuntimeTransform final {
        FieldType input;
        ResourceType output;
        Amount throughput;
        Amount output_per_input;
    };

    struct RuntimeRemainder final {
        ResourceType resource;
        StateType state;
        Amount state_per_resource;
    };

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
    [[nodiscard]] const RuntimeRemainder* find_remainder(ResourceType resource) const noexcept;
    [[nodiscard]] TickResource* find_tick_resource(ResourceType resource) noexcept;
    [[nodiscard]] StateValue* find_state(StateType state) noexcept;
    [[nodiscard]] const StateValue* find_state(StateType state) const noexcept;
    void produce(ResourceType resource, Amount amount) noexcept;
    [[nodiscard]] Amount take(ResourceType resource, Amount requested) noexcept;
    void add_state(StateType state, Amount amount) noexcept;

    CellPhenotype phenotype_;
    std::vector<RuntimeTransform> transforms_;
    std::vector<RuntimeRemainder> remainders_;
    std::vector<ResourceStorage> storage_;
    std::vector<TickResource> tick_resources_;
    std::vector<StateValue> states_;
};

} // namespace clife
