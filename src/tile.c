#define _POSIX_C_SOURCE 199309L
#include "tile.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

/* ---- Internal: FNV-1a hash ---- */
static uint64_t fnv1a(const char *data, size_t len) {
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint64_t)(unsigned char)data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

/* ---- ID Generation ---- */
static uint64_t id_counter = 0;

tile_id_t plato_tile_generate_id(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    __sync_fetch_and_add(&id_counter, 1);
    return ns ^ (id_counter * 0x9E3779B97F4A7C15ULL);
}

tile_id_t plato_tile_hash_content(const char *content, uint32_t len) {
    return fnv1a(content, len);
}

/* ---- Lifecycle ---- */

void plato_tile_init(plato_tile_t *tile) {
    memset(tile, 0, sizeof(plato_tile_t));
    tile->id = 0;
    tile->domain = TILE_DOMAIN_UNKNOWN;
    tile->status = TILE_STATUS_ACTIVE;
    tile->weight = 1.0f;
    tile->belief = 0.0f;
    tile->confidence = TILE_CONF_UNKNOWN;
    tile->source = TILE_SOURCE_UNKNOWN;
    tile->gene_id = 0;
    tile->gene_fitness = 0.0f;
    tile->instinct_trigger = TILE_SOURCE_UNKNOWN;
    tile->instinct_urgency = 0.0f;
    tile->tag_count = 0;
}

/* ---- Tags ---- */

bool plato_tile_add_tag(plato_tile_t *tile, const char *tag) {
    if (tile->tag_count >= TILE_TAGS_MAX) return false;
    for (uint8_t i = 0; i < tile->tag_count; i++) {
        if (strcmp(tile->tags[i], tag) == 0) return false;
    }
    size_t tag_len = strlen(tag);
    if (tag_len >= TILE_TAG_LEN) return false;
    strcpy(tile->tags[tile->tag_count], tag);
    tile->tag_count++;
    return true;
}

bool plato_tile_has_tag(const plato_tile_t *tile, const char *tag) {
    for (uint8_t i = 0; i < tile->tag_count; i++) {
        if (strcmp(tile->tags[i], tag) == 0) return true;
    }
    return false;
}

/* ---- Scoring ---- */

float plato_tile_compute_belief(float confidence, float trust, float relevance) {
    float b = 0.5f * confidence + 0.3f * trust + 0.2f * relevance;
    if (b < 0.0f) b = 0.0f;
    if (b > 1.0f) b = 1.0f;
    return b;
}

float plato_tile_decay_weight(float weight, uint64_t last_used, float lambda) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t now = (uint64_t)ts.tv_sec;
    uint64_t elapsed = now - last_used;
    if (elapsed > 86400 * 365) elapsed = 86400 * 365;
    float decay = expf(-lambda * (float)elapsed);
    float new_weight = weight * decay;
    if (new_weight < 0.0f) new_weight = 0.0f;
    if (new_weight > 1.0f) new_weight = 1.0f;
    return new_weight;
}

/* ---- Ghost/Afterlife ---- */

bool plato_tile_should_ghost(const plato_tile_t *tile, float threshold) {
    if (threshold <= 0.0f) threshold = 0.05f;
    return tile->weight < threshold;
}

bool plato_tile_resurrect(plato_tile_t *tile, float relevance) {
    if (tile->status != TILE_STATUS_GHOST) return false;
    if (relevance <= 0.0f) return false;
    tile->status = TILE_STATUS_ACTIVE;
    tile->weight = relevance * 0.5f;
    if (tile->weight > 1.0f) tile->weight = 1.0f;
    tile->source = TILE_SOURCE_RESURRECT;
    return true;
}

/* ---- Serialization ---- */

int plato_tile_serialize(const plato_tile_t *tile, char *buf, int buf_size) {
    if (!buf || buf_size < 256) return 0;
    char tag_str[TILE_TAGS_MAX * (TILE_TAG_LEN + 1)];
    tag_str[0] = '\0';
    for (uint8_t i = 0; i < tile->tag_count; i++) {
        if (i > 0) strcat(tag_str, ",");
        strcat(tag_str, tile->tags[i]);
    }
    return snprintf(buf, buf_size, "%lu\t%d\t%d\t%.6f\t%.6f\t%s\t%lu\t%lu\t%u\t%s",
        (unsigned long)tile->id,
        (int)tile->domain,
        (int)tile->status,
        tile->weight,
        tile->belief,
        tile->content,
        (unsigned long)tile->created_at,
        (unsigned long)tile->last_used,
        tile->use_count,
        tag_str);
}

bool plato_tile_deserialize(const char *buf, plato_tile_t *tile) {
    if (!buf || !tile) return false;
    plato_tile_init(tile);
    int domain_i = 0, status_i = 0;
    unsigned long created_l = 0, lastused_l = 0;
    unsigned int use_count_i = 0;
    char tags_raw[TILE_TAGS_MAX * (TILE_TAG_LEN + 1)] = {0};
    int matched = sscanf(buf, "%lu\t%d\t%d\t%f\t%f\t%4095[^\t]\t%lu\t%lu\t%u\t%255[^\n]",
        (unsigned long*)&tile->id, &domain_i, &status_i,
        &tile->weight, &tile->belief, tile->content,
        &created_l, &lastused_l, &use_count_i, tags_raw);
    if (matched < 5) return false;
    tile->domain = (tile_domain_t)domain_i;
    tile->status = (tile_status_t)status_i;
    tile->created_at = created_l;
    tile->last_used = lastused_l;
    tile->use_count = use_count_i;
    tile->content_len = (uint32_t)strlen(tile->content);
    if (tags_raw[0] != '\0') {
        char *tok = strtok(tags_raw, ",");
        while (tok && tile->tag_count < TILE_TAGS_MAX) {
            plato_tile_add_tag(tile, tok);
            tok = strtok(NULL, ",");
        }
    }
    return true;
}

/* ---- Validation ---- */

int plato_tile_validate(const plato_tile_t *tile) {
    int errs = 0;
    if (tile->id == 0) errs |= TILE_ERR_ZERO_ID;
    if (tile->weight < -1.0f || tile->weight > 1.0f) errs |= TILE_ERR_WEIGHT_RANGE;
    if (tile->belief < 0.0f || tile->belief > 1.0f) errs |= TILE_ERR_BELIEF_RANGE;
    if (tile->content[0] == '\0') errs |= TILE_ERR_CONTENT_EMPTY;
    if (tile->content_len >= TILE_CONTENT_MAX) errs |= TILE_ERR_CONTENT_LEN;
    for (uint8_t i = 0; i < tile->tag_count; i++) {
        if (strlen(tile->tags[i]) >= TILE_TAG_LEN) { errs |= TILE_ERR_TAG_TOO_LONG; break; }
    }
    return errs;
}
