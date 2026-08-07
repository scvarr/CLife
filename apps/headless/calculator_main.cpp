#include <clife/core/calculator.hpp>
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
constexpr int kValueWidth{18};

struct NamedValue final {
    clife::ValueId id;
    std::string name;
};

struct LabConfig final {
    clife::Tick ticks{kDefaultTicks};
    clife::Program program{};
    std::vector<clife::ValueAmount> external_values{};
    std::vector<clife::ValueId> observed_values{};
    std::vector<NamedValue> names{};
};

struct CommandLine final {
    LabConfig config;
    bool show_help{false};
};

[[nodiscard]] LabConfig make_default_config()
{
    constexpr clife::ValueId light{0};
    constexpr clife::ValueId energy{1};
    constexpr clife::ValueId used_energy{2};
    constexpr clife::ValueId temperature{3};

    LabConfig config;
    config.program.value_count = 4;
    config.program.functions = {
        {.input = light, .output = energy, .throughput = 1.0},
        {.input = energy, .output = used_energy, .throughput = 0.25},
    };
    config.program.end_rules = {
        {.source = energy, .target = temperature, .target_per_source = 0.1},
    };
    config.program.initial_values = {
        {.value = temperature, .amount = 0.2},
    };
    config.external_values = {
        {.value = light, .amount = 1.0},
    };
    config.observed_values = {used_energy, temperature};
    config.names = {
        {.id = light, .name = "Light"},
        {.id = energy, .name = "Energy"},
        {.id = used_energy, .name = "UsedEnergy"},
        {.id = temperature, .name = "Temperature"},
    };
    return config;
}

[[nodiscard]] bool is_structural_option(std::string_view argument) noexcept
{
    return argument == "--values" || argument == "--initial" || argument == "--external" ||
           argument == "--function" || argument == "--end-rule" || argument == "--observe";
}

