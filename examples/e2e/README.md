# End-to-End Cross-Language Examples

These examples demonstrate a **C++ main program configuring owplog once**, then
receiving log messages emitted from foreign-language components.

## Targets

1. `owplog_e2e_native`
- C++ main configures logger (formatter + sinks)
- calls C component (`c_component.c`)
- calls C++ component (`cpp_component.cpp`)
- calls Fortran component (`fortran_component.f90`) if a Fortran compiler is available
- embeds Python and calls a Python component if `pybind11` + Python development headers are available

2. `owplog_e2e_pybind11` (optional)
- C++ main configures logger
- embeds Python via `pybind11::embed`
- Python code calls an embedded pybind11 module function
- module forwards metadata-rich logs via `owplog_logger_log_ex`

## Build

From repo root:

```bash
cmake -S . -B build -DOWPLOG_BUILD_E2E_EXAMPLES=ON -DOWPLOG_BUILD_SHARED_C_API=ON
cmake --build build --config Release
```

## Run

Native mixed-language example:

```bash
./build/owplog_e2e_native
```

The native target prints an informational line when Fortran or Python embed dependencies are not available and continues running.

Pybind11 embedded Python example (only if target exists):

```bash
./build/owplog_e2e_pybind11
```

## Output

- Console output from the configured console sink
- Log files:
  - `owplog_e2e_native.log`
  - `owplog_e2e_pybind11.log`

Both include source/rank/language fields when supplied via metadata.
