#include "tile.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) void test_##name(void)
#define RUN(name) do { printf("  test_%s... ", #name); test_##name(); printf("OK\n"); tests_passed++; } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { printf("FAIL: %s != %s (%d vs %d)\n", #a, #b, (int)(a), (int)(b)); tests_failed++; return; } } while(0)
#define ASSERT_TRUE(x) do { if (!(x)) { printf("FAIL: %s is false\n", #x); tests_failed++; return; } } while(0)
#define ASSERT_FALSE(x) do { if ((x)) { printf("FAIL: %s is true\n", #x); tests_failed++; return; } } while(0)
#define ASSERT_NEAR(a, b, eps) do { float _a=(a), _b=(b); if (fabsf(_a-_b) > (eps)) { printf("FAIL: %s != %s (%.6f vs %.6f)\n", #a, #b, _a, _b); tests_failed++; return; } } while(0)

/* ---- Lifecycle Tests ---- */

TEST(init_defaults) {
    plato_tile_t t;
    plato_tile_init(&t);
    ASSERT_EQ(t.id, 0);
    ASSERT_EQ(t.domain, TILE_DOMAIN_UNKNOWN);
    ASSERT_EQ(t.status, TILE_STATUS_ACTIVE);
    ASSERT_NEAR(t.weight, 1.0f, 0.001f);
    ASSERT_NEAR(t.belief, 0.0f, 0.001f);
    ASSERT_EQ(t.tag_count, 0);
}

TEST(generate_id_unique) {
    tile_id_t id1 = plato_tile_generate_id();
    tile_id_t id2 = plato_tile_generate_id();
    ASSERT_TRUE(id1 != id2);
    ASSERT_TRUE(id1 != 0);
    ASSERT_TRUE(id2 != 0);
}

TEST(generate_id_many_unique) {
    tile_id_t ids[100];
    int all_unique = 1;
    for (int i = 0; i < 100; i++) {
        ids[i] = plato_tile_generate_id();
    }
    for (int i = 0; i < 100 && all_unique; i++) {
        for (int j = i+1; j < 100 && all_unique; j++) {
            if (ids[i] == ids[j]) all_unique = 0;
        }
    }
    ASSERT_TRUE(all_unique);
}

TEST(hash_content_deterministic) {
    tile_id_t h1 = plato_tile_hash_content("hello world", 11);
    tile_id_t h2 = plato_tile_hash_content("hello world", 11);
    ASSERT_EQ(h1, h2);
}

TEST(hash_content_different) {
    tile_id_t h1 = plato_tile_hash_content("hello", 5);
    tile_id_t h2 = plato_tile_hash_content("world", 5);
    ASSERT_TRUE(h1 != h2);
}

/* ---- Tag Tests ---- */

TEST(add_tag_basic) {
    plato_tile_t t;
    plato_tile_init(&t);
    ASSERT_TRUE(plato_tile_add_tag(&t, "constraint"));
    ASSERT_EQ(t.tag_count, 1);
}

TEST(add_tag_duplicate) {
    plato_tile_t t;
    plato_tile_init(&t);
    plato_tile_add_tag(&t, "constraint");
    ASSERT_FALSE(plato_tile_add_tag(&t, "constraint"));
    ASSERT_EQ(t.tag_count, 1);
}

TEST(add_tag_max) {
    plato_tile_t t;
    plato_tile_init(&t);
    int added = 0;
    char buf[32];
    for (int i = 0; i < 20; i++) {
        snprintf(buf, sizeof(buf), "tag%d", i);
        if (plato_tile_add_tag(&t, buf)) added++;
    }
    ASSERT_EQ(added, TILE_TAGS_MAX);
    ASSERT_EQ(t.tag_count, TILE_TAGS_MAX);
}

TEST(has_tag) {
    plato_tile_t t;
    plato_tile_init(&t);
    plato_tile_add_tag(&t, "flux");
    plato_tile_add_tag(&t, "trust");
    ASSERT_TRUE(plato_tile_has_tag(&t, "flux"));
    ASSERT_TRUE(plato_tile_has_tag(&t, "trust"));
    ASSERT_FALSE(plato_tile_has_tag(&t, "missing"));
}

/* ---- Scoring Tests ---- */

TEST(compute_belief_basic) {
    float b = plato_tile_compute_belief(0.8f, 0.6f, 0.4f);
    /* 0.5*0.8 + 0.3*0.6 + 0.2*0.4 = 0.4 + 0.18 + 0.08 = 0.66 */
    ASSERT_NEAR(b, 0.66f, 0.01f);
}