[[nodiscard]] bool has_custom_program(int argc, char* argv[]) noexcept
{
    for (int index = 1; index < argc; ++index) {
        if (is_structural_option(argv[index])) {
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

[[nodiscard]] std::size_t parse_size(std::string_view text, std::string_view label)
{
    std::size_t value{};
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto [parsed_end, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || parsed_end != end) {
        throw std::invalid_argument{"invalid " + std::string{label} + ": " + std::string{text}};
    }
    return value;
}

[[nodiscard]] clife::ValueId parse_value_id(std::string_view text, std::string_view label)
{
    clife::ValueIndex value{};
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto [parsed_end, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || parsed_end != end) {
        throw std::invalid_argument{"invalid " + std::string{label} + ": " + std::string{text}};
    }
    return clife::ValueId{value};
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
    const std::size_t position = text.find(delimiter);
    if (position == std::string_view::npos || text.find(delimiter, position + 1) != std::string_view::npos) {
        throw std::invalid_argument{"invalid " + std::string{label} + ": " + std::string{text}};
    }
    const std::string_view first = text.substr(0, position);
    const std::string_view second = text.substr(position + 1);
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
    const std::size_t first = text.find(delimiter);
    const std::size_t second = first == std::string_view::npos ? std::string_view::npos : text.find(delimiter, first + 1);
    if (first == std::string_view::npos || second == std::string_view::npos ||
        text.find(delimiter, second + 1) != std::string_view::npos) {
        throw std::invalid_argument{"invalid " + std::string{label} + ": " + std::string{text}};
    }
    const ThreeParts parts{
        .first = text.substr(0, first),
        .second = text.substr(first + 1, second - first - 1),
        .third = text.substr(second + 1),
    };
    if (parts.first.empty() || parts.second.empty() || parts.third.empty()) {
        throw std::invalid_argument{"invalid " + std::string{label} + ": " + std::string{text}};
    }
    return parts;
}

struct FourParts final {
    std::string_view first;
    std::string_view second;
    std::string_view third;
    std::string_view fourth;
};

[[nodiscard]] FourParts split_four(std::string_view text, char delimiter, std::string_view label)
{
    const std::size_t first = text.find(delimiter);
    const std::size_t second = first == std::string_view::npos ? std::string_view::npos : text.find(delimiter, first + 1);
    const std::size_t third = second == std::string_view::npos ? std::string_view::npos : text.find(delimiter, second + 1);
    if (first == std::string_view::npos || second == std::string_view::npos || third == std::string_view::npos ||
        text.find(delimiter, third + 1) != std::string_view::npos) {
        throw std::invalid_argument{"invalid " + std::string{label} + ": " + std::string{text}};
    }
    const FourParts parts{
        .first = text.substr(0, first),
        .second = text.substr(first + 1, second - first - 1),
        .third = text.substr(second + 1, third - second - 1),
        .fourth = text.substr(third + 1),
    };
    if (parts.first.empty() || parts.second.empty() || parts.third.empty() || parts.fourth.empty()) {
        throw std::invalid_argument{"invalid " + std::string{label} + ": " + std::string{text}};
    }
    return parts;
}

void set_name(LabConfig& config, std::string_view text)
{
    const auto [id_text, name] = split_pair(text, ':', "name definition");
    const clife::ValueId id = parse_value_id(id_text, "named value");
    const auto found = std::find_if(config.names.begin(), config.names.end(), [id](const NamedValue& entry) {
        return entry.id == id;
    });
    if (found == config.names.end()) {
        config.names.push_back({.id = id, .name = std::string{name}});
    } else {
        found->name = std::string{name};
    }
}

void add_value_amount(std::vector<clife::ValueAmount>& values, std::string_view text, std::string_view label)
{
    const auto [id_text, amount_text] = split_pair(text, '=', label);
    values.push_back({
        .value = parse_value_id(id_text, "value id"),
        .amount = parse_amount(amount_text, "value amount"),
    });
}

void add_function(LabConfig& config, std::string_view text)
{
    const FourParts parts = split_four(text, ':', "function definition");
    config.program.functions.push_back({
        .input = parse_value_id(parts.first, "function input"),
        .output = parse_value_id(parts.second, "function output"),
        .throughput = parse_amount(parts.third, "function throughput"),
        .result_per_input = parse_amount(parts.fourth, "function result per input"),
    });
}

void add_end_rule(LabConfig& config, std::string_view text)
{
    const ThreeParts parts = split_three(text, ':', "end rule definition");
    config.program.end_rules.push_back({
        .source = parse_value_id(parts.first, "end rule source"),
        .target = parse_value_id(parts.second, "end rule target"),
        .target_per_source = parse_amount(parts.third, "end rule factor"),
    });
}

[[nodiscard]] CommandLine parse_command_line(int argc, char* argv[])
{
    CommandLine command{.config = has_custom_program(argc, argv) ? LabConfig{} : make_default_config()};

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            command.show_help = true;
        } else if (argument == "--ticks") {
            command.config.ticks = parse_tick_count(require_value(argc, argv, index, argument));
        } else if (argument == "--values") {
            command.config.program.value_count = parse_size(require_value(argc, argv, index, argument), "value count");
        } else if (argument == "--name") {
            set_name(command.config, require_value(argc, argv, index, argument));
        } else if (argument == "--initial") {
            add_value_amount(command.config.program.initial_values, require_value(argc, argv, index, argument),
                             "initial value");
        } else if (argument == "--external") {
            add_value_amount(command.config.external_values, require_value(argc, argv, index, argument), "external value");
        } else if (argument == "--function") {
            add_function(command.config, require_value(argc, argv, index, argument));
        } else if (argument == "--end-rule") {
            add_end_rule(command.config, require_value(argc, argv, index, argument));
        } else if (argument == "--observe") {
            command.config.observed_values.push_back(
                parse_value_id(require_value(argc, argv, index, argument), "observed value"));
        } else {
            throw std::invalid_argument{"unknown option: " + std::string{argument}};
        }
    }
    return command;
}

[[nodiscard]] std::string display_name(const LabConfig& config, clife::ValueId id)
{
    const auto found = std::find_if(config.names.begin(), config.names.end(), [id](const NamedValue& entry) {
        return entry.id == id;
    });
    if (found != config.names.end()) {
        return found->name;
    }
    return "Value[" + std::to_string(id.index) + ']';
}

void print_help()
{
    std::cout << "CLife headless genome calculator\n\n"
              << "Usage:\n"
              << "  clife_headless [options]\n\n"
              << "Options:\n"
              << "  --ticks N                              Number of simulation ticks\n"
              << "  --values N                             Number of numeric values\n"
              << "  --name VALUE:NAME                      Human-readable value name\n"
              << "  --initial VALUE=AMOUNT                 Initial persistent value\n"
              << "  --external VALUE=AMOUNT                External root supplied every tick\n"
              << "  --function IN:OUT:THROUGHPUT:FACTOR    Genome function\n"
              << "  --end-rule SOURCE:TARGET:FACTOR        End-of-tick world rule\n"
              << "  --observe VALUE                        Print result column\n"
              << "  --help, -h                             Show this help\n\n"
              << "Functions compete proportionally when they take the same input. FACTOR is the current minimal\n"
              << "result formula: output = actually_taken * FACTOR. End rules consume their remaining source\n"
              << "simultaneously after the genome pipeline. A custom program must specify --values.\n\n"
              << "Default cell-world slice:\n"
              << "  Light=1 -> Energy; genome uses up to 0.25 Energy; remaining Energy adds 0.1 Temperature per unit.\n";
}

void print_configuration(const LabConfig& config)
{
    std::cout << "CLife genome calculator lab\n\n";
    std::cout << "ticks = " << config.ticks << '\n';
    std::cout << "values = " << config.program.value_count << '\n';

    for (const clife::ValueAmount& initial : config.program.initial_values) {
        std::cout << "Initial " << display_name(config, initial.value) << " = " << initial.amount << '\n';
    }
    for (const clife::ValueAmount& external : config.external_values) {
        std::cout << "External " << display_name(config, external.value) << " = " << external.amount << " / tick\n";
    }
    for (const clife::Function& function : config.program.functions) {
        std::cout << "Function: " << display_name(config, function.input) << " -> "
                  << display_name(config, function.output) << ", throughput=" << function.throughput
                  << ", factor=" << function.result_per_input << '\n';
    }
    for (const clife::EndRule& rule : config.program.end_rules) {
        std::cout << "World end rule: remaining " << display_name(config, rule.source) << " -> "
                  << display_name(config, rule.target) << " * " << rule.target_per_source << '\n';
    }
    std::cout << '\n';
}

void print_header(const LabConfig& config)
{
    std::cout << std::setw(8) << "tick";
    for (const clife::ValueAmount& external : config.external_values) {
        std::cout << std::setw(kValueWidth) << ("input:" + display_name(config, external.value));
    }
    for (const clife::ValueId id : config.observed_values) {
        std::cout << std::setw(kValueWidth) << display_name(config, id);
    }
    std::cout << '\n';
}

void print_row(const LabConfig& config, const clife::Simulation& simulation, const clife::Calculator& calculator)
{
    std::cout << std::setw(8) << simulation.tick();
    for (const clife::ValueAmount& external : config.external_values) {
        std::cout << std::setw(kValueWidth) << external.amount;
    }
    for (const clife::ValueId id : config.observed_values) {
        std::cout << std::setw(kValueWidth) << calculator.value(id);
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

    clife::Calculator calculator{config.program};
    clife::Simulation simulation;

    std::cout << std::fixed << std::setprecision(3);
    print_header(config);
    for (clife::Tick tick = 0; tick < config.ticks; ++tick) {
        calculator.step(config.external_values);
        simulation.step();
        print_row(config, simulation, calculator);
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
