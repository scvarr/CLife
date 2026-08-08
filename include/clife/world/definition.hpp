#pragma once

#include <clife/core/calculator.hpp>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace clife::world {

struct ValueKey final {
    std::uint32_t value{};

    friend constexpr auto operator<=>(ValueKey, ValueKey) noexcept = default;
};

struct TemplateId final {
    std::uint32_t value{};

    friend constexpr auto operator<=>(TemplateId, TemplateId) noexcept = default;
};

struct ValueDefinition final {
    ValueKey key;
    std::string name;
};

struct GenomeFunctionDefinition final {
    ValueKey input;
    ValueKey output;
    Amount throughput;
    Amount result_per_input{1.0};
};

struct InitialValueDefinition final {
    ValueKey value;
    Amount amount;
};

enum class HostChannelDirection {
    input,
    output,
};

struct HostBinding final {
    std::string channel;
    HostChannelDirection direction;
    ValueKey value;
};

struct ObjectTemplate final {
    TemplateId id;
    std::string name;
    std::vector<InitialValueDefinition> initial_values;
    std::vector<GenomeFunctionDefinition> genome;
    std::vector<HostBinding> host_bindings;
};

struct WorldRuleDefinition final {
    ValueKey source;
    ValueKey target;
    Amount target_per_source;
};

class WorldDefinition final {
public:
    [[nodiscard]] ValueKey add_value(std::string name);
    void rename_value(ValueKey key, std::string name);
    void remove_value(ValueKey key);
    void reorder_values(std::span<const ValueKey> order);

    [[nodiscard]] TemplateId add_template(std::string name);
    void rename_template(TemplateId id, std::string name);
    void remove_template(TemplateId id);
    void set_initial_value(TemplateId id, ValueKey value, Amount amount);
    void remove_initial_value(TemplateId id, ValueKey value);

    [[nodiscard]] std::size_t add_genome_function(TemplateId id, GenomeFunctionDefinition function);
    void change_genome_function(TemplateId id, std::size_t index, GenomeFunctionDefinition function);
    void remove_genome_function(TemplateId id, std::size_t index);

    [[nodiscard]] std::size_t add_world_rule(WorldRuleDefinition rule);
    void change_world_rule(std::size_t index, WorldRuleDefinition rule);
    void remove_world_rule(std::size_t index);

    [[nodiscard]] std::size_t add_host_binding(TemplateId id, HostBinding binding);
    void change_host_binding(TemplateId id, std::size_t index, HostBinding binding);
    void remove_host_binding(TemplateId id, std::size_t index);

    [[nodiscard]] const std::vector<ValueDefinition>& values() const noexcept;
    [[nodiscard]] const std::vector<ObjectTemplate>& templates() const noexcept;
    [[nodiscard]] const std::vector<WorldRuleDefinition>& world_rules() const noexcept;
    [[nodiscard]] const ValueDefinition& value(ValueKey key) const;
    [[nodiscard]] const ObjectTemplate& object_template(TemplateId id) const;

private:
    [[nodiscard]] ObjectTemplate& mutable_template(TemplateId id);
    void validate_function(const GenomeFunctionDefinition& function) const;
    void validate_rule(const WorldRuleDefinition& rule, std::size_t ignored_index) const;
    void validate_binding(const ObjectTemplate& object, const HostBinding& binding, std::size_t ignored_index) const;

    std::vector<ValueDefinition> values_;
    std::vector<ObjectTemplate> templates_;
    std::vector<WorldRuleDefinition> world_rules_;
    std::uint32_t next_value_key_{1};
    std::uint32_t next_template_id_{1};
};

} // namespace clife::world
