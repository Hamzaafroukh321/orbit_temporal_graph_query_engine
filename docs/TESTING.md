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
scripts\verify_msvc.cmd
```

Useful narrower gates:

```bat
scripts\build_msvc.cmd
scripts\run_regression_matrix.cmd --no-build
scripts\run_cli_smoke.cmd --no-build
scripts\run_fault_matrix.cmd
```

When CMake is available on `PATH`, run the full preset matrix:

```bat
scripts\verify_cmake_matrix.cmd
```

That matrix covers debug, release, RelWithDebInfo, ASan/UBSan, TSan, coverage,
and fuzz-smoke presets. Exit code 20 means CMake is not available in the
current shell.

The current agent environment has MSVC Build Tools but still does not have `cmake` on `PATH`.
