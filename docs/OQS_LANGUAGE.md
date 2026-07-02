# OQS Language

The implemented OQS subset is original and intentionally small:

```text
[AT COMMIT <selector> TIME <integer>] FROM <Label>
  [WHERE <property> <op> <literal>]
  [STEP <IN|OUT> <Type> | PATH <IN|OUT> <Type> [HOPS <integer>] [COST <edge-property>]]
  YIELD node.id | edge.id | path
```

`AT` selectors are accepted by the parser, but execution receives its snapshot from the API or CLI `--time` option. Query literals support booleans, signed integers, finite doubles, and strings.

`WHERE` supports `=`, `!=`, `<`, `<=`, `>`, and `>=`. Equality uses the
snapshot property index for exact value lookups. Other predicates scan the
selected label and apply a typed filter. Numeric predicates compare signed
integers and doubles together. String predicates use bytewise lexical ordering.
Boolean predicates support only `=` and `!=`; range operators on booleans or
incompatible property/literal types return `QueryType`.

## Ordering

Scans emit ascending node IDs. One-hop expansion iterates seed nodes, then
ascending edge IDs from the snapshot. `OUT` follows edge `from -> to`; `IN`
follows the same active edge in reverse from `to -> from`. Bounded paths use
breadth-first expansion with the same edge order and reject repeated nodes in a
path.

When `COST` is present on a path clause, every traversed edge must contain a finite nonnegative integer or double property with that name. Bounded paths are ordered by cumulative cost, then by stable path continuation key.

## Diagnostics

Syntax errors include a byte source range when the failing token is known.

## Compatibility

The current compatibility fixture set is `fixtures/compat/v0_1`; run
`scripts\compat_msvc.cmd` to compare CLI query and explain output with the
checked-in expected files.

## Pending

Typed parameters, parallel execution, and explicit order clauses remain pending.
