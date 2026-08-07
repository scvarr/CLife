#include <clife/core/cell.hpp>
#include <clife/core/simulation.hpp>

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

constexpr clife::Tick kDefaultTicks{5};
constexpr int kValueWidth{16};

struct LabConfig final {
    clife::Tick ticks{kDefaultTicks};
    std::vector<clife::Amount> fields;
    std::vector<clife::FieldType> configured_fields;
    clife::CellPhenotype phenotype;
};

struct CommandLine final {
    LabConfig config;
    bool show_help{false};
};

[[nodiscard]] LabConfig make_default_config()
{
    LabConfig config;
    config.fields = {0.0, 1.0};
    config.configured_fields = {clife::FieldType{1}};
    config.phenotype.transforms = {
        {.input = clife::FieldType{1}, .output = clife::ResourceType{7}, .throughput = 1.0},
    };
    config.phenotype.stores = {
        {.resource = clife::ResourceType{7}, .capacity = 0.25},
    };
    config.phenotype.remainders = {
        {.resource = clife::ResourceType{7}, .state = clife::StateType{2}},
    };
    return config;
}

[[nodiscard]] bool is_cell_definition_option(std::string_view argument) noexcept
{
    return argument == "--field" || argument == "--transform" || argument == "--store" || argument == "--remainder";
}

[[nodiscard]] bool has_custom_cell_definition(int argc, char* argv[]) noexcept
{
    for (int index = 1; index < argc; ++index) {
        if (is_cell_definition_option(argv[index])) {
            return true;
        }
    }

    return false;
}

[[nodiscard]] std::string_view require_value(int argc, char* argv[], int& index, std::string_view option)
{
    if (index + 1 >= argc) {
        throw std::invalid_argument{std::string{option} + " requires a value"};
    }

    ++index;
    return argv[index];
}

[[nodiscard]] clife::TypeIndex parse_type_index(std::string_view text, std::string_view label)
{
    clife::TypeIndex value{};
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto [parsed_end, error] = std::from_chars(begin, end, value);

    if (error != std::errc{} || parsed_end != end) {
        throw std::invalid_argument{"invalid " + std::string{label} + ": " + std::string{text}};
    }

    return value;
}

[[nodiscard]] clife::Tick parse_tick_count(std::string_view text)
{
    clife::Tick value{};
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto [parsed_end, error] = std::from_chars(begin, end, value);

    if (error != std::errc{} || parsed_end != end) {
        throw std::invalid_argument{"invalid tick count: " + std::string{text}};
    }

    return value;
}

[[nodiscard]] clife::Amount parse_amount(std::string_view text, std::string_view label)
{
    const std::string copy{text};
    char* parsed_end = nullptr;
    errno = 0;
    const double value = std::strtod(copy.c_str(), &parsed_end);

    if (errno == ERANGE || parsed_end != copy.c_str() + copy.size() || !std::isfinite(value)) {
        throw std::invalid_argument{"invalid " + std::string{label} + ": " + copy};
    }

    return value;
}

[[nodiscard]] std::pair<std::string_view, std::string_view> split_pair(std::string_view text, char delimiter,
                                                                        std::string_view label)
{
    const std::size_t delimiter_position = text.find(delimiter);
    if (delimiter_position == std::string_view::npos || text.find(delimiter, delimiter_position + 1) != std::string_view::npos) {
        throw std::invalid_argument{"invalid " + std::string{label} + ": " + std::string{text}};
    }

    const std::string_view first = text.substr(0, delimiter_position);
    const std::string_view second = text.substr(delimiter_position + 1);
    if (first.empty() || second.empty()) {
        throw std::invalid_argument{"invalid " + std::string{label} + ": " + std::string{text}};
    }

    return {first, second};
}

struct ThreeParts final {
    std::string_view first;
    std::string_view second;
    std::string_view third;
};

[[nodiscard]] ThreeParts split_three(std::string_view text, char delimiter, std::string_view label)
{
    const std::size_t first_delimiter = text.find(delimiter);
    const std::size_t second_delimiter =
        first_delimiter == std::string_view::npos ? std::string_view::npos : text.find(delimiter, first_delimiter + 1);

    if (first_delimiter == std::string_view::npos || second_delimiter == std::string_view::npos ||
        text.find(delimiter, second_delimiter + 1) != std::string_view::npos) {
        throw std::invalid_argument{"invalid " + std::string{label} + ": " + std::string{text}};
    }

    const ThreeParts parts{
        .first = text.substr(0, first_delimiter),
        .second = text.substr(first_delimiter + 1, second_delimiter - first_delimiter - 1),
        .third = text.substr(second_delimiter + 1),
    };

    if (parts.first.empty() || parts.second.empty() || parts.third.empty()) {
        throw std::invalid_argument{"invalid " + std::string{label} + ": " + std::string{text}};
    }

    return parts;
}