TEST(compute_belief_clamped_high) {
    float b = plato_tile_compute_belief(2.0f, 2.0f, 2.0f);
    ASSERT_TRUE(b <= 1.0f);
}

TEST(compute_belief_clamped_low) {
    float b = plato_tile_compute_belief(-1.0f, -1.0f, -1.0f);
    ASSERT_TRUE(b >= 0.0f);
}

TEST(compute_belief_zero) {
    float b = plato_tile_compute_belief(0.0f, 0.0f, 0.0f);
    ASSERT_NEAR(b, 0.0f, 0.001f);
}

/* ---- Ghost/Afterlife Tests ---- */

TEST(should_ghost_low_weight) {
    plato_tile_t t;
    plato_tile_init(&t);
    t.weight = 0.03f;
    ASSERT_TRUE(plato_tile_should_ghost(&t, 0.05f));
}

TEST(should_not_ghost_high_weight) {
    plato_tile_t t;
    plato_tile_init(&t);
    t.weight = 0.5f;
    ASSERT_FALSE(plato_tile_should_ghost(&t, 0.05f));
}

TEST(should_not_ghost_exact_threshold) {
    plato_tile_t t;
    plato_tile_init(&t);
    t.weight = 0.05f;
    ASSERT_FALSE(plato_tile_should_ghost(&t, 0.05f));
}

TEST(resurrect_ghost) {
    plato_tile_t t;
    plato_tile_init(&t);
    t.status = TILE_STATUS_GHOST;
    t.weight = 0.01f;
    ASSERT_TRUE(plato_tile_resurrect(&t, 0.8f));
    ASSERT_EQ(t.status, TILE_STATUS_ACTIVE);
    ASSERT_NEAR(t.weight, 0.4f, 0.01f); /* 0.8 * 0.5 */
    ASSERT_EQ(t.source, TILE_SOURCE_RESURRECT);
}

TEST(resurrect_non_ghost_fails) {
    plato_tile_t t;
    plato_tile_init(&t);
    t.status = TILE_STATUS_ACTIVE;
    ASSERT_FALSE(plato_tile_resurrect(&t, 0.8f));
}

TEST(resurrect_zero_relevance_fails) {
    plato_tile_t t;
    plato_tile_init(&t);
    t.status = TILE_STATUS_GHOST;
    ASSERT_FALSE(plato_tile_resurrect(&t, 0.0f));
}

TEST(resurrect_high_relevance_clamped) {
    plato_tile_t t;
    plato_tile_init(&t);
    t.status = TILE_STATUS_GHOST;
    ASSERT_TRUE(plato_tile_resurrect(&t, 3.0f));
    ASSERT_TRUE(t.weight <= 1.0f);
}

/* ---- Serialization Tests ---- */

TEST(serialize_roundtrip) {
    plato_tile_t t1, t2;
    plato_tile_init(&t1);
    t1.id = 42;
    t1.domain = TILE_DOMAIN_KNOWLEDGE;
    t1.status = TILE_STATUS_ACTIVE;
    t1.weight = 0.75f;
    t1.belief = 0.6f;
    strcpy(t1.content, "constraint theory proof");
    t1.content_len = (uint32_t)strlen(t1.content);
    t1.created_at = 1713400000;
    t1.use_count = 5;
    plato_tile_add_tag(&t1, "math");
    plato_tile_add_tag(&t1, "proof");

    char buf[8192];
    int len = plato_tile_serialize(&t1, buf, sizeof(buf));
    ASSERT_TRUE(len > 0);

    ASSERT_TRUE(plato_tile_deserialize(buf, &t2));
    ASSERT_EQ(t2.id, 42);
    ASSERT_EQ(t2.domain, TILE_DOMAIN_KNOWLEDGE);
    ASSERT_EQ(t2.status, TILE_STATUS_ACTIVE);
    ASSERT_NEAR(t2.weight, 0.75f, 0.001f);
    ASSERT_NEAR(t2.belief, 0.6f, 0.001f);
    ASSERT_EQ(strcmp(t2.content, "constraint theory proof"), 0);
    ASSERT_EQ(t2.use_count, 5);
    ASSERT_EQ(t2.tag_count, 2);
    ASSERT_TRUE(plato_tile_has_tag(&t2, "math"));
    ASSERT_TRUE(plato_tile_has_tag(&t2, "proof"));
}

