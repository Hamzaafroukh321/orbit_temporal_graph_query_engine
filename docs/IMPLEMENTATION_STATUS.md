# Implementation Status

## Current Phase

Phase 3 - first useful path, with Phase 0-2 vertical-slice work in progress.

## Selected Specification

`09_orbit_temporal_graph_query_engine.md` was selected because it matches this repository name and contains the complete numbered Orbit architecture, including Sections 14, 20 and 21.

## Last Completed Ticket

ORB-001 through ORB-039 are implemented and manually verified; retained compaction semantic verification is implemented through `80e20e8`, and deterministic recovery prefix verification is implemented through `1767c25`.

## Next Actionable Ticket

Perform the final requirement audit against Sections 20 and 21, then keep any full-version gaps explicitly tracked.

## Completed Modules

- CMake target layout and presets.
- Error/result model.
- Checked arithmetic, IDs, intervals, values and limits.
- OGR-1 development-subset reader/writer with transaction groups and CRC32C.
- Deterministic OGR recovery prefix checks for every byte cut across a
  multi-commit transaction stream.
- Single-writer transactions, immutable snapshots, reopen/check and stable commit IDs.
- In-process committed-change subscriptions that stream sorted mutation IDs live and after reopen.
- OQS subset parser, typed property predicates, explain fingerprints, scan, one-hop and bounded path execution.
- API-bound typed OQS predicate parameters using `$name` and `QueryOptions::parameters`.
- Snapshot-local label, property, incoming adjacency, and outgoing adjacency indexes used by query execution.
- Explicit commit-visible candidate selection followed by temporal interval selection for snapshot materialization.
- Stable value-based continuation keys for scan, adjacency, and path result batches.
- Explicit `ORDER ASC|DESC` clauses over canonical Orbit result order.
- `WHERE` predicates for `=`, `!=`, `<`, `<=`, `>`, and `>=` over typed values.
- API-level deterministic parallel query execution option for independent seed predicate filtering and one-hop expansion, with canonical serial ordering and cursor batching preserved.
- Bounded BFS path execution with explicit hop/frontier resource errors and simple-path cycle rejection.
- Cost-aware bounded path ordering using finite nonnegative edge properties.
- Bidirectional `STEP IN|OUT` and `PATH IN|OUT` traversal.
- Snapshot index coverage metadata with generation and commit coverage boundaries.
- Background snapshot-index build worker API with coverage reporting and shutdown rejection.
- Generation lease registry, snapshot pins, cache stats, and unpinned index-generation eviction.
- Keep-last-commit compaction retention planner with pin-aware publishability reports.
- Compaction publish/retire API that rejects pinned source generations, verifies retained commit/time snapshots plus snapshot-local indexes before publication, and retires unpinned older index generations.
- Cancellation token, query work budget enforcement, and store shutdown rejection for new work.
- Byte-driven OGR parser/recovery, graph sequence, and query pipeline fuzz smoke harnesses.
- Independent ordered-map reference comparison inside the graph sequence fuzz smoke for commit heads plus scan, one-hop, and bounded path rows across reopens.
- Fault matrix tests and script for I/O-open failure, malformed mutation rollback, shutdown/cancel safety, compaction failure preservation, and fuzz smoke.
- Regression, CLI smoke, MSVC verification, and CMake sanitizer/coverage matrix scripts.
- Coverage CMake preset and coverage instrumentation option for non-MSVC toolchains.
- In-process benchmark target and MSVC benchmark smoke for mutation, lookup, one-hop, and path execution.
- Snapshot/compaction/recovery soak target and MSVC soak smoke.
- `fixtures/compat/v0_1` compatibility fixture set and MSVC compatibility verifier.
- CMake package export metadata plus manual MSVC package and public consumer smoke.
- Manual MSVC build fallback for library, CLI, tests, and fuzz smoke executables.
- CLI workflow for init/apply/query/explain/check/inspect.
- Named unit/integration tests and three fuzz smoke targets.

