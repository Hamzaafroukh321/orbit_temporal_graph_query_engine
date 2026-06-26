# Architecture

## Current Modules

- `include/orbit/error.hpp` and `src/base/error.cpp`: stable error categories and diagnostic strings.
- `include/orbit/value.hpp` and `src/base/value.cpp`: IDs, half-open intervals, property values, limits, and checked arithmetic.
- `include/orbit/format.hpp` and `src/format/ogr.cpp`: OGR-1-inspired superblock, framed transaction records, CRC32C checks, truncation handling, and canonical property ordering.
- `include/orbit/store.hpp` and `src/store/graph_store.cpp`: single-writer transactions, append-only versions, commit-visible version lookup, explicit temporal interval selection, immutable snapshot materialization, snapshot-local label/property/adjacency indexes, reopen, and validation.
- `include/orbit/query.hpp` and `src/query/query.cpp`: OQS tokenization, parsing, explain fingerprints, indexed scan seeds, indexed adjacency expansion, resource-bounded BFS path execution, optional edge-cost path ordering, stable continuation keys, and resumable result batches.
- `src/cli/main.cpp`: command-line workflow over the same library APIs.

## Ownership

`GraphStore` owns durable path state, in-memory version maps, and a generation lease registry. `Transaction` is uniquely owned and writer-confined. `GraphSnapshot` is an immutable shared materialization of one commit/time selector and pins its index generation until the final shared snapshot implementation is released. `PreparedQuery` is immutable and shared. `ResultCursor` is uniquely owned by one consumer.

## State Machines

Transactions move from open to committed or aborted. A failed commit leaves the store head unchanged. Cursors move from ready to yielded/done or cancelled; cancellation is sticky and later `next` calls return `Cancelled`.

## Locking

The current implementation uses one store mutex around transaction publication and snapshot materialization plus a separate lease-registry mutex for generation pin accounting. It does not yet implement the full lock hierarchy for background builders, cache shards, or compaction workers.

## Invariants

- Node and edge IDs must be nonzero.
- Entity versions are immutable after commit.
- Snapshot visibility uses the newest version whose begin commit is not newer than the selector.
- Valid-time checks are half-open: start included, end excluded.
- Snapshot materialization separates commit visibility from temporal interval selection before publishing indexed views.
- Edges only appear in snapshots when the edge and both endpoints are active at the selected valid time.
- Snapshot indexes are rebuilt from canonical materialized vectors, so indexed query output remains scan-equivalent and stable.
- Snapshot indexes declare a generation and commit coverage boundary; synchronous snapshot-local indexes cover exactly the snapshot commit.
- Cache eviction removes only unpinned generations older than the latest registered generation.
- The current compaction stage plans keep-last-commit retention and reports whether a candidate is publishable under active generation pins; it does not rewrite or publish replacement storage yet.
- Result batches carry value-based continuation keys derived from node IDs, edge IDs, and path IDs rather than raw iterators.
- Path execution rejects hop/frontier limits explicitly and prevents repeated nodes within a path.
- Cost-aware path mode accepts a numeric nonnegative edge property and orders materialized bounded paths by cumulative cost with continuation-key ties.
