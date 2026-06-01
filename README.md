# owplog Logging Framework (C++17)

owplog is an asynchronous logging framework with:

- A singleton logger with a dedicated worker thread and thread-safe queue
- Extensible handlers (`ConsoleHandler`, `FileHandler`, `RouterHandler`,)
- Extensible formatters (`PatternFormatter`) and filters (`LevelFilter`, `SubstringFilter`)
- Optional capture of process `stdout`/`stderr` into log records
- A small C ABI (`extern "C"`) for use from C, Python (`ctypes`), Fortran, and other languages

Detailed cross-language instructions are in:

- `docs/C_API_INTEROP_GUIDE.md`
- Reusable wrappers and examples: `interop/`

API contracts (inputs/outputs/preconditions/postconditions/error behavior) are documented inline in:

- `include/owplog/*.hpp`
- `include/owplog/c_api.h`
- `interop/c/*`, `interop/cpp/*`, `interop/fortran/*`, and `interop/python/*`

## Build

```bash
cmake -S . -B build -DOWPLOG_BUILD_TESTS=ON -DOWPLOG_BUILD_SHARED_C_API=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

If `GTest` is not already installed, CMake will fetch `googletest` automatically.

### Helper Build Scripts

For low-fuss local builds, use:

```bash
bash scripts/build.sh
```

Run with prompts for all major values:

```bash
bash scripts/build.sh --interactive
```

Run configure + build + tests in one command:

```bash
bash scripts/test.sh
```

Interactive test flow:

```bash
bash scripts/test.sh --interactive
```

Pass `--help` to either script for all flags.

## C++ Usage

```cpp
#include "owplog/owplog.h"

int main() {
    owplog::basicConfig("app.log", owplog::LogLevel::Debug);

    owplog::log(owplog::LogLevel::Info, "service started", "app.main", OWPLOG_SOURCE_LOCATION);
    owplog::info("mpi step", "solver", OWPLOG_SOURCE_LOCATION, owplog::LogOrigin::Application, 0);
    OWPLOG_LOG_ERROR_MSG("something failed");
    logger.shutdown();
}
```

### Pattern Formatter Tokens

The formatting tokens are largely inspired by the python 
[logging](https://docs.python.org/3/library/logging.html#logrecord-attributes) style. Available
tokens are:

- `%(asctime)s`: timestamp
- `%(levelname)s`: log level name
- `%(levelno)d`: The number of the log level
- `%(name)s`: logger name
- `%(message)s`: message
- `%(thread)d`: thread id
- `%(filename)s`: source file
- `%(path)s`: The full path to the source file
- `%(lineno)d`: source line
- `%(funcName)s`: function name
- `%(process)d`: process id
- `%(relativeCreated)d`: The number of milliseconds since the application launched
- `%(elapsed)s`: A formatted string representing the amount of time elapsed within the application formatted as `HH:MM:SS.mmm`
- `%(rank)d`: The rank of the process if MPI is used
- `%(language)s`: The identified programming language

Default pattern:

```text
[%(asctime)s] %(levelname)s %(name)s %(filename)s::%(lineno)d - %(message)s
```

## C ABI Usage

Header: `include/owplog/c_api.h`

```c
#include "owplog/c_api.h"

int main(void) {
    owplog_logger_log(OWPLOG_LOG_INFO, "c.module", "hello from C ABI");
    const owplog_sublogger_handle h = owplog_logger_add_sublogger("c.module.rank_0");
    owplog_logger_log_sublogger(OWPLOG_LOG_INFO, h, "hello from sublogger handle");
    const owplog_log_meta meta = {"script.py", 88, "worker", "python", 0};
    owplog_logger_log_ex(OWPLOG_LOG_INFO, "c.module", "hello with metadata", &meta);
    return 0;
}
```

## Project Layout

- `include/owplog/*.hpp`: C++ public API
- `include/owplog/c_api.h`: C ABI
- `src/`: implementation
- `tests/logger_tests.cpp`: GoogleTest suite
- `examples/e2e/`: cross-language end-to-end demos (C/C++/Fortran/Python via pybind11)

## Notes

- Logging calls are non-blocking on producers; records are processed by the logger worker thread.
- Captured `stdout`/`stderr` records are marked by origin and, by default, not echoed back to console to avoid feedback loops.
- The C ABI does not expose logger initialization/reconfiguration; if no upstream sinks exist, C-ABI calls fall back to stdout/stderr.
