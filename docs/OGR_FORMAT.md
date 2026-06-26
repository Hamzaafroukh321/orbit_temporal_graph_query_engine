# OGR Format

The current store uses an OGR-1-compatible development subset.

## Superblock

The file starts with a 4096-byte superblock:

| Field | Encoding |
|---|---|
| Magic | `OGR1` |
| major/minor | little-endian u16/u16 |
| header bytes | little-endian u32, always 4096 |
| generation | little-endian u64 |
| latest commit | little-endian u64 |
| record count hint | little-endian u64 |
| header CRC32C | little-endian u32 over preceding header bytes |

## Records

Records are 8-byte aligned and use this fixed header:

| Field | Encoding |
|---|---|
| type | u16 |
| schema | u16, currently 1 |
| flags | u32, currently 0 |
| payload length | u64 |
| logical id | u64 |
| commit sequence | u64 |
| header CRC32C | u32 |
| payload CRC32C | u32 |

Supported required record types are `TXN_BEGIN`, `NODE_VERSION`, `EDGE_VERSION`, and `TXN_COMMIT`.

## Visibility

Only contiguous groups from `TXN_BEGIN` through a checksum-valid `TXN_COMMIT` are loaded. A truncated tail is ignored and exposes the previous committed prefix. Integrity failures in a complete record reject the store.

## Compatibility

`fixtures/compat/v0_1` contains a small mutation/query fixture whose generated
store is reopened and queried by `scripts\compat_msvc.cmd`.

## Pending

Dual superblock publication, optional/required feature maps, segment catalogs,
checkpoints, compaction maps, large blob streaming, and full-version golden
fixture matrices remain pending.
