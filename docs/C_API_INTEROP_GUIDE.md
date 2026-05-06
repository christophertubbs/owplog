# owplog C API Interop Guide

This guide is for people who need to send log messages into `owplog` from C, C++, Python, Fortran, or another language that can call C functions.

The fast version: link or load `libowplog_c`, include or reproduce the declarations from `include/owplog/c_api.h`, then call one of the logging functions.

## Pick Your Language

- C: see [C Minimum](#c-minimum)
- C++: see [C++ Minimum](#c-minimum-1)
- Python: see [Python Minimum](#python-minimum)
- Fortran: see [Fortran Minimum](#fortran-minimum)
- Other languages: see [Other Languages](#other-languages)
- Details: see [Low Level Guide](#low-level-guide)

## Bare Minimum Concepts

The C API is intentionally constrained. Foreign-language code can submit log records, but it does not configure the main logger.

That means:

- Your C, Python, or Fortran component can say "log this message."
- The host C++ application decides where messages go: console, file, captured stdio, filters, formatting, and shutdown.
- If the host application has not configured `owplog`, the C API falls back to stdout/stderr.

The three calls most users care about are:

```c
void owplog_logger_log(int level, const char* logger_name, const char* message);

owplog_sublogger_handle owplog_logger_add_sublogger(const char* sublogger_name);
void owplog_logger_log_sublogger(int level, owplog_sublogger_handle handle, const char* message);

void owplog_logger_log_ex(
    int level,
    const char* logger_name,
    const char* message,
    const owplog_log_meta* meta
);
```

Use `owplog_logger_log` first. Move to `owplog_logger_log_ex` only when you need source file, line number, function name, language name, or rank metadata.

## Build

Build the shared C ABI library:

```bash
cmake -S . -B build -DOWPLOG_BUILD_SHARED_C_API=ON
cmake --build build --config Release
```

The library target is `owplog_c`.

Typical output name on Linux:

```text
libowplog_c.so
```

Header:

```text
include/owplog/c_api.h
```

## C Minimum

Use this when writing C code or when you just want the smallest possible integration.

```c
#include "owplog/c_api.h"

int main(void) {
    owplog_logger_log(
        OWPLOG_LOG_INFO,
        "c.component",
        "hello from C"
    );

    return 0;
}
```

Compile/link shape:

```bash
cc app.c -I/path/to/owplog/include -L/path/to/owplog/build -lowplog_c
```

If the runtime loader cannot find `libowplog_c.so`, set a loader path:

```bash
LD_LIBRARY_PATH=/path/to/owplog/build ./a.out
```

## C++ Minimum

If you are writing C++ but deliberately using the C ABI:

```cpp
#include "owplog/c_api.h"

int main() {
    owplog_logger_log(
        OWPLOG_LOG_INFO,
        "cpp.component",
        "hello from C++ through the C ABI"
    );
}
```

If you are inside the core C++ application, prefer the native C++ logger API. Use the C ABI when you are crossing a language boundary, loading plugins, or matching the integration style used by non-C++ code.

## Python Minimum

Python can call the C ABI with `ctypes`.

```python
import ctypes

OWPLOG_LOG_INFO = 20

lib = ctypes.CDLL("libowplog_c.so")
lib.owplog_logger_log.argtypes = [
    ctypes.c_int,
    ctypes.c_char_p,
    ctypes.c_char_p,
]
lib.owplog_logger_log.restype = None

lib.owplog_logger_log(
    OWPLOG_LOG_INFO,
    b"python.component",
    b"hello from Python",
)
```

If Python cannot load the shared library, pass an absolute path:

```python
lib = ctypes.CDLL("/absolute/path/to/libowplog_c.so")
```

For normal Python logging integration, use the helper wrapper in:

```text
interop/python/owplog_logger.py
```

The helper is useful when you want `logging.getLogger(...).info(...)` to forward into `owplog`.

## Fortran Minimum

Fortran should use `ISO_C_BINDING`.

```fortran
program app
  use, intrinsic :: iso_c_binding
  implicit none

  interface
    subroutine owplog_logger_log(level, logger_name, message) bind(C, name="owplog_logger_log")
      import :: c_int, c_char
      integer(c_int), value :: level
      character(kind=c_char), dimension(*) :: logger_name
      character(kind=c_char), dimension(*) :: message
    end subroutine
  end interface

  call owplog_logger_log(20_c_int, "fortran.component" // c_null_char, "hello from Fortran" // c_null_char)
end program app
```

For real Fortran code, prefer the module in:

```text
interop/fortran/owplog_logger_mod.f90
```

That module hides the null-termination details and gives Fortran-friendly helper routines.

## Other Languages

If your language can call C functions from a shared library, you need to map these C types:

```c
int
const char*
struct owplog_log_meta*
```

Start by binding only:

```c
void owplog_logger_log(int level, const char* logger_name, const char* message);
```

Then add metadata after the basic call works.

## Tips And Tricks

- Start with `owplog_logger_log`. It is the lowest-friction path.
- Use stable logger names such as `python.routing`, `fortran.model`, or `plugin.reader`.
- Keep strings alive for the duration of the C call. Do not pass pointers to temporary memory that can disappear before the function returns.
- Null pointers are accepted for `logger_name`, `message`, and metadata fields in the main APIs.
- Use `OWPLOG_LOG_INFO` from C/C++. In Python or other languages, use the numeric value `20`.
- If you see output on stdout/stderr instead of the configured logger, the host process probably has not configured owplog handlers yet.
- If dynamic loading fails, check the shared library path first, not the logger code.
- In Python, always set `argtypes` before calling through `ctypes`. It catches many mistakes early.
- In Fortran, always add `c_null_char` to strings passed directly to C.
- In plugin-style systems, register subloggers once and reuse the handle.
- For high-volume loops, avoid formatting large strings unless the message is actually useful.

## Common Problems

### The message prints, but not in the expected format

The C ABI fallback path is probably active. Fallback output goes straight to stdout/stderr and does not apply the main C++ formatter.

Make sure the host application configured the global `owplog::Logger` with at least one handler.

### Python says it cannot find `libowplog_c.so`

Use an absolute path with `ctypes.CDLL`, or set `LD_LIBRARY_PATH` before launching Python.

### Fortran output has garbage characters

The string was probably not null-terminated. Add `// c_null_char`, or use `interop/fortran/owplog_logger_mod.f90`.

### Metadata does not show up

Metadata is only useful when the upstream logger path is active and the formatter includes metadata tokens such as source location, rank, or language.

### The process exits before final messages appear

The core logger is asynchronous. The host application should own logger lifecycle and flush or shut down at the appropriate time. The C ABI intentionally does not expose flush/shutdown.

## Design Philosophy

The C ABI is deliberately small.

Cross-language logging is most reliable when the boundary is boring. Every extra feature at the ABI boundary creates more ways for Python, Fortran, C, C++, plugins, and process shutdown order to disagree.

The design splits responsibility:

- Foreign-language components submit records.
- The host process owns logger configuration.
- The C ABI provides safe fallback output when the host has not configured logging.

This keeps the integration useful in two situations:

- A fully configured application where all languages feed into one central logger.
- A standalone component test where no C++ host logger exists yet, but messages still need to be visible.

The C ABI does not configure sinks, filters, formatters, stdio capture, flushing, or shutdown because those are process-level responsibilities. If a Python module or Fortran library could reconfigure the logger, it could accidentally break the main application.

## API Scope

The C API allows:

1. Submitting log messages.
2. Registering named subloggers.
3. Automatic stdout/stderr fallback when no upstream logger has configured handlers.
4. Optional extended metadata: file, line, function, language, rank.

The C API does not allow:

- Logger initialization.
- Adding handlers.
- Setting levels.
- Setting formatters.
- Adding filters.
- Starting/stopping stdio capture.
- Flush/shutdown lifecycle control.

## Low Level Guide

This section is for people writing wrappers, bindings, or runtime integration code.

### Header And Library

Header:

```text
include/owplog/c_api.h
```

Shared library target:

```text
owplog_c
```

Linux shared library name:

```text
libowplog_c.so
```

### Log Levels

```c
typedef enum owplog_log_level {
    OWPLOG_LOG_TRACE = 0,
    OWPLOG_LOG_DEBUG = 10,
    OWPLOG_LOG_INFO = 20,
    OWPLOG_LOG_WARNING = 30,
    OWPLOG_LOG_ERROR = 40,
    OWPLOG_LOG_CRITICAL = 50,
    OWPLOG_LOG_OFF = 60
} owplog_log_level;
```

Invalid numeric levels are clamped to the nearest owplog threshold by the implementation.

### Metadata Struct

```c
typedef struct owplog_log_meta {
    const char* file;
    int line;
    const char* function;
    const char* language;
    int rank;
} owplog_log_meta;
```

Field meaning:

- `file`: source file path or name.
- `line`: source line number. Use `0` when unknown.
- `function`: function, subroutine, method, or procedure name.
- `language`: caller language/runtime label, such as `c`, `python`, or `fortran`.
- `rank`: MPI/HPC rank. Use `-1` when unknown.

Pointer fields may be null. Non-null pointer fields must point to null-terminated strings that remain valid for the duration of the call.

### Function Reference

#### `owplog_logger_add_sublogger`

```c
owplog_sublogger_handle owplog_logger_add_sublogger(const char* sublogger_name);
```

Registers or resolves a logger name and returns an integer handle.

Returns:

- Positive handle on success.
- `0` if `sublogger_name` is null or empty.

Repeated registration of the same name returns the same handle.

#### `owplog_logger_log`

```c
void owplog_logger_log(
    int level,
    const char* logger_name,
    const char* message
);
```

Submits a simple record.

Null behavior:

- `logger_name == NULL` uses `owplog.c`.
- `message == NULL` uses an empty message.

#### `owplog_logger_log_sublogger`

```c
void owplog_logger_log_sublogger(
    int level,
    owplog_sublogger_handle sublogger_handle,
    const char* message
);
```

Submits a record through a previously registered handle.

Invalid handles fall back to `owplog.c`.

#### `owplog_logger_log_ex`

```c
void owplog_logger_log_ex(
    int level,
    const char* logger_name,
    const char* message,
    const owplog_log_meta* meta
);
```

Submits a record with optional metadata.

`meta` may be null. Individual pointer fields inside `meta` may also be null.

### Fallback Behavior

The C ABI checks whether the upstream global logger has configured handlers.

If handlers exist:

- The record is forwarded to `owplog::Logger::instance()`.
- Metadata is preserved.
- The active C++ formatter, filters, and handlers decide final output.

If handlers do not exist:

- The record is written directly to stdout or stderr.
- Error and above goes to stderr.
- Lower levels go to stdout.
- Structured metadata is not rendered by the fallback path.

### C Convenience Macros

`include/owplog/c_api.h` includes simple C macros:

```c
LOG_INFO("message");
LOG_WARNING("message");
LOG_ERROR("message");
LOG_DEBUG("message");
LOG_NAMED_INFO("component.name", "message");
```

These macros attach C source metadata using `__FILE__`, `__LINE__`, and `__func__`.

### Python Binding Notes

A complete low-level `ctypes` wrapper needs to define the metadata struct and function signatures:

```python
import ctypes

class OwplogLogMeta(ctypes.Structure):
    _fields_ = [
        ("file", ctypes.c_char_p),
        ("line", ctypes.c_int),
        ("function", ctypes.c_char_p),
        ("language", ctypes.c_char_p),
        ("rank", ctypes.c_int),
    ]

lib.owplog_logger_log_ex.argtypes = [
    ctypes.c_int,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.POINTER(OwplogLogMeta),
]
lib.owplog_logger_log_ex.restype = None
```

Keep encoded byte strings alive until the call returns. A short synchronous call like this is fine:

```python
meta = OwplogLogMeta(b"app.py", 42, b"worker", b"python", -1)
lib.owplog_logger_log_ex(20, b"py.worker", b"message", ctypes.byref(meta))
```

### Fortran Binding Notes

When calling C directly from Fortran:

- Use `bind(C, name="...")`.
- Use `integer(c_int), value` for `int` values passed by value.
- Pass C strings as `character(kind=c_char), dimension(*)`.
- Null-terminate strings with `// c_null_char`.

For metadata-heavy calls, prefer `interop/fortran/owplog_logger_mod.f90`, because it handles C string preparation and optional metadata fields for you.

### Threading Notes

The C ABI functions are intended to be callable from multiple producer threads. The C++ logger queues records asynchronously when the upstream logger path is active.

Callers should still avoid logging while holding locks that are also needed by shutdown, plugin unload, or language-runtime finalization paths.

### Lifecycle Notes

The host application owns logger lifecycle.

Foreign-language code should not assume it can flush or shut down the logger through the C ABI. If a plugin or script needs a flush point, expose that through the host application's own lifecycle, not through this constrained ABI.
