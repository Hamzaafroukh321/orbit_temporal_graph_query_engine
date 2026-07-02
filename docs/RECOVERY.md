# Recovery

Current recovery behavior is intentionally conservative:

- The reader starts from the fixed OGR superblock.
- It scans framed records from offset 4096.
- Only a complete checksum-valid transaction group ending in `TXN_COMMIT` advances the visible commit.
- Truncated tails are ignored and expose the previous committed prefix.
- Complete records with checksum or semantic integrity errors reject open/check.
- Compaction publication currently runs retained commit/time snapshot and
  snapshot-index semantic probes before publishing replacement generation
  metadata.

Pending full-version recovery work includes dual superblocks, checkpoints, fsync boundary fault injection, torn-write matrices, relocated compaction candidate validation, durable catalog publication, and idempotent salvage tooling.

## Soak Smoke

`scripts\soak_msvc.cmd` runs `orbit_soak`, which repeatedly commits edges,
keeps selected snapshots alive, attempts compaction, reopens the store, and
checks query row counts after each transition.

The 2026-06-26 local run used 25 cycles and produced:

```text
cycles=25
commits=26
compaction_attempts=25
compaction_successes=16
compaction_blocked=9
reopens=3
held_snapshot_checks=18
final_rows=25
```
