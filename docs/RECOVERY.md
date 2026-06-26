# Recovery

Current recovery behavior is intentionally conservative:

- The reader starts from the fixed OGR superblock.
- It scans framed records from offset 4096.
- Only a complete checksum-valid transaction group ending in `TXN_COMMIT` advances the visible commit.
- Truncated tails are ignored and expose the previous committed prefix.
- Complete records with checksum or semantic integrity errors reject open/check.

Pending full-version recovery work includes dual superblocks, checkpoints, fsync boundary fault injection, torn-write matrices, compaction candidate validation, durable catalog publication, and idempotent salvage tooling.
