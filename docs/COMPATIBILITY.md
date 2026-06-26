# Compatibility

The current compatibility fixture set is `fixtures/compat/v0_1`.

It freezes the observable behavior of the implemented development subset:

- CLI `init` and `apply` over the text mutation format.
- OGR reopen through the normal store open path.
- OQS label scan, temporal filtering, one-hop expansion, and path explain output.
- Stable row ordering for the fixture graph at valid times 10 and 25.

Run:

```bat
scripts\compat_msvc.cmd
```

The verifier creates a fresh store under `build\manual\compat\v0_1`, applies the
fixture mutation file, runs each fixture query or explain command, and compares
the output with the expected files.

This is not a stable public release contract. It is a pre-1.0 compatibility
fixture for detecting accidental changes while the OGR/OQS development subset
continues to mature.
