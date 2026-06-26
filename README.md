# Orbit Temporal Graph Query Engine

Orbit is a C++20 embedded temporal property-graph engine. This repository currently contains an early production-linked vertical slice:

- OGR-1 store creation, append-only transaction groups, checksummed record reading, and reopen.
- Stable node and edge IDs with immutable commit snapshots and half-open valid-time intervals.
- Atomic node/edge/property mutations through a single-writer transaction API.
- A small original OQS subset for label scans, property filters, one-hop `STEP OUT`, bounded `PATH OUT`, and deterministic result batches.
- CLI commands for `init`, `apply`, `query`, `explain`, `check`, and `inspect`.
- Unit/integration tests plus three smoke fuzz harnesses wired to production code.

The full specification in `09_orbit_temporal_graph_query_engine.md` remains the source of truth. Background indexing, compaction relocation, full recovery matrices, subscriptions, parallel query mode, and performance campaigns are not yet complete.

## Build

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Sanitizer presets are available as `asan` and `tsan` where the host toolchain supports them.

On Windows hosts with Visual Studio Build Tools but no CMake on `PATH`, the checked fallback is:

```bat
scripts\verify_msvc.cmd
```

When CMake is available, `scripts\verify_cmake_matrix.cmd` runs the debug,
release, RelWithDebInfo, ASan/UBSan, TSan, coverage, and fuzz-smoke presets.

## CLI Example

```sh
orbit init graph.ogr
printf "node 1 Service 0 100 tier=api\nnode 2 Database 0 100\nedge 10 1 2 DEPENDS 0 100\n" > changes.oms
orbit apply graph.ogr changes.oms
orbit query graph.ogr "FROM Service STEP OUT DEPENDS YIELD node.id" --time 10
```

## Maturity

This is not a complete full-version Orbit release. See `docs/IMPLEMENTATION_STATUS.md` and `docs/REQUIREMENTS_TRACEABILITY.md` for current evidence and remaining work.