TEST(serialize_empty_content) {
    plato_tile_t t1, t2;
    plato_tile_init(&t1);
    t1.id = 1;
    char buf[8192];
    plato_tile_serialize(&t1, buf, sizeof(buf));
    ASSERT_TRUE(plato_tile_deserialize(buf, &t2));
    ASSERT_EQ(t2.id, 1);
}

TEST(deserialize_invalid) {
    plato_tile_t t;
    ASSERT_FALSE(plato_tile_deserialize(NULL, &t));
    ASSERT_FALSE(plato_tile_deserialize("", &t));
    ASSERT_FALSE(plato_tile_deserialize("garbage", &t));
}

TEST(serialize_preserves_pipes) {
    plato_tile_t t1, t2;
    plato_tile_init(&t1);
    t1.id = 99;
    strcpy(t1.content, "x | y | z"); /* pipes should survive */
    t1.content_len = (uint32_t)strlen(t1.content);
    char buf[8192];
    plato_tile_serialize(&t1, buf, sizeof(buf));
    ASSERT_TRUE(plato_tile_deserialize(buf, &t2));
    ASSERT_EQ(strcmp(t2.content, "x | y | z"), 0);
}

/* ---- Validation Tests ---- */

TEST(validate_valid) {
    plato_tile_t t;
    plato_tile_init(&t);
    t.id = 1;
    strcpy(t.content, "test");
    t.content_len = 4;
    ASSERT_EQ(plato_tile_validate(&t), 0);
}

TEST(validate_zero_id) {
    plato_tile_t t;
    plato_tile_init(&t);
    strcpy(t.content, "test");
    ASSERT_TRUE(plato_tile_validate(&t) & TILE_ERR_ZERO_ID);
}

TEST(validate_empty_content) {
    plato_tile_t t;
    plato_tile_init(&t);
    t.id = 1;
    ASSERT_TRUE(plato_tile_validate(&t) & TILE_ERR_CONTENT_EMPTY);
}

TEST(validate_weight_range) {
    plato_tile_t t;
    plato_tile_init(&t);
    t.id = 1;
    strcpy(t.content, "test");
    t.weight = 2.0f;
    ASSERT_TRUE(plato_tile_validate(&t) & TILE_ERR_WEIGHT_RANGE);
}

TEST(validate_belief_range) {
    plato_tile_t t;
    plato_tile_init(&t);
    t.id = 1;
    strcpy(t.content, "test");
    t.belief = -0.5f;
    ASSERT_TRUE(plato_tile_validate(&t) & TILE_ERR_BELIEF_RANGE);
}

/* ---- Struct Size Tests ---- */

TEST(struct_size_reasonable) {
    /* The struct should be stack-allocatable but not wasteful */
    size_t sz = sizeof(plato_tile_t);
    printf("(size=%zu) ", sz);
    ASSERT_TRUE(sz < 10000);  /* should be under 10KB */
    ASSERT_TRUE(sz > 4000);   /* should have content buffer */
}

int main(void) {
    printf("=== plato-tile-spec-c Tests ===\n");

    printf("Lifecycle:\n");
    RUN(init_defaults);
    RUN(generate_id_unique);
    RUN(generate_id_many_unique);
    RUN(hash_content_deterministic);
    RUN(hash_content_different);

    printf("Tags:\n");
    RUN(add_tag_basic);
    RUN(add_tag_duplicate);
    RUN(add_tag_max);
    RUN(has_tag);

    printf("Scoring:\n");
    RUN(compute_belief_basic);
    RUN(compute_belief_clamped_high);
    RUN(compute_belief_clamped_low);
    RUN(compute_belief_zero);

    printf("Ghost/Afterlife:\n");
    RUN(should_ghost_low_weight);
    RUN(should_not_ghost_high_weight);
    RUN(should_not_ghost_exact_threshold);
    RUN(resurrect_ghost);
    RUN(resurrect_non_ghost_fails);
    RUN(resurrect_zero_relevance_fails);
    RUN(resurrect_high_relevance_clamped);

    printf("Serialization:\n");
    RUN(serialize_roundtrip);
    RUN(serialize_empty_content);
    RUN(deserialize_invalid);
    RUN(serialize_preserves_pipes);

    printf("Validation:\n");
    RUN(validate_valid);
    RUN(validate_zero_id);
    RUN(validate_empty_content);
    RUN(validate_weight_range);
    RUN(validate_belief_range);

    printf("Struct:\n");
    RUN(struct_size_reasonable);

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