template <typename Type>
void append_unique(std::vector<Type>& values, Type value)
{
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

void set_field(LabConfig& config, std::string_view definition)
{
    const auto [type_text, amount_text] = split_pair(definition, '=', "field definition");
    const clife::FieldType field{parse_type_index(type_text, "field type")};
    const clife::Amount amount = parse_amount(amount_text, "field amount");
    if (amount < 0.0) {
        throw std::invalid_argument{"field amount must be non-negative"};
    }

    const auto index = static_cast<std::size_t>(field.index);
    if (index >= config.fields.size()) {
        config.fields.resize(index + 1, 0.0);
    }

    config.fields[index] = amount;
    append_unique(config.configured_fields, field);
}

void add_transform(LabConfig& config, std::string_view definition)
{
    const ThreeParts parts = split_three(definition, ':', "transform definition");
    const clife::Amount throughput = parse_amount(parts.third, "transform throughput");
    if (throughput != 1.0) {
        throw std::invalid_argument{"current core requires transform throughput to be exactly 1"};
    }

    config.phenotype.transforms.push_back({
        .input = clife::FieldType{parse_type_index(parts.first, "transform field type")},
        .output = clife::ResourceType{parse_type_index(parts.second, "transform resource type")},
        .throughput = throughput,
    });
}

void add_store(LabConfig& config, std::string_view definition)
{
    const auto [resource_text, capacity_text] = split_pair(definition, ':', "store definition");
    const clife::Amount capacity = parse_amount(capacity_text, "store capacity");
    if (capacity < 0.0) {
        throw std::invalid_argument{"store capacity must be non-negative"};
    }

    config.phenotype.stores.push_back({
        .resource = clife::ResourceType{parse_type_index(resource_text, "store resource type")},
        .capacity = capacity,
    });
}

void add_remainder(LabConfig& config, std::string_view definition)
{
    const auto [resource_text, state_text] = split_pair(definition, ':', "remainder definition");
    config.phenotype.remainders.push_back({
        .resource = clife::ResourceType{parse_type_index(resource_text, "remainder resource type")},
        .state = clife::StateType{parse_type_index(state_text, "remainder state type")},
    });
}

[[nodiscard]] CommandLine parse_command_line(int argc, char* argv[])
{
    CommandLine command{
        .config = has_custom_cell_definition(argc, argv) ? LabConfig{} : make_default_config(),
    };

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];

        if (argument == "--help" || argument == "-h") {
            command.show_help = true;
        } else if (argument == "--ticks") {
            command.config.ticks = parse_tick_count(require_value(argc, argv, index, argument));
        } else if (argument == "--field") {
            set_field(command.config, require_value(argc, argv, index, argument));
        } else if (argument == "--transform") {
            add_transform(command.config, require_value(argc, argv, index, argument));
        } else if (argument == "--store") {
            add_store(command.config, require_value(argc, argv, index, argument));
        } else if (argument == "--remainder") {
            add_remainder(command.config, require_value(argc, argv, index, argument));
        } else {
            throw std::invalid_argument{"unknown option: " + std::string{argument}};
        }
    }

    return command;
}

void print_help()
{
    std::cout << "CLife headless cell lab\n\n"
              << "Usage:\n"
              << "  clife_headless [options]\n\n"
              << "Options:\n"
              << "  --ticks N                         Number of simulation ticks\n"
              << "  --field FIELD=AMOUNT              Constant field input for every tick\n"
              << "  --transform FIELD:RESOURCE:RATE   Field -> Resource transform\n"
              << "  --store RESOURCE:CAPACITY         Persistent resource store\n"
              << "  --remainder RESOURCE:STATE        Unstored resource -> cell state\n"
              << "  --help, -h                        Show this help\n\n"
              << "Cell-definition options may be repeated. If none are supplied, the built-in\n"
              << "Field[1] -> Resource[7] -> Store/State[2] demonstration is used.\n\n"
              << "Example:\n"
              << "  clife_headless --ticks 5 --field 1=1 --transform 1:7:1 "
                 "--store 7:0.25 --remainder 7:2\n";
}

[[nodiscard]] std::vector<clife::FieldType> observed_fields(const LabConfig& config)
{
    std::vector<clife::FieldType> fields = config.configured_fields;
    for (const clife::FieldToResourceTransform& transform : config.phenotype.transforms) {
        append_unique(fields, transform.input);
    }
    return fields;
}

