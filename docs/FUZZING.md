# Fuzzing

Three smoke harnesses are present:

- `orbit_ogr_parser_fuzz`: creates, appends, and reopens an OGR store through production format code.
- `orbit_graph_sequence_fuzz`: applies a deterministic graph sequence through production store/query APIs.
- `orbit_query_pipeline_fuzz`: parses and executes an OQS property-filter query against a small production store.

These are smoke harnesses, not full libFuzzer campaigns yet. They are bounded, deterministic, use local files under the supplied corpus directory, and do not execute arbitrary input.

Run after building:

```sh
build/debug/orbit_ogr_parser_fuzz corpus
build/debug/orbit_graph_sequence_fuzz corpus
build/debug/orbit_query_pipeline_fuzz corpus
```

With the manual MSVC fallback, use the corresponding executables in `build\manual\`.

Pending work includes byte-driven libFuzzer entry points, original seed corpora, dictionaries, minimization workflow, and sequence comparison against an independent reference model.
