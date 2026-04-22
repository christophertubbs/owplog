# owplog C API Interop Guide (Constrained API)

This guide covers the current C ABI, which is intentionally constrained.

## API Scope

The C API now only allows:

1. Submitting log messages
2. Registering named subloggers
3. Automatic stdout/stderr fallback when no upstream owplog logger has configured sinks
4. Optional extended metadata (`file`, `line`, `function`, `language`, `rank`)

The C API does **not** allow:

- Logger initialization
- Adding sinks
- Setting levels
- Starting/stopping stdio capture
- Flush/shutdown lifecycle control

## Header and Library

- Header: `include/owplog/c_api.h`
- Shared library target: `owplog_c`

## Exported Functions

```c
typedef int owplog_sublogger_handle;
typedef struct owplog_log_meta {
    const char* file;
    int line;
    const char* function;
    const char* language;
    int rank;
} owplog_log_meta;

owplog_sublogger_handle owplog_logger_add_sublogger(const char* sublogger_name);
void owplog_logger_log(int level, const char* logger_name, const char* message);
void owplog_logger_log_sublogger(int level, owplog_sublogger_handle sublogger_handle, const char* message);
void owplog_logger_log_ex(int level, const char* logger_name, const char* message, const owplog_log_meta* meta);
```

## Function Contracts

### `owplog_logger_add_sublogger`

- Inputs:
  - `sublogger_name`: null-terminated logger name string.
- Output:
  - Returns positive handle on success.
  - Returns `0` on failure.
- Preconditions:
  - `sublogger_name` should be non-null and non-empty.
- Postconditions:
  - Successful handle can be reused with `owplog_logger_log_sublogger`.
  - Re-registering the same name returns the same handle.
- Error behavior:
  - No exception mechanism in C ABI.
  - Invalid input maps to return value `0`.

### `owplog_logger_log`

- Inputs:
  - `level`: numeric level (out-of-range values are clamped).
  - `logger_name`: logger name (nullable; defaults to `"owplog.c"`).
  - `message`: message text (nullable; defaults to empty string).
- Output:
  - No return value.
- Preconditions:
  - None; null pointers are accepted.
- Postconditions:
  - Message is submitted to upstream owplog logger when sinks exist.
  - Otherwise message is emitted to stdout/stderr fallback.
- Error behavior:
  - Invalid level values are clamped, not rejected.

### `owplog_logger_log_sublogger`

- Inputs:
  - `level`: numeric level (clamped).
  - `sublogger_handle`: handle from `owplog_logger_add_sublogger`.
  - `message`: message text (nullable; defaults to empty string).
- Output:
  - No return value.
- Preconditions:
  - None.
- Postconditions:
  - Valid handle uses registered name.
  - Invalid handle falls back to `"owplog.c"`.
- Error behavior:
  - Invalid level/handle do not throw; safe fallback behavior is applied.

### `owplog_logger_log_ex`

- Inputs:
  - `level`, `logger_name`, `message`: same behavior as `owplog_logger_log`.
  - `meta`: nullable pointer to optional metadata (`file`, `line`, `function`, `language`, `rank`).
- Output:
  - No return value.
- Preconditions:
  - Any non-null pointers in `meta` should reference valid null-terminated strings for call duration.
- Postconditions:
  - Metadata is forwarded to upstream owplog logger path.
  - Fallback stdout/stderr path emits message text without structured metadata.
- Error behavior:
  - Same clamped-level behavior as other logging calls.

Levels:

- `OWPLOG_LOG_TRACE` (0)
- `OWPLOG_LOG_DEBUG` (10)
- `OWPLOG_LOG_INFO` (20)
- `OWPLOG_LOG_WARNING` (30)
- `OWPLOG_LOG_ERROR` (40)
- `OWPLOG_LOG_CRITICAL` (50)
- `OWPLOG_LOG_OFF` (60)

## Upstream vs Fallback Behavior

- If the host process has configured owplog sinks (upstream logger), C-ABI messages are forwarded into owplog.
- If not, C-ABI messages are written directly to stdout/stderr.
  - Error and above -> stderr
  - Other levels -> stdout

## Build

```bash
cmake -S . -B build -DOWPLOG_BUILD_SHARED_C_API=ON
cmake --build build --config Release
```

## C Example

```c
#include "owplog/c_api.h"

int main(void) {
    owplog_logger_log(OWPLOG_LOG_INFO, "c.main", "direct logger name message");

    owplog_sublogger_handle h = owplog_logger_add_sublogger("c.main.rank_0");
    if (h > 0) {
        owplog_logger_log_sublogger(OWPLOG_LOG_INFO, h, "sublogger handle message");
    }
    owplog_log_meta meta = {"main.c", 10, "main", "c", -1};
    owplog_logger_log_ex(OWPLOG_LOG_INFO, "c.main", "extended metadata message", &meta);
    return 0;
}
```

## C++ Example

```cpp
#include "owplog/c_api.h"

int main() {
    owplog_logger_log(OWPLOG_LOG_INFO, "cpp.main", "hello");
    const owplog_sublogger_handle h = owplog_logger_add_sublogger("cpp.main.worker");
    owplog_logger_log_sublogger(OWPLOG_LOG_WARNING, h, "worker warning");
}
```

## Python Example (ctypes)

```python
import ctypes

lib = ctypes.CDLL("libowplog_c.so")  # adjust for platform
lib.owplog_logger_add_sublogger.argtypes = [ctypes.c_char_p]
lib.owplog_logger_add_sublogger.restype = ctypes.c_int
lib.owplog_logger_log.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p]
lib.owplog_logger_log_sublogger.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_char_p]
class OwplogLogMeta(ctypes.Structure):
    _fields_ = [("file", ctypes.c_char_p), ("line", ctypes.c_int), ("function", ctypes.c_char_p), ("language", ctypes.c_char_p), ("rank", ctypes.c_int)]
lib.owplog_logger_log_ex.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(OwplogLogMeta)]

h = lib.owplog_logger_add_sublogger(b"py.main")
lib.owplog_logger_log(20, b"py.main", b"direct name")
lib.owplog_logger_log_sublogger(20, h, b"handle-based message")
meta = OwplogLogMeta(b"app.py", 42, b"worker", b"python", 0)
lib.owplog_logger_log_ex(20, b"py.main", b"with metadata", ctypes.byref(meta))
```

## Fortran Example (ISO_C_BINDING)

```fortran
use, intrinsic :: iso_c_binding
implicit none

interface
  function owplog_logger_add_sublogger(name) bind(C, name="owplog_logger_add_sublogger") result(h)
    import :: c_char, c_int
    character(kind=c_char), dimension(*) :: name
    integer(c_int) :: h
  end function
  subroutine owplog_logger_log(level, logger_name, message) bind(C, name="owplog_logger_log")
    import :: c_int, c_char
    integer(c_int), value :: level
    character(kind=c_char), dimension(*) :: logger_name
    character(kind=c_char), dimension(*) :: message
  end subroutine
end interface
```

## Interop Helpers

Reusable helpers are in `interop/`:

- `interop/c/owplog_logger_client.*`
- `interop/cpp/owplog_cxx_logger.hpp`
- `interop/fortran/owplog_logger_mod.f90`
- `interop/python/owplog_logger.py`
