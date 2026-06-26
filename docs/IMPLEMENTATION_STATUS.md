# Implementation Status

## Current Phase

Phase 3 - first useful path, with Phase 0-2 vertical-slice work in progress.

## Selected Specification

`09_orbit_temporal_graph_query_engine.md` was selected because it matches this repository name and contains the complete numbered Orbit architecture, including Sections 14, 20 and 21.

## Last Completed Ticket

ORB-001 through ORB-036 are implemented and manually verified through `b20dd73`.

## Next Actionable Ticket

Install or expose CMake, then run the documented CMake debug/release/sanitizer/coverage matrix. In parallel, continue with ORB-037 long snapshot/compaction/recovery soak automation.

## Completed Modules

- CMake target layout and presets.
- Error/result model.
- Checked arithmetic, IDs, intervals, values and limits.
- OGR-1 development-subset reader/writer with transaction groups and CRC32C.
- Single-writer transactions, immutable snapshots, reopen/check and stable commit IDs.
- OQS subset parser, explain fingerprints, scan, one-hop and bounded path execution.
- Snapshot-local label, property, and adjacency indexes used by query execution.
- Explicit commit-visible candidate selection followed by temporal interval selection for snapshot materialization.
- Stable value-based continuation keys for scan, adjacency, and path result batches.
- Bounded BFS path execution with explicit hop/frontier resource errors and simple-path cycle rejection.
- Cost-aware bounded path ordering using finite nonnegative edge properties.
- Snapshot index coverage metadata with generation and commit coverage boundaries.
- Generation lease registry, snapshot pins, cache stats, and unpinned index-generation eviction.
- Keep-last-commit compaction retention planner with pin-aware publishability reports.
- Compaction publish/retire API that rejects pinned source generations and retires unpinned older index generations.
- Cancellation token, query work budget enforcement, and store shutdown rejection for new work.
- Byte-driven OGR parser/recovery, graph sequence, and query pipeline fuzz smoke harnesses.
- Fault matrix tests and script for I/O-open failure, malformed mutation rollback, shutdown/cancel safety, compaction failure preservation, and fuzz smoke.
- Regression, CLI smoke, MSVC verification, and CMake sanitizer/coverage matrix scripts.
- Coverage CMake preset and coverage instrumentation option for non-MSVC toolchains.
- In-process benchmark target and MSVC benchmark smoke for mutation, lookup, one-hop, and path execution.
- Manual MSVC build fallback for library, CLI, tests, and fuzz smoke executables.
- CLI workflow for init/apply/query/explain/check/inspect.
- Named unit/integration tests and three fuzz smoke targets.

## In-Progress Modules

- Full-version recovery, compaction, background index building, parallel query mode, performance budgets and long-run fuzzing.

## Known Blockers

- CMake remains unavailable on `PATH`, so CMake presets, CTest, CMake sanitizer presets, and install/export behavior are not verified.
- Full-version implementation remains substantially incomplete; unsupported items are tracked as `Not started` or `Blocked` rather than claimed complete.

## Build And Test Status

- `where.exe cmake` failed; CMake is not on `PATH`.
- MSVC Build Tools were found through `C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat`.
- `scripts\build_msvc.cmd` succeeded.
- Manual MSVC `/std:c++20 /EHsc /W4 /WX` build succeeded for `orbit_unit_tests.exe`, `orbit.exe`, and all three fuzz smoke executables.
- `build\manual\orbit_unit_tests.exe` passed: 69 tests, 0 failed.
- Fault matrix smoke passed through `scripts\run_fault_matrix.cmd` build/test output, plus direct successful runs of all three fuzz smoke executables.
- `scripts\run_regression_matrix.cmd --no-build` passed, covering unit tests, fuzz smokes, CLI smoke, and fault matrix smoke.
- `scripts\verify_msvc.cmd` passed, covering fresh manual build plus the full regression matrix.
- `scripts\benchmark_msvc.cmd --no-build` passed and produced the local baseline in `docs/PERFORMANCE.md`.
- `scripts\verify_cmake_matrix.cmd` exits 20 in this environment because `cmake` is not on `PATH`.
- CLI workflow passed: `init`, `apply`, `query`, `explain`, and `check`.

## Sanitizer Status

ASan/UBSan, TSan, coverage, and fuzz presets exist, and `scripts\verify_cmake_matrix.cmd` automates them. Local sanitizer and coverage runs have not completed because CMake is unavailable and MSVC sanitizer support has not been configured for this project.

## Fuzz Status

Three production-linked byte-driven fuzz smoke targets exist in `fuzz/`. All three build with the manual MSVC fallback and run successfully against `corpus`.

## Documentation Status

Required documentation files exist and describe the implemented subset and pending work.

## Performance Status

`scripts\benchmark_msvc.cmd --no-build` passed in this environment and records a small deterministic debug-style baseline in `docs/PERFORMANCE.md`. Full release-platform performance acceptance remains unverified.

## Deviations

- ADR-0002 records the OGR development subset and pending full format features.
- ADR-0003 records the currently implemented OQS subset and pending full planner/operator surface.
- Compaction currently publishes in-memory index-generation replacement metadata but does not rewrite OGR segment bytes; this is documented as an implementation subset.

## Last Verified Commit

`b20dd73` (`perf: add benchmark smoke automation`) has passed manual MSVC build/test/regression/fault/fuzz/benchmark verification.

## Timestamp

2026-06-26 Africa/Casablanca.
