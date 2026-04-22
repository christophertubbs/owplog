# owplog Interop Helpers

This folder provides reusable language helpers so callers do not need to rewrite C API setup every time.

Contents:

- `python/owplog_logger.py`: Python `logging` integration
  - `OwplogLogger` (`logging.Logger` subclass)
  - `OwplogHandler` (`logging.Handler`)
  - `owplog_handler_factory` (dictConfig factory)
  - `install_owplog_logger_class` / `restore_logger_class`
- `cpp/owplog_cxx_logger.hpp`: C++ wrapper around C API
- `c/owplog_logger_client.h` and `c/owplog_logger_client.c`: C convenience helpers
- `fortran/owplog_logger_mod.f90`: Fortran `ISO_C_BINDING` module with utility routines

## Intended Use

1. Build owplog with `OWPLOG_BUILD_SHARED_C_API=ON`.
2. Ensure runtime shared library path is set.
3. Reuse the wrapper matching your language.

Python dictConfig flow:

1. `install_owplog_logger_class()`
2. `logging.config.dictConfig(...)` with handler factory:
   - `"()": "owplog_logger.owplog_handler_factory"`

## Note

The constrained C ABI only supports:

1. Message submission
2. Named sublogger registration
3. Automatic stdout/stderr fallback when no upstream owplog sinks are configured
4. Optional extended metadata submission via `owplog_logger_log_ex` (source, language, rank)

It intentionally does not expose logger initialization or reconfiguration hooks.
