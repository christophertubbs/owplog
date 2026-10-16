#ifndef OWPLOG_CONFIGURATION_HPP
#define OWPLOG_CONFIGURATION_HPP

#include "owplog/core/logger.h"
#include <functional>
#include <istream>
#include <map>
#include <stdexcept>

namespace owplog::configuration {
/** String options belonging to one named handler; class is removed before dispatch. */
using HandlerOptions = std::map<std::string, std::string>;
/** Factories validate their options and return a non-null handler or throw. */
using HandlerFactory = std::function<handlers::LogHandlerPtr(const HandlerOptions&)>;

/** Configuration syntax, validation, file I/O or factory failure. */
class ConfigurationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/** Explicit application-owned registry. Built-ins: ConsoleHandler and FileHandler.
 * Register before loading. Concurrent reads are safe only without registration.
 * Libraries register factories into this same registry; no dynamic loading occurs.
 */
class HandlerRegistry {
public:
    HandlerRegistry();
    /** Reject empty names, empty factories and duplicate names. */
    void register_handler(const std::string& name, HandlerFactory factory);
    const HandlerFactory& factory(const std::string& name) const;
private:
    std::map<std::string, HandlerFactory> factories_;
};

/** Parse and validate before constructing selected handlers in list order.
 * Does not modify a logger. Factory construction may have external side effects.
 * Throws ConfigurationError; see docs/FILE_CONFIGURATION.md for the grammar.
 */
core::LoggerConfigurationArguments load(std::istream& input, const HandlerRegistry& registry);
/** Read a text file. Relative handler paths use the process working directory. */
core::LoggerConfigurationArguments load_file(const std::string& path, const HandlerRegistry& registry);
/** Load fully, then call the existing process-wide core::configure API. */
void configure_file(const std::string& path, const HandlerRegistry& registry);
}
#endif
