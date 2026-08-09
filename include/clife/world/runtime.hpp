#pragma once

#include <clife/world/definition.hpp>
#include <clife/world/phenotype.hpp>

#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

namespace clife::world {

struct ObjectId final {
    std::uint64_t value{};

    friend constexpr auto operator<=>(ObjectId, ObjectId) noexcept = default;
};

struct RuntimeFunctionState final {
    std::size_t function_index;
    FunctionTypeId type;
    std::optional<BufferState> buffer;
};

class RuntimeWorld final {
public:
    explicit RuntimeWorld(const WorldDefinition& definition);

    [[nodiscard]] ObjectId instantiate(TemplateId source_template);

    void set_input(ObjectId object, ValueKey value, Amount amount);
    void set_input(ObjectId object, std::string_view channel, Amount amount);
    void step();

    [[nodiscard]] Amount value(ObjectId object, ValueKey value) const;
    [[nodiscard]] Amount output(ObjectId object, std::string_view channel) const;
    [[nodiscard]] TemplateId source_template(ObjectId object) const;
    [[nodiscard]] const CompiledPhenotype& phenotype(ObjectId object) const;
    [[nodiscard]] std::vector<RuntimeFunctionState> function_states(ObjectId object) const;
    [[nodiscard]] Amount last_end_value(ObjectId object, ValueKey value) const;
    [[nodiscard]] std::optional<ValueId> runtime_value_id(ValueKey key) const noexcept;
    [[nodiscard]] std::size_t object_count() const noexcept;

private:
    struct CompiledTemplate final {
        TemplateId source;
        CompiledPhenotype phenotype;
        Program program;
        std::vector<std::optional<std::size_t>> buffer_indices;
        std::vector<HostBinding> bindings;
    };

    struct RuntimeObject final {
        ObjectId id;
        TemplateId source;
        Calculator calculator;
        std::vector<HostBinding> bindings;
        std::map<ValueKey, Amount> staged_inputs;
    };

    [[nodiscard]] ValueId require_value_id(ValueKey key) const;
    [[nodiscard]] const CompiledTemplate& compiled_template(TemplateId id) const;
    [[nodiscard]] RuntimeObject& object(ObjectId id);
    [[nodiscard]] const RuntimeObject& object(ObjectId id) const;

    std::vector<std::pair<ValueKey, ValueId>> value_ids_;
    std::vector<CompiledTemplate> templates_;
    std::vector<RuntimeObject> objects_;
    std::uint64_t next_object_id_{1};
};

} // namespace clife::world
