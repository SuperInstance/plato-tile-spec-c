# plato-tile-spec-c

C adapter for the canonical `plato-tile-spec::Tile` format. Maps C structures to the Rust serde-compatible JSON wire format.

## Usage

```c
#include "plato_tile_spec.h"

// From a holodeck-c room event
PlatoTile tile = tile_from_event("harbor", "oracle1", "enter_room", "Harbor loaded", 0.8);
printf("Domain: %s\n", tile_domain_name(tile.domain));
printf("Source: %s\n", tile.provenance.source);
```

## TileDomain (14 types)

Knowledge, Procedural, Experience, Constraint, **NegativeSpace** (10x), Belief, Lock, Sentiment, Diagnostic, Semantic, Ghost, Simulation, Anchor, Meta.

## Compatibility

Output matches `plato_tile_spec::Tile` Rust serde JSON exactly. Same field names, same types, same wire format.
