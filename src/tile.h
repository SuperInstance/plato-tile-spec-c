/**
 * plato-tile-spec-c — Canonical Tile Format (C binding)
 * 
 * This is the C header for the canonical plato-tile-spec::Tile.
 * Maps 1:1 with the Rust struct defined in plato-tile-spec.
 * 
 * Wire format: tab-delimited (matches plato-tile-current).
 * All strings are null-terminated UTF-8.
 * Timestamps are Unix epoch seconds (i64).
 * Weights and scores are f32.
 * 
 * Use this in holodeck-c, mycorrhizal-relay, and any C/CUDA agent.
 */

#ifndef PLATO_TILE_SPEC_H
#define PLATO_TILE_SPEC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Tile ID: 64-bit, use nanosecond-based nonce generation */
typedef uint64_t tile_id_t;

/* Tile domain */
typedef enum {
    TILE_DOMAIN_KNOWLEDGE = 0,
    TILE_DOMAIN_EXPERIENCE = 1,
    TILE_DOMAIN_CONSTRAINT = 2,
    TILE_DOMAIN_INSTINCT = 3,
    TILE_DOMAIN_SOCIAL = 4,
    TILE_DOMAIN_META = 5,
    TILE_DOMAIN_UNKNOWN = 99
} tile_domain_t;

/* Tile status */
typedef enum {
    TILE_STATUS_ACTIVE = 0,
    TILE_STATUS_DORMANT = 1,
    TILE_STATUS_GHOST = 2,       /* weight < 0.05, awaiting resurrection */
    TILE_STATUS_QUARANTINED = 3,
    TILE_STATUS_ARCHIVED = 4
} tile_status_t;

/* Activation source — who/what activated this tile */
typedef enum {
    TILE_SOURCE_QUERY = 0,
    TILE_SOURCE_REFLEX = 1,      /* instinct engine fired */
    TILE_SOURCE_BRIDGE = 2,      /* fleet I2I relay */
    TILE_SOURCE_FORGE = 3,       /* forge pipeline generated */
    TILE_SOURCE_RESURRECT = 4,   /* afterlife brought it back */
    TILE_SOURCE_UNKNOWN = 99
} tile_source_t;

/* Tile confidence level (maps to plato-unified-belief tiers) */
typedef enum {
    TILE_CONF_HIGH = 0,          /* belief > 0.7 */
    TILE_CONF_MEDIUM = 1,        /* 0.3 < belief <= 0.7 */
    TILE_CONF_LOW = 2,           /* belief <= 0.3 */
    TILE_CONF_UNKNOWN = 99
} tile_confidence_t;

#define TILE_CONTENT_MAX 4096
#define TILE_TAGS_MAX 16
#define TILE_TAG_LEN 64

/**
 * Canonical Tile struct — mirrors plato-tile-spec::Tile exactly.
 * 
 * Memory layout is designed for:
 *   - Cache-line friendly access (metadata first, content last)
 *   - Zero-copy serialization to tab-delimited wire format
 *   - CUDA-compatible (no heap indirection in fixed arrays)
 */
typedef struct {
    /* Identity */
    tile_id_t id;
    tile_domain_t domain;
    tile_status_t status;
    
    /* Content */
    char content[TILE_CONTENT_MAX];
    uint32_t content_len;
    
    /* Scoring */
    float weight;           /* 0.0-1.0, decayed by afterlife */
    float belief;           /* unified belief score (conf+trust+relevance) */
    tile_confidence_t confidence;
    
    /* Activation */
    tile_source_t source;
    uint64_t created_at;    /* epoch seconds */
    uint64_t last_used;     /* epoch seconds */
    uint32_t use_count;
    
    /* Gene linkage (plato-genepool-tile) */
    tile_id_t gene_id;      /* 0 = no gene linkage */
    float gene_fitness;     /* EV score from genepool */
    
    /* Tags */
    char tags[TILE_TAGS_MAX][TILE_TAG_LEN];
    uint8_t tag_count;
    
    /* Instinct linkage (plato-instinct) */
    tile_source_t instinct_trigger;  /* which instinct activated this */
    float instinct_urgency;         /* urgency at activation time */
} plato_tile_t;

/* ---- Lifecycle ---- */

/**
 * Initialize a tile with defaults.
 * Sets id=0, domain=UNKNOWN, status=ACTIVE, weight=1.0, belief=0.0.
 */
void plato_tile_init(plato_tile_t *tile);

/**
 * Generate a unique tile ID from nanosecond clock + counter.
 * Thread-safe on most platforms (uses atomics where available).
 */
tile_id_t plato_tile_generate_id(void);

/**
 * Generate tile ID deterministically from content hash (FNV-1a).
 * Use when the same content should produce the same ID.
 */
tile_id_t plato_tile_hash_content(const char *content, uint32_t len);

/* ---- Tags ---- */

/**
 * Add a tag to the tile. No-op if already present or tag_count >= TILE_TAGS_MAX.
 * Returns true if added, false if duplicate or full.
 */
bool plato_tile_add_tag(plato_tile_t *tile, const char *tag);

/**
 * Check if tile has a specific tag.
 */
bool plato_tile_has_tag(const plato_tile_t *tile, const char *tag);

/* ---- Scoring ---- */

/**
 * Compute unified belief score from components.
 * belief = 0.5*confidence + 0.3*trust + 0.2*relevance
 * Maps to confidence tier automatically.
 */
float plato_tile_compute_belief(float confidence, float trust, float relevance);

/**
 * Apply decay to weight based on time since last_used.
 * decay = exp(-lambda * seconds_since_used)
 * Returns new weight (clamped to [0.0, 1.0]).
 */
float plato_tile_decay_weight(float weight, uint64_t last_used, float lambda);

/* ---- Ghost/Afterlife ---- */

/**
 * Check if tile should become a ghost (weight < threshold).
 * Returns true if weight < 0.05.
 */
bool plato_tile_should_ghost(const plato_tile_t *tile, float threshold);

/**
 * Resurrect a ghost tile — apply relevance discount.
 * Sets status=ACTIVE, weight = relevance * 0.5 (resurrection discount).
 * Returns true if resurrection happened.
 */
bool plato_tile_resurrect(plato_tile_t *tile, float relevance);

/* ---- Serialization ---- */

/**
 * Serialize tile to tab-delimited wire format.
 * Output is written to buf (must be at least 8192 bytes).
 * Returns number of bytes written (excluding null terminator).
 * Format: id\tdomain\tstatus\tweight\tbelief\tcontent\tcreated\ttags...
 */
int plato_tile_serialize(const plato_tile_t *tile, char *buf, int buf_size);

/**
 * Deserialize tile from tab-delimited wire format.
 * Returns true on success, false on parse error.
 */
bool plato_tile_deserialize(const char *buf, plato_tile_t *tile);

/* ---- Validation ---- */

/**
 * Validate tile invariants.
 * Returns 0 if valid, or a bitmask of TILE_ERR_* flags.
 */
#define TILE_ERR_ZERO_ID        (1 << 0)
#define TILE_ERR_WEIGHT_RANGE   (1 << 1)
#define TILE_ERR_BELIEF_RANGE   (1 << 2)
#define TILE_ERR_CONTENT_EMPTY  (1 << 3)
#define TILE_ERR_CONTENT_LEN    (1 << 4)
#define TILE_ERR_TAG_TOO_LONG   (1 << 5)
int plato_tile_validate(const plato_tile_t *tile);

#ifdef __cplusplus
}
#endif

#endif /* PLATO_TILE_SPEC_H */
