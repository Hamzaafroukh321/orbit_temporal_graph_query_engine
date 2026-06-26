# Testing

The test executable `orbit_unit_tests` contains named unit and integration tests for:

- Checked arithmetic and interval semantics.
- Canonical property values.
- OGR creation, reading, truncation behavior, CRC determinism, and malformed stores.
- Transaction coalescing, endpoint validation, deletes, snapshot preservation, and reopen.
- OQS grammar, diagnostics, explain fingerprints, temporal filter presence, cursor batching, one-hop expansion, bounded paths, and cancellation.

Run:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

When CMake is unavailable but Visual Studio Build Tools are installed, run:

```bat
scripts\build_msvc.cmd
build\manual\orbit_unit_tests.exe
build\manual\orbit_ogr_parser_fuzz.exe corpus
build\manual\orbit_graph_sequence_fuzz.exe corpus
build\manual\orbit_query_pipeline_fuzz.exe corpus
```

Fault matrix smoke:

```bat
scripts\run_fault_matrix.cmd
```

The current agent environment has MSVC Build Tools but still does not have `cmake` on `PATH`.
