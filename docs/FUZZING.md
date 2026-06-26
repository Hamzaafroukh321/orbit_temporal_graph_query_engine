# Fuzzing

Three bounded byte-driven smoke harnesses are present:

- `orbit_ogr_parser_fuzz`: creates a canonical store, mutates truncation points from seed bytes, and reopens each variant through production format/recovery code.
- `orbit_graph_sequence_fuzz`: interprets seed bytes as bounded create/delete/abort/query/reopen graph operations through production store/query APIs.
- `orbit_query_pipeline_fuzz`: maps seed bytes to OQS variants, snapshot selectors, batch sizes, and cancellation cases through production parser/planner/operator code.

These are smoke harnesses, not full libFuzzer campaigns yet. They are bounded, deterministic, use local files under the supplied corpus directory, and do not execute arbitrary input.

Run after building:

```sh
build/debug/orbit_ogr_parser_fuzz corpus
build/debug/orbit_graph_sequence_fuzz corpus
build/debug/orbit_query_pipeline_fuzz corpus
```

With the manual MSVC fallback, use the corresponding executables in `build\manual\`.

For the current regression gate, run:

```bat
scripts\run_regression_matrix.cmd
```

The smoke harnesses create temporary files under the supplied corpus directory.
Generated smoke files are ignored by Git; minimized human-reviewed regression
inputs should be added as named fixtures rather than committed from automatic
run output.

Pending work includes native libFuzzer entry points, richer original seed corpora, dictionaries, minimization workflow, and sequence comparison against an independent reference model.
