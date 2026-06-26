# Security

Orbit treats local store files, mutation scripts, and OQS text as untrusted input.

## Current Defensive Behavior

- Store records carry explicit lengths and CRC32C checks.
- Payload lengths, property counts, strings, query bytes, row counts, and hop bounds have configurable limits.
- Invalid intervals, unknown required record types, malformed query syntax, and invalid IDs return typed errors.
- The CLI does not execute commands from input files.

## Supported Versions

No stable release exists yet. The current format is a development subset of OGR-1.

## Reporting

Report security issues privately to the repository owner. Do not include secrets or unrelated local paths in diagnostics.

## Pending

Full sanitizer evidence, fuzz campaigns, static analysis, allocation-failure matrices, cache/compaction concurrency hardening, and binary-format patent review are pending.
