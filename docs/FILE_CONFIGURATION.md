# File configuration draft

The built library now provides `owplog/configuration.hpp`. This implements a
small, dependency-free text configuration interface analogous to Python's
`dictConfig`: the application supplies a registry mapping class names to C++
factories, and a run-specific file chooses instances and constructor options.
No JSON, YAML, reflection, or third-party parser dependency is required.

## Quick start

```cpp
#include <owplog/configuration.hpp>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try {
        owplog::configuration::HandlerRegistry registry;
        owplog::configuration::configure_file(argv[1], registry);
        owplog::core::info("Started with file configuration");
        owplog::core::flush();
    } catch (const owplog::configuration::ConfigurationError& error) {
        // Report to stderr and terminate according to your application's policy.
        return 1;
    }
}
```

Pass a different filename each run. `examples/logging.ini` is a complete sample.
To construct a separate logger rather than configure the global logger:

```cpp
owplog::configuration::HandlerRegistry registry;
auto arguments = owplog::configuration::load_file("logging.ini", registry);
owplog::core::Logger logger(arguments);
```

`load(std::istream&, registry)` supports strings, application-owned streams, and
tests. Both loaders return `owplog::core::LoggerConfigurationArguments` directly.
You may inspect or adjust the arguments before calling `Logger(arguments)`,
`logger.configure(arguments)`, or `core::configure(arguments)`.

## Text grammar

* Sections are `[logger]` and `[handler.NAME]`. Names, keys and class identifiers
  are case sensitive. Section order is unrestricted; duplicates are errors.
* Settings are `key = value`, split at the first equals sign. Leading and trailing
  whitespace is removed. Interior spaces and additional equals signs are literal.
* Blank lines and lines whose first non-whitespace character is `#` or `;` are
  ignored. Inline comment markers are literal parts of values.
* Values are unquoted strings. Quotes, backslashes, dollar signs and percent
  signs have no parser meaning. Do not quote paths. No interpolation, environment
  expansion, includes, escaping, multiline values, or automatic library loading
  occurs. Paths are relative to the process working directory, not the file.
* Exactly one logger section is required. Unknown logger keys, malformed lines,
  duplicate keys, unresolved classes and missing references are errors.
* Handler instance names used in lists cannot contain commas. Avoid whitespace
  at either end of names. Use descriptive names such as `audit` and `console`.

### Logger settings

| Key | Meaning when supplied |
| --- | --- |
| `level` | trace, debug, info, warning, error, critical, off (case insensitive) |
| `format` | Passed verbatim to the existing PatternFormatter |
| `datefmt` | Passed to the existing logger datefmt argument |
| `force` | `true` or `false`; defaults to false |
| `filename` | Nonempty shorthand for the existing logger file argument |
| `handlers` | Nonempty comma-separated instance names, in dispatch order |

`filename` and `handlers` are mutually exclusive. Empty handler lists are rejected
because the current Logger does not implement an empty list as “disable output.”
Omitting both preserves the underlying logger's default behavior: construction
uses a console handler; reconfiguration retains existing handlers.

The loader forwards `force` and `datefmt` without changing their existing logger
semantics. This draft does not repair unrelated logger lifecycle or formatting
behavior. In particular, date rendering remains the responsibility of the existing
formatter. There is no hot reload, transactional logger replacement, capture
configuration, or global filter schema in this draft.

### Built-in handlers

Each handler section requires `class`. The registry initially contains:

| Class | Options |
| --- | --- |
| `ConsoleHandler` | `stderr_for_errors=true`, `suppress_captured_streams=true` |
| `FileHandler` | required nonempty `path`, `append=true` |

Boolean values are strictly `true` and `false`. Unknown built-in options are
errors when that handler is constructed. Multiple FileHandler instances are
supported. Avoid multiple console handlers unless duplicate output is intended.
Unselected sections have their class checked but are not instantiated, and their
constructor options are not validated.

## Adding handler implementations at runtime

A library can expose a registration function accepting the application's registry.
It may register any implementation returning `handlers::LogHandlerPtr`, including
SQLite, remote services, or application-owned sinks. No owplog source modification
is needed. For example, given your own `SqliteHandler` implementation:

```cpp
void register_database_logging(owplog::configuration::HandlerRegistry& registry) {
    registry.register_handler("mycompany.SqliteHandler",
        [](const owplog::configuration::HandlerOptions& options)
            -> owplog::handlers::LogHandlerPtr {
            if (options.size() != 1 || !options.count("database") ||
                options.at("database").empty()) {
                throw std::invalid_argument("expected only a nonempty database option");
            }
            return std::make_shared<SqliteHandler>(options.at("database"));
        });
}
```

Register before loading, then select it in the file:

```ini
[logger]
handlers = database
[handler.database]
class = mycompany.SqliteHandler
database = events.sqlite
```

Factories receive all section values except `class`, as a map of strings. They
own conversion and validation of constructor options. They may also attach
handler-local filters, set error policies and buffer sizes, or capture dependencies
such as an application-owned service client. A factory may return an existing
shared handler if sharing is appropriate. Duplicate class registration is rejected
rather than silently replacing behavior. Empty factories and null results are
errors. Names are exact registry identifiers, not C++ symbols or import paths.

Registry ownership is explicit: pass the same registry to each library's
registration function. There is no static global registration order. Complete all
registrations before concurrent loads; registration must not race with lookup.
Factory state must itself support concurrent loads if you use them. Keep shared
libraries loaded for the lifetimes of their registered callbacks and created
handlers. This API does not define a binary plugin ABI or call dlopen.

## Errors and side effects

Loaders throw `ConfigurationError` for configuration and standard factory errors.
Syntax errors include line numbers; file-loader errors include the filename;
factory failures identify the instance and class. Allocation failures and
non-standard exceptions may propagate. Applications choose their own fallback
policy; the loader does not silently fall back to default logging.

The complete document, logger scalar values, all class names and all selected
references are checked before factories run. Selected factories then execute in
list order. Loading does not mutate any logger. However, factories may open or
truncate files, connect to services, or otherwise have side effects. If a later
factory fails, previously created shared pointers are released but external side
effects are not rolled back. Treat configuration as trusted application input:
registered factories define the capabilities available to the file.

`configure_file` loads first, then calls `core::configure`. Existing logger
configuration semantics and exceptions still apply; this is not an atomic reload
API. For services requiring reload, build a separate lifecycle design before
using it on a running production logger.

## Verification and previous prototype

`tests/configuration_tests.cpp` covers custom factories and output dispatch,
ordered selection, real file loading and writing, literal format text, filename
shorthand, invalid documents, registry errors, stream failures, null factories,
factory exceptions, and validation before construction. Run the normal CMake
build and CTest suite.

`src/configuration.cpp.prototype` and `include/configuration.cpp.hpp` remain
historical sketches and are not built. Their dotted-key format (including the
root `logging.conf`) is not accepted by this API. Their capture, interpolation,
and filter ideas are not promises of current functionality. Use the new sample
and public header for this draft.
