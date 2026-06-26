# Architecture

## Current Modules

- `include/orbit/error.hpp` and `src/base/error.cpp`: stable error categories and diagnostic strings.
- `include/orbit/value.hpp` and `src/base/value.cpp`: IDs, half-open intervals, property values, limits, and checked arithmetic.
- `include/orbit/format.hpp` and `src/format/ogr.cpp`: OGR-1-inspired superblock, framed transaction records, CRC32C checks, truncation handling, and canonical property ordering.
- `include/orbit/store.hpp` and `src/store/graph_store.cpp`: single-writer transactions, append-only versions, immutable snapshot materialization, reopen, and validation.
- `include/orbit/query.hpp` and `src/query/query.cpp`: OQS tokenization, parsing, explain fingerprints, scan, expand, bounded BFS path execution, and resumable result batches.
- `src/cli/main.cpp`: command-line workflow over the same library APIs.

## Ownership

`GraphStore` owns durable path state and in-memory version maps. `Transaction` is uniquely owned and writer-confined. `GraphSnapshot` is an immutable shared materialization of one commit/time selector. `PreparedQuery` is immutable and shared. `ResultCursor` is uniquely owned by one consumer.

## State Machines

Transactions move from open to committed or aborted. A failed commit leaves the store head unchanged. Cursors move from ready to yielded/done or cancelled; cancellation is sticky and later `next` calls return `Cancelled`.

## Locking

The current implementation uses one store mutex around transaction publication and snapshot materialization. It does not yet implement the full lock hierarchy for background builders, cache shards, or compaction workers.

## Invariants

- Node and edge IDs must be nonzero.
- Entity versions are immutable after commit.
- Snapshot visibility uses the newest version whose begin commit is not newer than the selector.
- Valid-time checks are half-open: start included, end excluded.
- Edges only appear in snapshots when the edge and both endpoints are active at the selected valid time.
- Query output is sorted by canonical node and edge IDs from the materialized snapshot.
