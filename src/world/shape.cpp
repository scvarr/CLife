#include <clife/world/shape.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace clife::world {
namespace {

constexpr Amount kFieldLimit = 0.9;
constexpr Amount kCoefficientLimit = 0.15;
constexpr std::size_t kNormalizationLatitudeSamples = 96;
constexpr std::size_t kNormalizationLongitudeSamples = 192;
constexpr Amount kPi = 3.141592653589793238462643383279502884;

[[nodiscard]] std::uint64_t mix(std::uint64_t value) noexcept
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

[[nodiscard]] std::uint64_t combine(std::uint64_t state, std::uint64_t value) noexcept
{
    return mix(state ^ (mix(value) + 0x9e3779b97f4a7c15ULL + (state << 6U) + (state >> 2U)));
}

[[nodiscard]] std::uint64_t amount_bits(Amount amount) noexcept
{
    return amount == 0.0 ? 0U : std::bit_cast<std::uint64_t>(amount);
}

[[nodiscard]] Amount signed_unit(std::uint64_t value) noexcept
{
    constexpr Amount kInverseMantissa = 1.0 / 9007199254740992.0;
    return static_cast<Amount>(value >> 11U) * kInverseMantissa * 2.0 - 1.0;
}

[[nodiscard]] Amount raw_radius(const std::array<Amount, ShapePhenotype::coefficient_count>& coefficients,
                                Amount x, Amount y, Amount z)
{
    const std::array<Amount, ShapePhenotype::coefficient_count> basis{
        x, y, z, 2.0 * x * y, 2.0 * y * z, 2.0 * z * x, x * x - y * y, 0.5 * (3.0 * z * z - 1.0),
    };
    Amount field{};
    for (std::size_t index = 0; index < coefficients.size(); ++index) {
        field += coefficients[index] * basis[index];
    }
    return std::exp(std::clamp(field, -kFieldLimit, kFieldLimit));
}

[[nodiscard]] Amount raw_volume(const std::array<Amount, ShapePhenotype::coefficient_count>& coefficients)
{
    const Amount z_step = 2.0 / static_cast<Amount>(kNormalizationLatitudeSamples);
    const Amount phi_step = 2.0 * kPi / static_cast<Amount>(kNormalizationLongitudeSamples);
    Amount integral{};
    for (std::size_t latitude = 0; latitude < kNormalizationLatitudeSamples; ++latitude) {
        const Amount z = -1.0 + (static_cast<Amount>(latitude) + 0.5) * z_step;
        const Amount radial = std::sqrt(1.0 - z * z);
        for (std::size_t longitude = 0; longitude < kNormalizationLongitudeSamples; ++longitude) {
            const Amount phi = (static_cast<Amount>(longitude) + 0.5) * phi_step;
            const Amount radius = raw_radius(coefficients, radial * std::cos(phi), z, radial * std::sin(phi));
            integral += radius * radius * radius;
        }
    }
    return integral * z_step * phi_step / 3.0;
}

} // namespace

std::span<const Amount, ShapePhenotype::coefficient_count> ShapePhenotype::coefficients() const noexcept
{
    return coefficients_;
}

Amount ShapePhenotype::radius(Amount x, Amount y, Amount z) const
{
    const Amount length_squared = x * x + y * y + z * z;
    if (!std::isfinite(length_squared) || length_squared <= 0.0) {
        throw std::invalid_argument{"shape direction must be finite and non-zero"};
    }
    const Amount inverse_length = 1.0 / std::sqrt(length_squared);
    const Amount result = normalization_scale_ * raw_radius(coefficients_, x * inverse_length, y * inverse_length,
                                                             z * inverse_length);
    if (!std::isfinite(result) || result <= 0.0) {
        throw std::overflow_error{"shape radius is invalid"};
    }
    return result;
}

ShapePhenotype compile_semantic_shape_phenotype(const WorldDefinition& definition, TemplateId source_template)
{
    const ObjectTemplate& object = definition.object_template(source_template);
    ShapePhenotype phenotype;
    std::array<Amount, ShapePhenotype::coefficient_count> accumulated{};
    for (std::size_t entry_index = 0; entry_index < object.genome.size(); ++entry_index) {
        const GenomeFunctionInstance& entry = object.genome[entry_index];
        std::uint64_t entry_seed = combine(static_cast<std::uint64_t>(entry_index), entry.type.value);
        for (std::size_t coefficient = 0; coefficient < accumulated.size(); ++coefficient) {
            accumulated[coefficient] += signed_unit(combine(entry_seed, coefficient));
        }
        for (const ParameterValue& parameter : entry.parameters) {
            if (!std::isfinite(parameter.value)) {
                throw std::invalid_argument{"semantic shape projection requires finite genome parameters"};
            }
            std::uint64_t parameter_seed = combine(entry_seed, parameter.parameter.value);
            parameter_seed = combine(parameter_seed, amount_bits(parameter.value));
            for (std::size_t coefficient = 0; coefficient < accumulated.size(); ++coefficient) {
                accumulated[coefficient] += signed_unit(combine(parameter_seed, coefficient));
            }
        }
    }
    for (std::size_t coefficient = 0; coefficient < phenotype.coefficients_.size(); ++coefficient) {
        phenotype.coefficients_[coefficient] = kCoefficientLimit * std::tanh(accumulated[coefficient]);
    }
    const Amount volume = raw_volume(phenotype.coefficients_);
    if (!std::isfinite(volume) || volume <= std::numeric_limits<Amount>::min()) {
        throw std::overflow_error{"semantic shape normalization volume is invalid"};
    }
    phenotype.normalization_scale_ = std::cbrt(1.0 / volume);
    if (!std::isfinite(phenotype.normalization_scale_) || phenotype.normalization_scale_ <= 0.0) {
        throw std::overflow_error{"semantic shape normalization scale is invalid"};
    }
    return phenotype;
}

} // namespace clife::world
