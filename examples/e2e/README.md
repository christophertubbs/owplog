# End-to-End Cross-Language Example

This example is a stress-oriented mixed-language logging run. The point is not only to show that one message can cross the boundary, but to exercise `owplog` under noisy, concurrent, and uneven workloads from several runtimes.

The native C++ program configures `owplog` once, then schedules repeated jobs from:

- C++ code in `cpp_component.cpp`
- C code in `c_component.c`
- Fortran code in `example_fortran.f90`, when a Fortran compiler is available
- Embedded Python code in `python_consumer.py`, when Python embedding and `pybind11` are available
- A small inline Python snippet that logs through a pybind11 embedded module

## Target

The CMake target is:

```text
owplog_example
```

The entrypoint is:

```text
examples/e2e/main.cpp
```

## Build

From the repository root:

```bash
cmake -S . -B build -DOWPLOG_BUILD_E2E_EXAMPLES=ON -DOWPLOG_BUILD_SHARED_C_API=ON
cmake --build build --target owplog_example
```

In CLion, reload the CMake project and build `owplog_example`.

The e2e CMake file tries to discover Python and `pybind11` with as little command-line setup as possible. If Python embedding is not available, the native target still builds and runs without the Python component.

## Optional Dependencies

Fortran is enabled when CMake finds a Fortran compiler.

Python embedding is enabled when CMake finds:

- Python interpreter
- Python development embed libraries/headers
- `pybind11`

When Python or Fortran support is unavailable, the executable prints an informational skip message and continues.

## Run

The executable path depends on the generator/build directory. Common examples:

```bash
./build/examples/e2e/owplog_example
```

or, for the current CLion debug tree:

```bash
./cmake-build-debug/examples/e2e/owplog_example
```

## Logging Behavior

The current e2e program configures logging in `main.cpp` with:

```text
level: trace
formatter: %T [%L] (%N) %f:%l %F rank=%r lang=%g %m
application name: End2End
console output: enabled
file append: false
stdio capture: enabled
```

The current hardcoded file path is:

```text
/DMOD/christopher.tubbs/owplog/End2End.log
```

The repository root also contains `logging.conf`, a sample prototype configuration file matching these settings with template variables:

```text
application.name = End2End
handlers.file.path = ${cwd}/${application_name}.log
```

That config file is for the prototype configuration engine in `include/configuration.hpp` and `src/configuration.cpp`. The e2e program still configures the logger directly unless it is explicitly wired to call the configuration engine.

## What The Test Exercises

The run intentionally mixes fast and slow producers:

- C and C++ jobs finish quickly.
- Fortran jobs emit repeated structured records from a fake model workload.
- Python jobs are slower because embedded Python execution is serialized by the Python GIL.
- Jobs are duplicated and shuffled before scheduling so language boundaries interleave instead of running in a neat fixed order.

This helps expose issues in:

- asynchronous logger queue behavior
- stdio capture
- source/rank/language metadata propagation
- C ABI fallback and forwarding paths
- Python GIL ownership and interpreter lifetime
- mixed-language shutdown ordering

## Files

- `main.cpp`: native driver, logger setup, job scheduling, Python embedding
- `threadpool.cpp`: small local thread pool used by the e2e driver
- `c_component.c`: C producer using the C ABI
- `cpp_component.cpp`: C++ producer using the logger API
- `example_fortran.f90`: Fortran producer using `interop/fortran/owplog_logger_mod.f90`
- `python_consumer.py`: Python workload using the local Python logging bridge
- `owplog_api.py`: Python `ctypes` wrapper for the C ABI
- `owplog_logger.py`: Python `logging.Handler` integration
- `CMakeLists.txt`: target and optional dependency discovery

## Troubleshooting

If the run appears idle after the fast jobs complete, check Python GIL ownership first. The main thread must not hold the GIL while worker threads are waiting to run Python jobs.

If no Python logs appear, confirm that:

- `OWPLOG_E2E_HAVE_PYTHON_EMBED` is defined for the target
- `pybind11_FOUND` is true during CMake configure
- `Python3_Development.Embed_FOUND` is true during CMake configure
- `sys.path` includes `examples/e2e`
- the Python logging handler can load `libowplog_c.so`

If Fortran is skipped, check whether CMake found a Fortran compiler.

If output goes to stdout/stderr instead of the configured file/console formatter, the C ABI fallback path may be active because the global logger did not have handlers when the foreign-language component logged.
