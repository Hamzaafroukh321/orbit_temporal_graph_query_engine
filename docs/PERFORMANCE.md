# Performance

The current benchmark smoke target is `orbit_benchmark`, runnable through:

```bat
scripts\benchmark_msvc.cmd
```

It creates a small deterministic graph, profiles mutation publication, snapshot
point lookups, one-hop expansion, and bounded path execution, then writes
key-value metrics to `build\manual\bench\benchmark.txt`.

## Local Baseline

On 2026-06-26 in this agent environment, the manual MSVC debug-style fallback
produced:

| Metric | Value |
| --- | ---: |
| Entity versions | 2001 |
| Mutation throughput | 18906.1 versions/s |
| Point lookup mean | 0.778765 us |
| One-hop traversal | 383642 rows/s |
| Bounded path traversal | 157067 rows/s |

These numbers are a smoke baseline, not final acceptance evidence. Reference
release builds, hardware details, memory ceilings, recovery startup, compaction
throughput, and long-run variance are still pending.

## Implemented Limits

- Maximum query text: 1 MiB by default.
- Maximum record payload: 32 MiB by default.
- Maximum property count: 1024 by default.
- Maximum string size: 1 MiB by default.
- Maximum path hops: 32 by default.
- Maximum query rows: 10000 by default.

## Pending

Mutation throughput, lookup latency, traversal throughput, recovery startup,
compaction throughput, memory ceilings, and fuzz executions-per-second must be
measured on documented release hardware before any full performance acceptance
criterion is marked verified.
