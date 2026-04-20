#include "tile.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

static uint64_t now_ms(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

TileError tile_init(PlatoTile* tile, const char* id, const char* room, const char* content) {
    if (!tile) return TILE_ERR_NULL;
    if (!id || !id[0]) return TILE_ERR_ID_EMPTY;
    if (!room || !room[0]) return TILE_ERR_ROOM_EMPTY;

    memset(tile, 0, sizeof(PlatoTile));
    strncpy(tile->id, id, PLATO_MAX_ID_LEN - 1);
    strncpy(tile->room, room, PLATO_MAX_ROOM_LEN - 1);
    if (content) {
        strncpy(tile->content, content, PLATO_MAX_CONTENT_LEN - 1);
    }
    tile->type = TILE_FACT;
    tile->state = TILE_DRAFT;
    tile->confidence = 0.5f;
    tile->importance = 0.5f;
    tile->version = 1;
    tile->created_at = now_ms();
    tile->updated_at = tile->created_at;
    tile->accessed_at = tile->created_at;
    tile_compute_hash(tile, tile->hash);
    return TILE_OK;
}

TileError tile_validate(const PlatoTile* tile) {
    if (!tile) return TILE_ERR_NULL;
    if (!tile->id[0]) return TILE_ERR_ID_EMPTY;
    if (!tile->content[0]) return TILE_ERR_CONTENT_EMPTY;
    if (strlen(tile->content) >= PLATO_MAX_CONTENT_LEN) return TILE_ERR_CONTENT_TOO_LONG;
    if (!tile->room[0]) return TILE_ERR_ROOM_EMPTY;
    if (tile->state > TILE_DELETED) return TILE_ERR_INVALID_STATE;
    if (tile->type > TILE_NEGATIVE) return TILE_ERR_INVALID_STATE;
    return TILE_OK;
}

TileError tile_set_content(PlatoTile* tile, const char* content) {
    if (!tile || !content) return TILE_ERR_NULL;
    if (strlen(content) >= PLATO_MAX_CONTENT_LEN) return TILE_ERR_CONTENT_TOO_LONG;
    strncpy(tile->content, content, PLATO_MAX_CONTENT_LEN - 1);
    tile->version++;
    tile->updated_at = now_ms();
    tile_compute_hash(tile, tile->hash);
    return TILE_OK;
}

TileError tile_add_tag(PlatoTile* tile, const char* tag) {
    if (!tile || !tag) return TILE_ERR_NULL;
    if (tile->tag_count >= PLATO_MAX_TAGS) return TILE_ERR_TAGS_FULL;
    /* Check duplicate */
    for (uint8_t i = 0; i < tile->tag_count; i++) {
        if (strcmp(tile->tags[i], tag) == 0) return TILE_OK; /* already tagged */
    }
    strncpy(tile->tags[tile->tag_count], tag, PLATO_MAX_ID_LEN - 1);
    tile->tag_count++;
    tile->updated_at = now_ms();
    return TILE_OK;
}

TileError tile_remove_tag(PlatoTile* tile, const char* tag) {
    if (!tile || !tag) return TILE_ERR_NULL;
    for (uint8_t i = 0; i < tile->tag_count; i++) {
        if (strcmp(tile->tags[i], tag) == 0) {
            /* Shift remaining tags down */
            for (uint8_t j = i; j < tile->tag_count - 1; j++) {
                strcpy(tile->tags[j], tile->tags[j + 1]);
            }
            tile->tag_count--;
            tile->updated_at = now_ms();
            return TILE_OK;
        }
    }
    return TILE_OK; /* tag not found, no-op */
}

TileError tile_set_state(PlatoTile* tile, TileState state) {
    if (!tile) return TILE_ERR_NULL;
    if (state > TILE_DELETED) return TILE_ERR_INVALID_STATE;
    tile->state = state;
    tile->updated_at = now_ms();
    return TILE_OK;
}

TileError tile_set_type(PlatoTile* tile, TileType type) {
    if (!tile) return TILE_ERR_NULL;
    if (type > TILE_NEGATIVE) return TILE_ERR_INVALID_STATE;
    tile->type = type;
    tile->updated_at = now_ms();
    return TILE_OK;
}

bool tile_has_tag(const PlatoTile* tile, const char* tag) {
    if (!tile || !tag) return false;
    for (uint8_t i = 0; i < tile->tag_count; i++) {
        if (strcmp(tile->tags[i], tag) == 0) return true;
    }
    return false;
}

void tile_touch(PlatoTile* tile) {
    if (!tile) return;
    tile->accessed_at = now_ms();
    tile->access_count++;
}

/* Simple FNV-1a hash for tile content */
void tile_compute_hash(const PlatoTile* tile, char* out_hash) {
    if (!tile || !out_hash) return;
    uint64_t hash = 14695981039346656037ULL;
    const char* data = tile->content;
    size_t len = strlen(tile->content);
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint64_t)(unsigned char)data[i];
        hash *= 1099511628211ULL;
    }
    snprintf(out_hash, PLATO_HASH_LEN + 1, "%016lx", hash);
}

