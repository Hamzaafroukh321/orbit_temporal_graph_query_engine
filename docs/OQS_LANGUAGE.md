# OQS Language

The implemented OQS subset is original and intentionally small:

```text
[AT COMMIT <selector> TIME <integer>] FROM <Label>
  [WHERE <property> = <literal>]
  [STEP OUT <Type> | PATH OUT <Type> [HOPS <integer>]]
  YIELD node.id | edge.id | path
```

`AT` selectors are accepted by the parser, but execution receives its snapshot from the API or CLI `--time` option. Query literals support booleans, signed integers, and strings.

## Ordering

Scans emit ascending node IDs. One-hop expansion iterates seed nodes, then ascending edge IDs from the snapshot. Bounded paths use breadth-first expansion with the same edge order and reject repeated nodes in a path.

## Diagnostics

Syntax errors include a byte source range when the failing token is known.

## Pending

Typed parameters, richer predicates, cost-aware paths, parallel execution, explicit order clauses, and compatibility fixtures remain pending.
