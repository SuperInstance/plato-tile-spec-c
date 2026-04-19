# plato-tile-spec-c

Canonical Tile format in C — the C binding for [plato-tile-spec](https://github.com/SuperInstance/plato-tile-spec).

## Why

The fleet has 4+ incompatible tile definitions across Rust and C codebases. This header declares THE canonical tile format. Use it in holodeck-c, mycorrhizal-relay, flux-runtime-c, and any C/CUDA agent.

## What

- `tile.h` — Header with struct, enums, and full API
- `tile.c` — Implementation (lifecycle, tags, scoring, ghost/afterlife, serialization, validation)
- `tests/test_tile.c` — 30 tests

## Wire Format

Tab-delimited (matches `plato-tile-current`). Pipes in content are preserved natively.

## API Highlights

| Function | Purpose |
|----------|---------|
| `plato_tile_init()` | Zero-initialize with defaults |
| `plato_tile_generate_id()` | Nanosecond-based unique ID |
| `plato_tile_hash_content()` | Deterministic FNV-1a content hash |
| `plato_tile_add_tag()` / `has_tag()` | Tag management (max 16) |
| `plato_tile_compute_belief()` | Unified belief: 0.5*conf + 0.3*trust + 0.2*relevance |
| `plato_tile_decay_weight()` | Exponential weight decay |
| `plato_tile_should_ghost()` | Weight < threshold → ghost status |
| `plato_tile_resurrect()` | Ghost → active with 0.5 relevance discount |
| `plato_tile_serialize()` / `deserialize()` | Tab-delimited wire format |
| `plato_tile_validate()` | Invariant checker with error bitmask |

## Build

```bash
gcc -std=c99 -O2 -Isrc -o test_tile tests/test_tile.c src/tile.c -lm
./test_tile
```

## Convergence

This struct maps 1:1 to `plato-tile-spec::Tile` in Rust. All fleet systems should converge on this format.

## License

MIT
