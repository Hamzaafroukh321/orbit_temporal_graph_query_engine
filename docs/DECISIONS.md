# Architecture Decisions

## ADR-0001: Start With A Small First-Party C++20 Core

### Context

The specification requires a C++20 embedded temporal graph engine with a custom binary format and original query syntax. The repository starts from an empty Git history and must remain buildable while implementation proceeds.

### Decision

Use a single first-party library target, a CLI target, plain CTest-based executables, and no production dependencies beyond the C++ standard library for the foundation and first vertical slice.

### Alternatives Considered

- Add third-party parser, serialization or test frameworks immediately.
- Build a CLI-only prototype before defining a reusable library boundary.

### Consequences

The first implementation has more local utility code, but core storage and query behavior remain reviewable and dependency-light. Dependency decisions can be made later with an explicit ADR if a real need appears.

### Validation

Validate with CMake debug/release/sanitizer presets and CTest.

## ADR-0002: Implement A Recoverable OGR-1 Development Subset First

### Context

The full OGR-1 format includes dual superblocks, segment catalogs, checkpoints, feature maps, compaction maps, digest validation, and large streamed blobs. A first useful path still needs durable stores and reopen behavior.

### Decision

Implement a development subset with a fixed 4096-byte superblock, checksummed aligned records, `TXN_BEGIN`, `NODE_VERSION`, `EDGE_VERSION`, and `TXN_COMMIT` groups. The reader exposes only complete valid groups and ignores truncated tails.

### Alternatives Considered

- Delay persistence until the in-memory graph is complete.
- Implement the full segment/checkpoint/compaction format before any query workflow.

### Consequences

The CLI and tests use real durable bytes early, while full compatibility and recovery requirements remain pending and clearly documented.

### Validation

Format tests and `orbit_ogr_parser_fuzz` are present. Execution is blocked in this environment until CMake and a C++20 compiler are available.

## ADR-0003: Keep The First OQS Surface Small And Deterministic

### Context

The specification requires an original query language with scans, expansion, bounded paths, plans, and stable ordering.

### Decision

Implement the subset `FROM`, optional equality `WHERE`, `STEP OUT`, `PATH OUT HOPS`, and `YIELD node.id|edge.id|path`. Execution receives an immutable snapshot and emits materialized result batches.

### Alternatives Considered

- Add a parser generator dependency.
- Implement richer predicates before storage and snapshot behavior were available.

### Consequences

The implemented language is intentionally small but exercises production store/query boundaries. Full semantic binding, parameter typing, cost-aware paths, and parallel modes remain pending.

### Validation

Parser, explain, batch, expansion, path, and cancellation tests are present. Execution is blocked in this environment until CMake and a C++20 compiler are available.
