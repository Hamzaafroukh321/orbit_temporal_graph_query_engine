# Implementation Status

## Current Phase

Phase 3 - first useful path, with Phase 0-2 vertical-slice work in progress.

## Selected Specification

`09_orbit_temporal_graph_query_engine.md` was selected because it matches this repository name and contains the complete numbered Orbit architecture, including Sections 14, 20 and 21.

## Last Completed Ticket

ORB-001 through the initial ORB-026 vertical slice is implemented and manually verified. Index coverage metadata changes are pending commit.

## Next Actionable Ticket

Install or expose CMake, then run the documented CMake debug/release/sanitizer builds. In parallel, continue with ORB-027 cache leases and eviction after ORB-026 verification.

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
- `build\manual\orbit_unit_tests.exe` passed: 53 tests, 0 failed.
- CLI workflow passed: `init`, `apply`, `query`, `explain`, and `check`.

## Sanitizer Status

ASan/UBSan and TSan presets exist, but sanitizer runs have not completed because CMake is unavailable and MSVC sanitizer support has not been configured for this project.

## Fuzz Status

Three production-linked fuzz smoke targets exist in `fuzz/`. All three build with the manual MSVC fallback and run successfully against `corpus`.

## Documentation Status

Required documentation files exist and describe the implemented subset and pending work.

## Performance Status

No benchmark has been run. Numeric performance requirements remain unverified.

## Deviations

- ADR-0002 records the OGR development subset and pending full format features.
- ADR-0003 records the currently implemented OQS subset and pending full planner/operator surface.

## Last Verified Commit

`fed3a13` (`feat(query): add cost-aware path ordering`) has passed manual MSVC build/test/fuzz verification. ORB-026 index coverage changes are manually verified and awaiting commit.

## Timestamp

2026-06-26 Africa/Casablanca.