/* Collection operations */
TileError tile_collection_stats(const PlatoTile* tiles, uint32_t count, TileStats* stats) {
    if (!tiles || !stats) return TILE_ERR_NULL;
    memset(stats, 0, sizeof(TileStats));
    stats->total = count;
    float conf_sum = 0, imp_sum = 0;
    for (uint32_t i = 0; i < count; i++) {
        stats->by_state[tiles[i].state]++;
        stats->by_type[tiles[i].type]++;
        conf_sum += tiles[i].confidence;
        imp_sum += tiles[i].importance;
    }
    if (count > 0) {
        stats->avg_confidence = conf_sum / count;
        stats->avg_importance = imp_sum / count;
    }
    return TILE_OK;
}

uint32_t tile_collection_by_state(const PlatoTile* tiles, uint32_t count, TileState state,
                                  PlatoTile* out, uint32_t max_out) {
    if (!tiles || !out) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < count && n < max_out; i++) {
        if (tiles[i].state == state) out[n++] = tiles[i];
    }
    return n;
}

uint32_t tile_collection_by_type(const PlatoTile* tiles, uint32_t count, TileType type,
                                 PlatoTile* out, uint32_t max_out) {
    if (!tiles || !out) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < count && n < max_out; i++) {
        if (tiles[i].type == type) out[n++] = tiles[i];
    }
    return n;
}

uint32_t tile_collection_by_room(const PlatoTile* tiles, uint32_t count, const char* room,
                                 PlatoTile* out, uint32_t max_out) {
    if (!tiles || !out || !room) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < count && n < max_out; i++) {
        if (strcmp(tiles[i].room, room) == 0) out[n++] = tiles[i];
    }
    return n;
}

uint32_t tile_collection_by_tag(const PlatoTile* tiles, uint32_t count, const char* tag,
                                PlatoTile* out, uint32_t max_out) {
    if (!tiles || !out || !tag) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < count && n < max_out; i++) {
        if (tile_has_tag(&tiles[i], tag)) out[n++] = tiles[i];
    }
    return n;
}

/* Simple substring search */
uint32_t tile_collection_search(const PlatoTile* tiles, uint32_t count, const char* query,
                                PlatoTile* out, uint32_t max_out) {
    if (!tiles || !out || !query) return 0;
    uint32_t n = 0;
    size_t qlen = strlen(query);
    for (uint32_t i = 0; i < count && n < max_out; i++) {
        if (strcasestr(tiles[i].content, query) || strcasestr(tiles[i].id, query)) {
            out[n++] = tiles[i];
        }
    }
    return n;
}

/* Sort helpers */
static int cmp_importance_desc(const void* a, const void* b) {
    float diff = ((const PlatoTile*)b)->importance - ((const PlatoTile*)a)->importance;
    return (diff > 0) ? 1 : (diff < 0) ? -1 : 0;
}
static int cmp_confidence_desc(const void* a, const void* b) {
    float diff = ((const PlatoTile*)b)->confidence - ((const PlatoTile*)a)->confidence;
    return (diff > 0) ? 1 : (diff < 0) ? -1 : 0;
}
static int cmp_accessed_desc(const void* a, const void* b) {
    uint64_t diff = ((const PlatoTile*)b)->accessed_at - ((const PlatoTile*)a)->accessed_at;
    return (diff > 0) ? 1 : (diff < 0) ? -1 : 0;
}

void tile_collection_sort_by_importance(PlatoTile* tiles, uint32_t count, bool descending) {
    if (!tiles || count < 2) return;
    qsort(tiles, count, sizeof(PlatoTile), descending ? cmp_importance_desc : NULL);
}

void tile_collection_sort_by_confidence(PlatoTile* tiles, uint32_t count, bool descending) {
    if (!tiles || count < 2) return;
    qsort(tiles, count, sizeof(PlatoTile), descending ? cmp_confidence_desc : NULL);
}

void tile_collection_sort_by_accessed(PlatoTile* tiles, uint32_t count, bool descending) {
    if (!tiles || count < 2) return;
    qsort(tiles, count, sizeof(PlatoTile), descending ? cmp_accessed_desc : NULL);
}
