#ifndef PLATO_TILE_SPEC_H
#define PLATO_TILE_SPEC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define PLATO_TILE_VERSION 2
#define PLATO_MAX_CONTENT_LEN 65536
#define PLATO_MAX_TAGS 32
#define PLATO_MAX_ID_LEN 128
#define PLATO_MAX_ROOM_LEN 128
#define PLATO_HASH_LEN 64

/* Tile lifecycle states */
typedef enum {
    TILE_DRAFT = 0,
    TILE_ACTIVE = 1,
    TILE_PINNED = 2,
    TILE_DEPRECATED = 3,
    TILE_ARCHIVED = 4,
    TILE_DELETED = 5
} TileState;

/* Tile types */
typedef enum {
    TILE_FACT = 0,
    TILE_RULE = 1,
    TILE_PROCEDURE = 2,
    TILE_QUESTION = 3,
    TILE_ANSWER = 4,
    TILE_META = 5,
    TILE_NEGATIVE = 6
} TileType;

/* Validation result codes */
typedef enum {
    TILE_OK = 0,
    TILE_ERR_NULL = -1,
    TILE_ERR_ID_EMPTY = -2,
    TILE_ERR_CONTENT_EMPTY = -3,
    TILE_ERR_CONTENT_TOO_LONG = -4,
    TILE_ERR_ROOM_EMPTY = -5,
    TILE_ERR_TAGS_FULL = -6,
    TILE_ERR_INVALID_STATE = -7,
    TILE_ERR_VERSION_CONFLICT = -8
} TileError;

typedef struct {
    char id[PLATO_MAX_ID_LEN];
    char room[PLATO_MAX_ROOM_LEN];
    char content[PLATO_MAX_CONTENT_LEN];
    char tags[PLATO_MAX_TAGS][PLATO_MAX_ID_LEN];
    uint8_t tag_count;
    TileType type;
    TileState state;
    float confidence;
    float importance;
    uint32_t version;
    uint64_t created_at;
    uint64_t updated_at;
    uint64_t accessed_at;
    uint32_t access_count;
    char hash[PLATO_HASH_LEN + 1];
} PlatoTile;

typedef struct {
    uint32_t total;
    uint32_t by_state[6];
    uint32_t by_type[7];
    float avg_confidence;
    float avg_importance;
} TileStats;

/* Core operations */
TileError tile_init(PlatoTile* tile, const char* id, const char* room, const char* content);
TileError tile_validate(const PlatoTile* tile);
TileError tile_set_content(PlatoTile* tile, const char* content);
TileError tile_add_tag(PlatoTile* tile, const char* tag);
TileError tile_remove_tag(PlatoTile* tile, const char* tag);
TileError tile_set_state(PlatoTile* tile, TileState state);
TileError tile_set_type(PlatoTile* tile, TileType type);
bool tile_has_tag(const PlatoTile* tile, const char* tag);
void tile_touch(PlatoTile* tile);

/* Hash */
void tile_compute_hash(const PlatoTile* tile, char* out_hash);

/* Collection operations (static array) */
TileError tile_collection_stats(const PlatoTile* tiles, uint32_t count, TileStats* stats);
uint32_t tile_collection_by_state(const PlatoTile* tiles, uint32_t count, TileState state,
                                  PlatoTile* out, uint32_t max_out);
uint32_t tile_collection_by_type(const PlatoTile* tiles, uint32_t count, TileType type,
                                 PlatoTile* out, uint32_t max_out);
uint32_t tile_collection_by_room(const PlatoTile* tiles, uint32_t count, const char* room,
                                 PlatoTile* out, uint32_t max_out);
uint32_t tile_collection_by_tag(const PlatoTile* tiles, uint32_t count, const char* tag,
                                PlatoTile* out, uint32_t max_out);
uint32_t tile_collection_search(const PlatoTile* tiles, uint32_t count, const char* query,
                                PlatoTile* out, uint32_t max_out);
void tile_collection_sort_by_importance(PlatoTile* tiles, uint32_t count, bool descending);
void tile_collection_sort_by_confidence(PlatoTile* tiles, uint32_t count, bool descending);
void tile_collection_sort_by_accessed(PlatoTile* tiles, uint32_t count, bool descending);

#endif /* PLATO_TILE_SPEC_H */
