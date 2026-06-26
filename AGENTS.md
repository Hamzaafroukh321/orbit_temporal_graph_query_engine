# Repository Instructions

## Build

- Configure: `cmake --preset debug`
- Build: `cmake --build --preset debug`
- Release build: `cmake --preset release && cmake --build --preset release`

## Test

- Quick tests: `ctest --preset debug`
- ASan/UBSan: `cmake --preset asan && cmake --build --preset asan && ctest --preset asan`
- TSan, where supported by the host toolchain: `cmake --preset tsan && cmake --build --preset tsan && ctest --preset tsan`
- Fuzz smoke targets are normal executables named `orbit_ogr_parser_fuzz`, `orbit_graph_sequence_fuzz`, and `orbit_query_pipeline_fuzz`.

## Layout

- Public C++ API: `include/orbit/`
- Production implementation: `src/`
- CLI: `src/cli/`
- Unit and integration tests: `tests/`
- Fuzz smoke harnesses: `fuzz/`
- Project documentation and status: `docs/`

## Conventions

- C++20, RAII, explicit ownership, deterministic ordering, checked arithmetic before allocation or indexing.
- Public APIs return `orbit::Result<T>` or a stable `orbit::Error`.
- Do not expose raw storage pointers or iterators across snapshot/cursor yields.
- Keep binary format and query behavior documented before marking requirements verified.

## Git And Safety

- Commit coherent units with tests and status updates.
- Do not commit build directories, fuzz artifacts, crash dumps, local IDE files, or credentials.
- Do not rewrite history or use destructive Git operations without explicit user approval.

## Definition Of Done

- Relevant builds and tests pass.
- Sanitizers are run for touched native code when supported.
- Requirements traceability and implementation status are updated.
- Documentation describes actual behavior, not planned behavior.
