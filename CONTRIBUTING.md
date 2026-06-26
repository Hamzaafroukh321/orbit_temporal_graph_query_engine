# Contributing

Use `09_orbit_temporal_graph_query_engine.md` as the governing specification.

## Workflow

- Keep changes scoped to one coherent requirement group.
- Add or update tests with production behavior.
- Update `docs/IMPLEMENTATION_STATUS.md` and `docs/REQUIREMENTS_TRACEABILITY.md`.
- Record material architecture choices in `docs/DECISIONS.md`.
- Do not vendor large dependencies or replace core storage/query behavior with third-party engines.

## Style

- C++20, RAII, deterministic ordering, checked arithmetic before allocation/indexing.
- Public APIs return `Result<T>` with stable error categories.
- Avoid raw pointer ownership and storing views into movable buffers.

## Review Checklist

- Does the change preserve commit-time and valid-time semantics?
- Are malformed inputs rejected deterministically?
- Are resource limits enforced before growth?
- Do tests exercise failure paths and boundaries?
- Does documentation describe actual behavior?
