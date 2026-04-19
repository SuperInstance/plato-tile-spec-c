#ifndef PLATO_TILE_SPEC_H
#define PLATO_TILE_SPEC_H

/*
 * plato-tile-spec C Adapter — Canonical Tile format for C codebases.
 * Maps holodeck-c Room+Note structures to canonical plato-tile-spec Tile.
 * Compatible with Rust serde JSON output of plato_tile_spec::Tile.
 */

#include <stdint.h>
#include <time.h>

/* TileDomain — 14 types, matches Rust TileDomain enum */
typedef enum {
    TILE_DOMAIN_KNOWLEDGE = 0,
    TILE_DOMAIN_PROCEDURAL,
    TILE_DOMAIN_EXPERIENCE,
    TILE_DOMAIN_CONSTRAINT,
    TILE_DOMAIN_NEGATIVE_SPACE,  /* Worth 10x */
    TILE_DOMAIN_BELIEF,
    TILE_DOMAIN_LOCK,
    TILE_DOMAIN_SENTIMENT,
    TILE_DOMAIN_DIAGNOSTIC,
    TILE_DOMAIN_SEMANTIC,
    TILE_DOMAIN_GHOST,
    TILE_DOMAIN_SIMULATION,
    TILE_DOMAIN_ANCHOR,
    TILE_DOMAIN_META,
} TileDomain;

/* Provenance — where a tile came from */
typedef struct {
    char source[128];
    uint32_t generation;
} Provenance;

/* ConstraintBlock — tolerance and threshold */
typedef struct {
    double tolerance;  /* Default: 0.05 */
    double threshold;  /* Default: 0.5 */
} ConstraintBlock;

/* Canonical Tile — matches plato-tile-spec::Tile Rust struct */
typedef struct {
    /* Core */
    char id[64];
    double confidence;
    Provenance provenance;
    TileDomain domain;

    /* Content */
    char question[512];
    char answer[2048];
    char tags[8][64];
    int tag_count;
    char anchors[4][64];
    int anchor_count;

    /* Attention */
    double weight;
    uint64_t use_count;
    int active;
    uint64_t last_used_tick;

    /* Constraints */
    ConstraintBlock constraints;
} PlatoTile;

/* Create a canonical tile from holodeck-c Room + Note */
static PlatoTile tile_from_room_note(
    const char *room_id,
    const char *room_name,
    const char *note_author,
    const char *note_text,
    TileDomain domain
) {
    PlatoTile tile = {0};
    
    /* Generate ID from room + timestamp */
    snprintf(tile.id, sizeof(tile.id), "holo-c-%ld", (long)time(NULL));
    
    tile.confidence = 0.7;
    tile.provenance.generation = 0;
    snprintf(tile.provenance.source, sizeof(tile.provenance.source),
             "holodeck-c:%s", note_author);
    tile.domain = domain;
    
    /* Map room → question, note → answer */
    snprintf(tile.question, sizeof(tile.question),
             "Room %s: %s", room_id, room_name);
    snprintf(tile.answer, sizeof(tile.answer),
             "%s", note_text);
    
    snprintf(tile.tags[0], sizeof(tile.tags[0]), "holodeck");
    snprintf(tile.tags[1], sizeof(tile.tags[1]), "%s", room_id);
    tile.tag_count = 2;
    
    tile.weight = 1.0;
    tile.use_count = 0;
    tile.active = 1;
    tile.last_used_tick = (uint64_t)time(NULL);
    
    tile.constraints.tolerance = 0.05;
    tile.constraints.threshold = 0.5;
    
    return tile;
}

/* Create a tile from a room event (agent action in room) */
static PlatoTile tile_from_event(
    const char *room_id,
    const char *agent_name,
    const char *action,
    const char *outcome,
    double reward
) {
    PlatoTile tile = {0};
    
    snprintf(tile.id, sizeof(tile.id), "holo-c-%s-%ld", room_id, (long)time(NULL));
    tile.confidence = reward;
    tile.provenance.generation = 0;
    snprintf(tile.provenance.source, sizeof(tile.provenance.source),
             "holodeck-c:%s", agent_name);
    tile.domain = TILE_DOMAIN_EXPERIENCE;
    
    snprintf(tile.question, sizeof(tile.question), "%s", action);
    snprintf(tile.answer, sizeof(tile.answer), "%s", outcome);
    
    snprintf(tile.tags[0], sizeof(tile.tags[0]), "holodeck");
    snprintf(tile.tags[1], sizeof(tile.tags[1]), "%s", room_id);
    tile.tag_count = 2;
    
    tile.weight = 1.0;
    tile.use_count = 0;
    tile.active = 1;
    tile.last_used_tick = (uint64_t)time(NULL);
    
    tile.constraints.tolerance = 0.05;
    tile.constraints.threshold = 0.5;
    
    return tile;
}

/* Get domain name string */
static const char *tile_domain_name(TileDomain d) {
    static const char *names[] = {
        "Knowledge", "Procedural", "Experience", "Constraint",
        "NegativeSpace", "Belief", "Lock", "Sentiment",
        "Diagnostic", "Semantic", "Ghost", "Simulation",
        "Anchor", "Meta"
    };
    if (d >= 0 && d < 14) return names[d];
    return "Unknown";
}

#endif /* PLATO_TILE_SPEC_H */
