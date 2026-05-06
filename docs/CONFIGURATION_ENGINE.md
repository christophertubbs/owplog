# Prototype Configuration Engine

This document describes the prototype text-based configuration engine in `include/configuration.hpp` and `src/configuration.cpp`.

The engine is intentionally small. It does not use JSON, YAML, TOML, or any other third-party parser. The goal is to let applications configure the global `owplog` logger from a plain text document while keeping the code easy to audit in regulated environments.

## What It Does

The configuration engine reads a text file containing `key = value` settings. It turns those settings into a `LoggerConfiguration` object, then applies that object to the process-wide logger only when the logger does not already have handlers.

In plain language:

- If the application has already set up logging, the configuration engine leaves it alone.
- If logging has not been set up yet, the configuration engine adds the configured outputs, filters, and formatter.
- If the text file contains a mistake, the engine reports errors instead of guessing silently.

This is a prototype. It is meant to prove the workflow and provide a safe starting point for a fuller configuration system.

## Supported Settings

Blank lines are ignored. Text after `#` is treated as a comment.

```text
# Overall logger threshold
logger.level = debug

# Formatter used by all handlers
formatter.pattern = %T [%L] (%N) %f:%l %F rank=%r lang=%g %m

# Optional global filters
filters.global.level = info
filters.global.substring = solver

# Console output
handlers.console.enabled = true
handlers.console.stderr_for_errors = true
handlers.console.suppress_captured_streams = true
handlers.console.filter.level = debug

# File output
handlers.file.enabled = true
handlers.file.path = ${cwd}/${application_name}.log
handlers.file.append = true
handlers.file.filter.substring = e2e

# Optional stdout/stderr capture
capture_stdio = false
capture_stdout = true
capture_stderr = true
```

Valid log levels are:

- `trace`
- `debug`
- `info`
- `warning` or `warn`
- `error`
- `critical`
- `off`

Boolean values may be written as:

- `true`, `yes`, `on`, `1`
- `false`, `no`, `off`, `0`

Template variables may be used in string settings such as file paths, formatter patterns, and substring filters:

- `${cwd}` expands to the process current working directory.
- `${application_name}` expands to `application.name` from the config file, or to the application name supplied by `ConfigurationContext`.

Example:

```text
application.name = End2End
handlers.file.path = ${cwd}/${application_name}.log
```

## How Applications Use It

An application can configure logging early during startup:

```cpp
#include "configuration.hpp"

int main() {
    owplog::ConfigurationResult result =
        owplog::configure_global_logger_from_file_if_needed("owplog.conf");

    if (!result.ok()) {
        // Report result.errors using the application's startup error path.
        return 1;
    }

    // Continue with normal application work.
}
```

The phrase `if_needed` matters. The global logger is only configured when `Logger::instance().has_handlers()` is false. This prevents a library or plugin from unexpectedly replacing logging that the application already configured.

## Design Approach

The design has three steps.

1. Parse text into data.

The parser reads each `key = value` line and fills a `LoggerConfiguration` structure. This keeps file parsing separate from logger mutation.

2. Validate obvious mistakes.

For example, if the file handler is enabled but no file path is provided, the parser records an error.

3. Apply only when appropriate.

`configure_global_logger_if_needed()` checks whether the global logger already has handlers. If it does, configuration is skipped and `ConfigurationResult::skipped_existing_logger` is set.

## Error Reporting

Most functions return `ConfigurationResult`.

Important fields:

- `applied`: settings were applied to the logger.
- `skipped_existing_logger`: settings were not applied because logging already had handlers.
- `warnings`: non-fatal issues, such as unknown keys.
- `errors`: fatal issues, such as unknown log levels or missing file paths.

`ConfigurationResult::ok()` returns true when there are no errors.

## Current Limitations

This prototype supports only the built-in logger pieces already present in the project:

- `PatternFormatter`
- `ConsoleHandler`
- `FileHandler`
- `LevelFilter`
- `SubstringFilter`

It does not yet support named handler lists, multiple file handlers, custom filters, environment-variable expansion, includes, schema versions, hot reload, or per-language configuration.

## Suggested Next Steps

The next practical improvements would be:

- Add this source file to the build after the interface settles.
- Add focused parser tests using `std::istringstream`.
- Add tests proving that an already-configured logger is not modified.
- Decide whether unknown keys should be warnings, errors, or controlled by a strict mode.
- Add a real example configuration file under `examples/` once the final syntax is accepted.
