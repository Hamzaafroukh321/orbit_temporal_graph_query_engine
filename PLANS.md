# Orbit Implementation Plan

## Selected Specification

`09_orbit_temporal_graph_query_engine.md` is the governing specification because it matches the repository name, uses the numbered architecture format, and contains the required project identity, fuzzing, MVP, and full-version acceptance sections.

## Architecture Summary

Orbit is a C++20 embedded temporal property-graph engine. The implementation is organized around checked primitives, OGR-1 binary persistence, immutable commit snapshots, transaction builders, deterministic query parsing/planning/execution, bounded cursors, recovery checks, and fuzzable state sequences.

## Phases

1. Foundation: CMake presets, warnings, sanitizers, test and fuzz targets, project controls.
2. Primitives: errors, results, IDs, intervals, checked arithmetic, values and limits.
3. Format: canonical OGR-1 superblock/record writer and reader with deterministic rejection.
4. Store: transaction commits, snapshots, node/edge/property versions, reopen and validation.
5. Query: OQS lexer/parser, semantic checks, scans, one-hop expansion, bounded paths and cursors.
6. Hardening: recovery cut points, cancellation, fault injection, fuzz targets and sanitizer matrix.
7. Full-version features: background indexes, compaction, concurrency, subscriptions, performance gates and packaging.

## Dependency Graph

Foundation -> Primitives -> Format -> Store -> Query -> Hardening -> Full-version features.

## Requirement Groups

- R-FND: Build, tooling, status, traceability, assisted-development provenance.
- R-BASE: Checked arithmetic, IDs, errors, intervals, values and resource limits.
- R-FMT: OGR-1 headers, records, canonical encoding, truncation and integrity behavior.
- R-STORE: Transactions, commits, snapshots, version visibility, stable IDs and reopen.
- R-QUERY: OQS parsing, planning, operators, batches, ordering and diagnostics.
- R-FUZZ: Parser, graph-sequence and query-pipeline fuzz harnesses.
- R-DOC: README, architecture, format, testing, fuzzing, recovery, security and performance docs.

## Validation Commands

- `cmake --preset debug`
- `cmake --build --preset debug`
- `ctest --preset debug`
- `cmake --preset release && cmake --build --preset release`
- `cmake --preset asan && cmake --build --preset asan && ctest --preset asan`
- `cmake --preset tsan && cmake --build --preset tsan && ctest --preset tsan`

## Risks And Mitigation

- Scope: keep the first vertical slice small and trace all unsupported full-version work as pending.
- Format ambiguity: document the implemented subset and reject unsupported required semantics.
- Recovery correctness: make incomplete transactions invisible before adding compaction or background work.
- Query determinism: sort all visible IDs and path ties canonically.

## Definition Of Done

Full completion requires every requirement in `docs/REQUIREMENTS_TRACEABILITY.md` to be verified or honestly blocked, clean debug/release/sanitized builds, passing unit/integration/fuzz smoke tests, current documentation, and a clean working tree except for documented user-owned files.

## MVP Checklist

- [ ] OGR-1 stores can be created, reopened, checked, and inspected.
- [ ] Atomic node/edge/property mutations persist with half-open valid-time intervals.
- [ ] Immutable snapshots preserve old commit/time results.
- [ ] Label, property, adjacency and temporal filters match full scans.
- [ ] OQS core parses and produces deterministic plans.
- [ ] Point, scan, one-hop, and bounded path queries stream in stable batches.
- [ ] Recovery exposes only complete committed prefixes.
- [ ] Malformed input, cancellation and resource limits leave committed state unchanged.
- [ ] At least 30 named tests and three fuzz targets pass.
- [ ] Required MVP documentation exists and matches code.

## Full-Version Checklist

- [ ] Background index builds and partial coverage planning.
- [ ] Compaction with relocation and pinned generation protection.
- [ ] Stateful sequence fuzzing against the independent reference model.
- [ ] Parallel query mode with identical normative ordering.
- [ ] Long-running recovery, compaction and concurrency soaks.
- [ ] Performance budgets measured on documented hardware.