## In-Progress Modules

- Full-version recovery, relocation compaction, durable background index catalog integration, parallel bounded-path coverage, performance budgets and long-run fuzzing.

## Known Blockers

- The local CMake and Ninja installations are outside the default shell `PATH`; `scripts\verify_cmake_matrix.cmd` now discovers their installed locations.
- ASan/UBSan, TSan, and coverage presets pass under the local MSVC generator path, but MSVC does not enable the non-MSVC sanitizer/coverage instrumentation configured in `cmake/Sanitizers.cmake`.
- Full-version implementation remains substantially incomplete; unsupported items are tracked as `Not started` or `Blocked` rather than claimed complete.

## Build And Test Status

- `where.exe cmake` fails in a fresh shell, but CMake 4.3.3 is installed at `C:\Program Files\CMake\bin\cmake.exe`.
- Ninja is installed under the user WinGet package directory.
- MSVC Build Tools were found through `C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat`.
- `scripts\build_msvc.cmd` succeeded.
- `scripts\verify_cmake_matrix.cmd` passed for debug, release, RelWithDebInfo, asan, tsan, coverage, fuzz, CTest, and release install/export.
- Manual MSVC `/std:c++20 /EHsc /W4 /WX` build succeeded for `orbit_unit_tests.exe`, `orbit.exe`, and all three fuzz smoke executables.
- `build\manual\orbit_unit_tests.exe` passed: 80 tests, 0 failed.
- Fault matrix smoke passed through `scripts\run_fault_matrix.cmd` build/test output, plus direct successful runs of all three fuzz smoke executables.
- `scripts\run_regression_matrix.cmd --no-build` passed, covering unit tests, fuzz smokes, CLI smoke, and fault matrix smoke.
- `scripts\verify_msvc.cmd` passed, covering fresh manual build plus the full regression matrix.
- `scripts\benchmark_msvc.cmd --no-build` passed and produced the local baseline in `docs/PERFORMANCE.md`.
- `scripts\soak_msvc.cmd --no-build` passed: 25 cycles, 26 commits, 25 compaction attempts, 3 reopens, and 18 held-snapshot checks.
- `scripts\compat_msvc.cmd --no-build` passed against `fixtures\compat\v0_1`.
- `scripts\package_msvc.cmd --no-build` passed, including compilation and execution of `examples\embedded_history.cpp` against the packaged headers and `orbit.lib`.
- CLI workflow passed: `init`, `apply`, `query`, `explain`, and `check`.

## Sanitizer Status

ASan/UBSan, TSan, coverage, and fuzz presets exist, and `scripts\verify_cmake_matrix.cmd` passes under MSVC. Real sanitizer and coverage instrumentation remains toolchain-dependent: the current CMake sanitizer helper intentionally returns early for MSVC, so Clang/GCC sanitizer evidence is still pending.

## Fuzz Status

Three production-linked byte-driven fuzz smoke targets exist in `fuzz/`. All three build with the manual MSVC fallback and run successfully against `corpus`. The graph sequence smoke compares production commit heads plus scan, one-hop, and bounded path rows against an independent ordered-map reference model after each operation and reopen.

## Documentation Status

Required documentation files exist and describe the implemented subset and pending work.

## Performance Status

`scripts\benchmark_msvc.cmd --no-build` passed in this environment and records a small deterministic debug-style baseline in `docs/PERFORMANCE.md`. Full release-platform performance acceptance remains unverified.

## Deviations

- ADR-0002 records the OGR development subset and pending full format features.
- ADR-0003 records the currently implemented OQS subset and pending full planner/operator surface.
- Compaction currently verifies retained snapshot/index semantics and publishes in-memory index-generation replacement metadata, but does not rewrite OGR segment bytes; this is documented as an implementation subset.

## Last Verified Commit

`1767c25` (`test(format): verify recovery prefixes`) has passed manual MSVC and CMake/Ninja verification.

## Timestamp

2026-07-02 Africa/Casablanca.
