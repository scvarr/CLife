#pragma once

#include <clife/world/definition.hpp>

#include <array>
#include <span>

namespace clife::world {

class ShapePhenotype final {
public:
    static constexpr std::size_t coefficient_count = 8;

    [[nodiscard]] std::span<const Amount, coefficient_count> coefficients() const noexcept;
    [[nodiscard]] Amount radius(Amount x, Amount y, Amount z) const;

private:
    friend ShapePhenotype compile_semantic_shape_phenotype(const WorldDefinition& definition, TemplateId source_template);

    std::array<Amount, coefficient_count> coefficients_{};
    Amount normalization_scale_{1.0};
};

[[nodiscard]] ShapePhenotype compile_semantic_shape_phenotype(const WorldDefinition& definition,
                                                               TemplateId source_template);

} // namespace clife::world
