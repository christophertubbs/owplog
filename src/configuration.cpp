#include "owplog/configuration.hpp"
#include "owplog/handlers/console_handler.h"
#include "owplog/handlers/file_handler.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>

namespace owplog::configuration {
namespace {
std::string trim(const std::string& value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    return first == std::string::npos ? "" : value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}
bool boolean(const std::string& value) {
    if (value == "true") return true;
    if (value == "false") return false;
    throw ConfigurationError("expected true or false, got '" + value + "'");
}
void allowed(const HandlerOptions& options, const std::set<std::string>& keys) {
    for (const auto& option : options) {
        if (!keys.count(option.first)) throw ConfigurationError("unknown option '" + option.first + "'");
    }
}
LogLevel level(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    for (LogLevel candidate : {LogLevel::Trace, LogLevel::Debug, LogLevel::Info, LogLevel::Warning,
                               LogLevel::Error, LogLevel::Critical, LogLevel::Off}) {
        if (value == to_string(candidate)) return candidate;
    }
    throw ConfigurationError("unknown level '" + value + "'");
}
}
HandlerRegistry::HandlerRegistry() {
    register_handler("ConsoleHandler", [](const HandlerOptions& options) {
        allowed(options, {"stderr_for_errors", "suppress_captured_streams"});
        return std::make_shared<handlers::ConsoleHandler>(
            options.count("stderr_for_errors") ? boolean(options.at("stderr_for_errors")) : true,
            options.count("suppress_captured_streams") ? boolean(options.at("suppress_captured_streams")) : true);
    });
    register_handler("FileHandler", [](const HandlerOptions& options) {
        allowed(options, {"path", "append"});
        if (!options.count("path") || options.at("path").empty()) throw ConfigurationError("FileHandler requires path");
        return std::make_shared<handlers::FileHandler>(options.at("path"),
            options.count("append") ? boolean(options.at("append")) : true);
    });
}
void HandlerRegistry::register_handler(const std::string& name, HandlerFactory factory) {
    if (name.empty() || !factory) throw ConfigurationError("handler name and factory must be nonempty");
    if (!factories_.emplace(name, std::move(factory)).second) throw ConfigurationError("duplicate handler class '" + name + "'");
}
const HandlerFactory& HandlerRegistry::factory(const std::string& name) const {
    const auto found = factories_.find(name);
    if (found == factories_.end()) throw ConfigurationError("unknown handler class '" + name + "'");
    return found->second;
}
core::LoggerConfigurationArguments load(std::istream& input, const HandlerRegistry& registry) {
    std::map<std::string, HandlerOptions> sections;
    std::string section;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        line = trim(line);
        if (line.empty() || line.front() == '#' || line.front() == ';') continue;
        try {
            if (line.front() == '[' && line.back() == ']') {
                section = trim(line.substr(1, line.size() - 2));
                if (section != "logger" && (section.rfind("handler.", 0) != 0 || section.size() == 8))
                    throw ConfigurationError("expected [logger] or [handler.NAME]");
                if (!sections.emplace(section, HandlerOptions{}).second) throw ConfigurationError("duplicate section '" + section + "'");
            } else {
                const std::size_t separator = line.find('=');
                if (section.empty() || separator == std::string::npos) throw ConfigurationError("expected key = value inside a section");
                const std::string key = trim(line.substr(0, separator));
                if (key.empty() || !sections.at(section).emplace(key, trim(line.substr(separator + 1))).second)
                    throw ConfigurationError("empty or duplicate key '" + key + "'");
            }
        } catch (const ConfigurationError& error) {
            throw ConfigurationError("line " + std::to_string(line_number) + ": " + error.what());
        }
    }
    if (input.bad() || (input.fail() && !input.eof())) throw ConfigurationError("configuration stream read failed");
    if (!sections.count("logger")) throw ConfigurationError("missing [logger] section");
    const HandlerOptions& logger = sections.at("logger");
    allowed(logger, {"level", "format", "datefmt", "filename", "handlers", "force"});
    core::LoggerConfigurationArguments arguments;
    if (logger.count("level")) arguments.level = level(logger.at("level"));
    if (logger.count("format")) arguments.format = logger.at("format");
    if (logger.count("datefmt")) arguments.datefmt = logger.at("datefmt");
    if (logger.count("force")) arguments.force = boolean(logger.at("force"));
    if (logger.count("filename")) {
        if (logger.at("filename").empty()) throw ConfigurationError("filename must not be empty");
        arguments.filename = logger.at("filename");
    }
    if (logger.count("handlers") && arguments.filename) throw ConfigurationError("filename and handlers are mutually exclusive");
    std::vector<std::string> names;
    std::set<std::string> selected;
    if (logger.count("handlers")) {
        std::istringstream list(logger.at("handlers"));
        std::string name;
        while (std::getline(list, name, ',')) {
            name = trim(name);
            if (name.empty() || !selected.insert(name).second) throw ConfigurationError("empty or duplicate handler reference");
            names.push_back(name);
        }
        if (names.empty() || trim(logger.at("handlers")).back() == ',') throw ConfigurationError("handlers requires a nonempty list");
    }
    // Resolve every declaration and reference before invoking any factory.
    for (const auto& entry : sections) {
        if (entry.first == "logger") continue;
        if (!entry.second.count("class")) throw ConfigurationError(entry.first + ": missing class");
        registry.factory(entry.second.at("class"));
    }
    for (const std::string& name : names) {
        if (!sections.count("handler." + name)) throw ConfigurationError("unknown handler instance '" + name + "'");
    }
    if (!names.empty()) arguments.handlers = std::vector<handlers::LogHandlerPtr>{};
    for (const std::string& name : names) {
        HandlerOptions options = sections.at("handler." + name);
        const std::string class_name = options.at("class");
        options.erase("class");
        try {
            handlers::LogHandlerPtr handler = registry.factory(class_name)(options);
            if (!handler) throw ConfigurationError("factory returned null");
            arguments.handlers->push_back(std::move(handler));
        } catch (const std::exception& error) {
            throw ConfigurationError("handler '" + name + "' (" + class_name + "): " + error.what());
        }
    }
    return arguments;
}
core::LoggerConfigurationArguments load_file(const std::string& path, const HandlerRegistry& registry) {
    std::ifstream input(path);
    if (!input) throw ConfigurationError("cannot open configuration file '" + path + "'");
    try { return load(input, registry); }
    catch (const ConfigurationError& error) { throw ConfigurationError(path + ": " + error.what()); }
}
void configure_file(const std::string& path, const HandlerRegistry& registry) {
    const core::LoggerConfigurationArguments arguments = load_file(path, registry);
    core::configure(arguments);
}
}