[[nodiscard]] std::vector<clife::ResourceType> observed_resources(const LabConfig& config)
{
    std::vector<clife::ResourceType> resources;
    for (const clife::FieldToResourceTransform& transform : config.phenotype.transforms) {
        append_unique(resources, transform.output);
    }
    for (const clife::Store& store : config.phenotype.stores) {
        append_unique(resources, store.resource);
    }
    return resources;
}

[[nodiscard]] std::vector<clife::StateType> observed_states(const LabConfig& config)
{
    std::vector<clife::StateType> states;
    for (const clife::RemainderToState& remainder : config.phenotype.remainders) {
        append_unique(states, remainder.state);
    }
    return states;
}

void print_configuration(const LabConfig& config)
{
    std::cout << "CLife cell lab\n\n";
    std::cout << "ticks = " << config.ticks << '\n';

    for (const clife::FieldType field : config.configured_fields) {
        const auto index = static_cast<std::size_t>(field.index);
        const clife::Amount amount = index < config.fields.size() ? config.fields[index] : 0.0;
        std::cout << "Field[" << field.index << "] = " << amount << '\n';
    }

    for (const clife::FieldToResourceTransform& transform : config.phenotype.transforms) {
        std::cout << "Transform: Field[" << transform.input.index << "] -> Resource[" << transform.output.index
                  << "], throughput=" << transform.throughput << '\n';
    }

    for (const clife::Store& store : config.phenotype.stores) {
        std::cout << "Store: Resource[" << store.resource.index << "], capacity=" << store.capacity << '\n';
    }

    for (const clife::RemainderToState& remainder : config.phenotype.remainders) {
        std::cout << "Remainder: Resource[" << remainder.resource.index << "] -> State[" << remainder.state.index
                  << "]\n";
    }

    std::cout << '\n';
}

[[nodiscard]] std::string field_label(clife::FieldType type)
{
    return "field[" + std::to_string(type.index) + "]";
}

[[nodiscard]] std::string resource_label(clife::ResourceType type)
{
    return "stored[" + std::to_string(type.index) + "]";
}

[[nodiscard]] std::string state_label(clife::StateType type)
{
    return "state[" + std::to_string(type.index) + "]";
}

void print_table_header(const std::vector<clife::FieldType>& fields, const std::vector<clife::ResourceType>& resources,
                        const std::vector<clife::StateType>& states)
{
    std::cout << std::setw(8) << "tick";
    for (const clife::FieldType field : fields) {
        std::cout << std::setw(kValueWidth) << field_label(field);
    }
    for (const clife::ResourceType resource : resources) {
        std::cout << std::setw(kValueWidth) << resource_label(resource);
    }
    for (const clife::StateType state : states) {
        std::cout << std::setw(kValueWidth) << state_label(state);
    }
    std::cout << '\n';
}

void print_table_row(const clife::Simulation& simulation, const clife::CellInputs inputs, const clife::Cell& cell,
                     const std::vector<clife::FieldType>& fields, const std::vector<clife::ResourceType>& resources,
                     const std::vector<clife::StateType>& states)
{
    std::cout << std::setw(8) << simulation.tick();
    for (const clife::FieldType field : fields) {
        std::cout << std::setw(kValueWidth) << inputs.field(field);
    }
    for (const clife::ResourceType resource : resources) {
        std::cout << std::setw(kValueWidth) << cell.stored(resource);
    }
    for (const clife::StateType state : states) {
        std::cout << std::setw(kValueWidth) << cell.state(state);
    }
    std::cout << '\n';
}

int run_headless(int argc, char* argv[])
{
    const CommandLine command = parse_command_line(argc, argv);
    if (command.show_help) {
        print_help();
        return EXIT_SUCCESS;
    }

    const LabConfig& config = command.config;
    print_configuration(config);

    clife::Cell cell{config.phenotype};
    clife::Simulation simulation;
    const clife::CellInputs inputs{.fields = config.fields};

    const std::vector<clife::FieldType> fields = observed_fields(config);
    const std::vector<clife::ResourceType> resources = observed_resources(config);
    const std::vector<clife::StateType> states = observed_states(config);

    std::cout << std::fixed << std::setprecision(3);
    print_table_header(fields, resources, states);
    print_table_row(simulation, inputs, cell, fields, resources, states);

    for (clife::Tick tick = 0; tick < config.ticks; ++tick) {
        cell.step(inputs);
        simulation.step();
        print_table_row(simulation, inputs, cell, fields, resources, states);
    }

    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char* argv[])
{
    try {
        return run_headless(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "CLife headless fatal error: " << error.what() << '\n';
    } catch (...) {
        std::cerr << "CLife headless fatal error: unknown exception\n";
    }

    return EXIT_FAILURE;
}
