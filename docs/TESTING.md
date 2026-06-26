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

The current environment used by the agent did not have `cmake`, `ninja`, or a C++ compiler on `PATH`, so executable results must be produced on a configured toolchain.
