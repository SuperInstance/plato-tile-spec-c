#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tile.h"

#define TEST(name) printf("  %s... ", name); 
#define PASS() printf("PASS\n")

static void test_init_validate(void) {
    TEST("init and validate");
    PlatoTile t;
    TileError e = tile_init(&t, "tile-1", "harbor", "Hello world");
    assert(e == TILE_OK);
    assert(strcmp(t.id, "tile-1") == 0);
    assert(t.state == TILE_DRAFT);
    assert(t.version == 1);
    assert(t.tag_count == 0);
    assert(t.access_count == 0);
    e = tile_validate(&t);
    assert(e == TILE_OK);
    PASS();
}

static void test_null_checks(void) {
    TEST("null safety");
    assert(tile_init(NULL, "a", "b", "c") == TILE_ERR_NULL);
    assert(tile_validate(NULL) == TILE_ERR_NULL);
    PlatoTile t;
    assert(tile_init(&t, "", "room", "content") == TILE_ERR_ID_EMPTY);
    assert(tile_init(&t, "id", "", "content") == TILE_ERR_ROOM_EMPTY);
    PASS();
}

static void test_tags(void) {
    TEST("tags add/remove/has");
    PlatoTile t;
    tile_init(&t, "t1", "room", "content");
    assert(tile_add_tag(&t, "python") == TILE_OK);
    assert(tile_add_tag(&t, "rust") == TILE_OK);
    assert(t.tag_count == 2);
    assert(tile_has_tag(&t, "python"));
    assert(!tile_has_tag(&t, "cpp"));
    assert(tile_add_tag(&t, "python") == TILE_OK); /* duplicate no-op */
    assert(t.tag_count == 2);
    tile_remove_tag(&t, "python");
    assert(t.tag_count == 1);
    assert(!tile_has_tag(&t, "python"));
    PASS();
}

static void test_state_lifecycle(void) {
    TEST("state lifecycle");
    PlatoTile t;
    tile_init(&t, "t2", "room", "content");
    assert(tile_set_state(&t, TILE_ACTIVE) == TILE_OK);
    assert(t.state == TILE_ACTIVE);
    assert(tile_set_state(&t, TILE_PINNED) == TILE_OK);
    assert(tile_set_state(&t, TILE_DEPRECATED) == TILE_OK);
    assert(tile_set_state(&t, TILE_ARCHIVED) == TILE_OK);
    assert(tile_set_state(&t, TILE_DELETED) == TILE_OK);
    PASS();
}

static void test_content_version(void) {
    TEST("content update bumps version");
    PlatoTile t;
    tile_init(&t, "t3", "room", "v1");
    assert(t.version == 1);
    tile_set_content(&t, "v2");
    assert(t.version == 2);
    tile_set_content(&t, "v3");
    assert(t.version == 3);
    PASS();
}

static void test_touch(void) {
    TEST("touch updates access");
    PlatoTile t;
    tile_init(&t, "t4", "room", "content");
    assert(t.access_count == 0);
    tile_touch(&t);
    assert(t.access_count == 1);
    tile_touch(&t);
    assert(t.access_count == 2);
    PASS();
}

static void test_hash(void) {
    TEST("hash computation");
    PlatoTile t1, t2;
    tile_init(&t1, "t5", "room", "same content");
    tile_init(&t2, "t6", "room", "same content");
    assert(strcmp(t1.hash, t2.hash) == 0); /* same content = same hash */
    tile_set_content(&t2, "different");
    assert(strcmp(t1.hash, t2.hash) != 0);
    PASS();
}

static void test_collection_stats(void) {
    TEST("collection stats");
    PlatoTile tiles[3];
    tile_init(&tiles[0], "a", "room", "c1");
    tile_init(&tiles[1], "b", "room", "c2");
    tile_init(&tiles[2], "c", "room", "c3");
    tiles[0].confidence = 1.0f;
    tiles[1].confidence = 0.5f;
    tiles[2].confidence = 0.0f;
    tile_set_state(&tiles[0], TILE_ACTIVE);
    tile_set_state(&tiles[1], TILE_ACTIVE);
    tile_set_state(&tiles[2], TILE_DRAFT);
    TileStats stats;
    tile_collection_stats(tiles, 3, &stats);
    assert(stats.total == 3);
    assert(stats.by_state[TILE_ACTIVE] == 2);
    assert(stats.by_state[TILE_DRAFT] == 1);
    assert(stats.avg_confidence == 0.5f);
    PASS();
}

static void test_collection_filter(void) {
    TEST("collection filter by state/type/room/tag");
    PlatoTile tiles[3];
    tile_init(&tiles[0], "a", "harbor", "c1");
    tile_init(&tiles[1], "b", "tavern", "c2");
    tile_init(&tiles[2], "c", "harbor", "c3");
    tile_set_state(&tiles[0], TILE_ACTIVE);
    tile_set_state(&tiles[1], TILE_ACTIVE);
    tile_set_state(&tiles[2], TILE_DELETED);
    tile_add_tag(&tiles[0], "important");

    PlatoTile out[3];
    uint32_t n = tile_collection_by_state(tiles, 3, TILE_ACTIVE, out, 3);
    assert(n == 2);
    n = tile_collection_by_room(tiles, 3, "harbor", out, 3);
    assert(n == 2);
    n = tile_collection_by_tag(tiles, 3, "important", out, 3);
    assert(n == 1);
    PASS();
}

static void test_collection_search(void) {
    TEST("collection search");
    PlatoTile tiles[2];
    tile_init(&tiles[0], "a", "room", "hello world");
    tile_init(&tiles[1], "b", "room", "goodbye world");
    PlatoTile out[2];
    uint32_t n = tile_collection_search(tiles, 2, "hello", out, 2);
    assert(n == 1);
    n = tile_collection_search(tiles, 2, "world", out, 2);
    assert(n == 2);
    PASS();
}

static void test_collection_sort(void) {
    TEST("collection sort by importance");
    PlatoTile tiles[3];
    tile_init(&tiles[0], "a", "room", "low");
    tile_init(&tiles[1], "b", "room", "high");
    tile_init(&tiles[2], "c", "room", "mid");
    tiles[0].importance = 0.2f;
    tiles[1].importance = 0.9f;
    tiles[2].importance = 0.5f;
    tile_collection_sort_by_importance(tiles, 3, true);
    assert(tiles[0].importance >= tiles[1].importance);
    assert(tiles[1].importance >= tiles[2].importance);
    PASS();
}

static void test_type_operations(void) {
    TEST("type set and filter");
    PlatoTile tiles[3];
    tile_init(&tiles[0], "a", "room", "fact");
    tile_init(&tiles[1], "b", "room", "rule");
    tile_init(&tiles[2], "c", "room", "another fact");
    tile_set_type(&tiles[0], TILE_FACT);
    tile_set_type(&tiles[1], TILE_RULE);
    tile_set_type(&tiles[2], TILE_FACT);
    PlatoTile out[3];
    uint32_t n = tile_collection_by_type(tiles, 3, TILE_FACT, out, 3);
    assert(n == 2);
    n = tile_collection_by_type(tiles, 3, TILE_RULE, out, 3);
    assert(n == 1);
    PASS();
}

int main(void) {
    printf("=== plato-tile-spec-c tests ===\n");
    test_init_validate();
    test_null_checks();
    test_tags();
    test_state_lifecycle();
    test_content_version();
    test_touch();
    test_hash();
    test_collection_stats();
    test_collection_filter();
    test_collection_search();
    test_collection_sort();
    test_type_operations();
    printf("\nAll tests passed!\n");
    return 0;
}
