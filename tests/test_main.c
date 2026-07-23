/*
 * Minimal unit tests for core Zelda3 functions.
 *
 * Covers three areas:
 *   1. INI parsing  (ParseBool, NextDelim, SplitKeyValue, NextLineStripComments,
 *                    StringEqualsNoCase, StringStartsWithNoCase, SkipPrefix)
 *   2. Save state   (ByteArray_AppendByte/AppendData/Resize/Destroy,
 *                    FindIndexInMemblk)
 *   3. Submodule dispatch pattern (function-pointer table indexing,
 *                    bounds checking — mirrors kMainRouting in misc.c)
 *
 * The test binary links only util.c plus this file; no SDL or game state
 * is required.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "util.h"

/* ---- minimal test framework --------------------------------------- */

static int g_tests_run   = 0;
static int g_tests_pass  = 0;
static int g_tests_fail  = 0;

#define TEST(name)  static void name(void)

#define RUN(name) do {            \
    g_tests_run++;                \
    fprintf(stderr, "  [RUN ] %s ... ", #name); \
    name();                       \
    g_tests_pass++;               \
    fprintf(stderr, "ok\n");      \
  } while (0)

#define ASSERT(cond, ...) do {                          \
    if (!(cond)) {                                      \
      fprintf(stderr, "FAIL\n  %s:%d: ", __FILE__, __LINE__); \
      fprintf(stderr, __VA_ARGS__);                     \
      fprintf(stderr, "\n");                            \
      g_tests_fail++;                                   \
      exit(1);                                          \
    }                                                   \
  } while (0)

#define ASSERT_EQ(a, b) do {                            \
    long long _a = (long long)(a);                      \
    long long _b = (long long)(b);                      \
    if (_a != _b) {                                     \
      fprintf(stderr, "FAIL\n  %s:%d: expected %lld, got %lld\n", \
              __FILE__, __LINE__, _b, _a);              \
      g_tests_fail++;                                   \
      exit(1);                                          \
    }                                                   \
  } while (0)

#define ASSERT_STR_EQ(a, b) do {                        \
    if (strcmp((a), (b)) != 0) {                        \
      fprintf(stderr, "FAIL\n  %s:%d: expected \"%s\", got \"%s\"\n", \
              __FILE__, __LINE__, (b), (a));            \
      g_tests_fail++;                                   \
      exit(1);                                          \
    }                                                   \
  } while (0)

/* Stub for Die() — util.c references it on allocation failure. */
void NORETURN Die(const char *error) {
    fprintf(stderr, "Fatal: %s\n", error);
    exit(1);
}

/* ---- 1. INI parsing tests ----------------------------------------- */

TEST(test_ParseBool_true_values) {
    bool result = false;

    ASSERT(ParseBool("true", &result),  "ParseBool(\"true\") should succeed");
    ASSERT(result == true, "ParseBool(\"true\") should set true");

    ASSERT(ParseBool("TRUE", &result),  "ParseBool(\"TRUE\") should succeed");
    ASSERT(result == true, "ParseBool(\"TRUE\") should set true");

    ASSERT(ParseBool("1", &result),     "ParseBool(\"1\") should succeed");
    ASSERT(result == true, "ParseBool(\"1\") should set true");

    ASSERT(ParseBool("yes", &result),   "ParseBool(\"yes\") should succeed");
    ASSERT(result == true, "ParseBool(\"yes\") should set true");

    ASSERT(ParseBool("on", &result),    "ParseBool(\"on\") should succeed");
    ASSERT(result == true, "ParseBool(\"on\") should set true");
}

TEST(test_ParseBool_false_values) {
    bool result = true;

    ASSERT(ParseBool("false", &result), "ParseBool(\"false\") should succeed");
    ASSERT(result == false, "ParseBool(\"false\") should set false");

    ASSERT(ParseBool("FALSE", &result), "ParseBool(\"FALSE\") should succeed");
    ASSERT(result == false, "ParseBool(\"FALSE\") should set false");

    ASSERT(ParseBool("0", &result),     "ParseBool(\"0\") should succeed");
    ASSERT(result == false, "ParseBool(\"0\") should set false");

    ASSERT(ParseBool("no", &result),    "ParseBool(\"no\") should succeed");
    ASSERT(result == false, "ParseBool(\"no\") should set false");

    ASSERT(ParseBool("off", &result),   "ParseBool(\"off\") should succeed");
    ASSERT(result == false, "ParseBool(\"off\") should set false");
}

TEST(test_ParseBool_invalid) {
    bool result;
    ASSERT(!ParseBool("xyz", &result),  "ParseBool(\"xyz\") should fail");
    ASSERT(!ParseBool("", &result),     "ParseBool(\"\") should fail");
    ASSERT(!ParseBool("2", &result),    "ParseBool(\"2\") should fail");
    ASSERT(!ParseBool("10", &result),   "ParseBool(\"10\") should fail");
}

TEST(test_StringEqualsNoCase) {
    ASSERT(StringEqualsNoCase("hello", "hello"),  "case equal");
    ASSERT(StringEqualsNoCase("Hello", "hello"),  "diff case");
    ASSERT(StringEqualsNoCase("HELLO", "hello"),  "upper vs lower");
    ASSERT(!StringEqualsNoCase("hello", "world"), "different strings");
    ASSERT(!StringEqualsNoCase("hello", "hell"),  "prefix not equal");
    ASSERT(StringEqualsNoCase("", ""),            "empty strings");
    ASSERT(!StringEqualsNoCase("a", ""),           "one empty");
}

TEST(test_StringStartsWithNoCase) {
    const char *r;
    r = StringStartsWithNoCase("HelloWorld", "hello");
    ASSERT(r != NULL,  "should match prefix");
    ASSERT_STR_EQ(r, "World");

    r = StringStartsWithNoCase("HelloWorld", "HEL");
    ASSERT(r != NULL,  "should match prefix case-insensitive");
    ASSERT_STR_EQ(r, "loWorld");

    r = StringStartsWithNoCase("abc", "abcd");
    ASSERT(r == NULL, "should not match when string is shorter than prefix");

    r = StringStartsWithNoCase("abc", "");
    ASSERT(r != NULL,  "empty prefix always matches");
    ASSERT_STR_EQ(r, "abc");
}

TEST(test_NextDelim) {
    char input[] = "a,b,,c";
    char *s = input;
    char *t;

    t = NextDelim(&s, ',');
    ASSERT_STR_EQ(t, "a");

    t = NextDelim(&s, ',');
    ASSERT_STR_EQ(t, "b");

    t = NextDelim(&s, ',');
    ASSERT_STR_EQ(t, "");

    t = NextDelim(&s, ',');
    ASSERT_STR_EQ(t, "c");

    t = NextDelim(&s, ',');
    ASSERT(t == NULL, "should return NULL when no more delimiters");
}

TEST(test_SplitKeyValue) {
    char input1[] = "key=value";
    char *v = SplitKeyValue(input1);
    ASSERT_STR_EQ(input1, "key");
    ASSERT_STR_EQ(v, "value");

    char input2[] = "key  =  value";
    v = SplitKeyValue(input2);
    ASSERT_STR_EQ(input2, "key");
    ASSERT_STR_EQ(v, "value");

    char input3[] = "no_equals_here";
    v = SplitKeyValue(input3);
    ASSERT(v == NULL, "should return NULL when no '=' found");
}

TEST(test_NextLineStripComments) {
    char data[] = "line1 # comment\nline2\n#full comment\n  \n";
    char *p = data;
    char *line;

    line = NextLineStripComments(&p);
    ASSERT_STR_EQ(line, "line1");

    line = NextLineStripComments(&p);
    ASSERT_STR_EQ(line, "line2");

    line = NextLineStripComments(&p);
    ASSERT_STR_EQ(line, "");

    line = NextLineStripComments(&p);
    ASSERT_STR_EQ(line, "");

    /* After the final newline, one more call returns empty string,
     * then the next call returns NULL (exhausted). */
    line = NextLineStripComments(&p);
    ASSERT_STR_EQ(line, "");

    line = NextLineStripComments(&p);
    ASSERT(line == NULL, "should return NULL at end");
}

TEST(test_SkipPrefix) {
    const char *r;
    r = SkipPrefix("HelloWorld", "Hello");
    ASSERT(r != NULL,  "should match prefix");
    ASSERT_STR_EQ(r, "World");

    r = SkipPrefix("HelloWorld", "Bye");
    ASSERT(r == NULL, "should not match different prefix");

    r = SkipPrefix("abc", "");
    ASSERT(r != NULL,  "empty prefix matches");
    ASSERT_STR_EQ(r, "abc");
}

/* ---- 2. Save state serialization tests ---------------------------- */

TEST(test_ByteArray_basic) {
    ByteArray arr = {0};

    ByteArray_AppendByte(&arr, 0x42);
    ASSERT_EQ(arr.size, 1);
    ASSERT_EQ(arr.data[0], 0x42);

    ByteArray_AppendByte(&arr, 0xFF);
    ASSERT_EQ(arr.size, 2);
    ASSERT_EQ(arr.data[1], 0xFF);

    ByteArray_Destroy(&arr);
    ASSERT(arr.data == NULL, "data should be NULL after destroy");
}

TEST(test_ByteArray_append_data) {
    ByteArray arr = {0};
    uint8 buf[] = {1, 2, 3, 4, 5};

    ByteArray_AppendData(&arr, buf, 5);
    ASSERT_EQ(arr.size, 5);
    ASSERT(memcmp(arr.data, buf, 5) == 0, "data should match");

    /* Append more data to test growth */
    ByteArray_AppendData(&arr, buf, 5);
    ASSERT_EQ(arr.size, 10);
    ASSERT_EQ(arr.data[0], 1);
    ASSERT_EQ(arr.data[4], 5);
    ASSERT_EQ(arr.data[5], 1);
    ASSERT_EQ(arr.data[9], 5);

    ByteArray_Destroy(&arr);
}

TEST(test_ByteArray_resize) {
    ByteArray arr = {0};

    ByteArray_Resize(&arr, 100);
    ASSERT_EQ(arr.size, 100);
    ASSERT(arr.capacity >= 100, "capacity should be >= size");

    /* Resize smaller — capacity should not shrink */
    ByteArray_Resize(&arr, 10);
    ASSERT_EQ(arr.size, 10);
    ASSERT(arr.capacity >= 100, "capacity should not shrink");

    /* Resize larger again */
    ByteArray_Resize(&arr, 200);
    ASSERT_EQ(arr.size, 200);
    ASSERT(arr.capacity >= 200, "capacity should grow");

    ByteArray_Destroy(&arr);
}

TEST(test_FindIndexInMemblk_16bit) {
    /* Build a 16-bit indexed memblk. The format is:
     *   [offset_0 .. offset_{mx-1}]  (uint16 LE each)
     *   [data for all indices]
     *   [mx as uint16 LE]            (last 2 bytes)
     *
     * For mx=1 (indices 0 and 1), data is:
     *   offset_0=2, "AB", "CD", mx=1
     */
    uint8 data[] = {
        2, 0,             /* offset_0 = 2 (uint16 LE) */
        'A', 'B',         /* index 0 data (2 bytes) */
        'C', 'D',         /* index 1 data (2 bytes) */
        1, 0,             /* max index = 1 (uint16 LE, last 2 bytes) */
    };
    MemBlk blk = { data, sizeof(data) };

    MemBlk r0 = FindIndexInMemblk(blk, 0);
    ASSERT(r0.size == 2, "index 0 should have 2 bytes");
    ASSERT(memcmp(r0.ptr, "AB", 2) == 0, "index 0 data should be 'AB'");

    MemBlk r1 = FindIndexInMemblk(blk, 1);
    ASSERT(r1.size == 2, "index 1 should have 2 bytes");
    ASSERT(memcmp(r1.ptr, "CD", 2) == 0, "index 1 data should be 'CD'");

    /* Out of range */
    MemBlk r2 = FindIndexInMemblk(blk, 2);
    ASSERT(r2.ptr == NULL && r2.size == 0, "index 2 should be empty");
}

TEST(test_FindIndexInMemblk_empty) {
    MemBlk empty = { NULL, 0 };
    MemBlk r = FindIndexInMemblk(empty, 0);
    ASSERT(r.ptr == NULL && r.size == 0, "empty memblk should return empty");

    uint8 tiny[] = { 0 };
    MemBlk tiny_blk = { tiny, 1 };
    r = FindIndexInMemblk(tiny_blk, 0);
    ASSERT(r.ptr == NULL && r.size == 0, "memblk < 2 bytes should return empty");
}

/* Simulates the saveFunc/loadFunc round-trip pattern used in zelda_rtl.c */
TEST(test_save_load_roundtrip) {
    ByteArray save_buf = {0};

    /* Simulate saving: append some data */
    uint8 sim_ram[] = {0x10, 0x11, 0x12, 0x13, 0x14};
    uint8 sim_sram[] = {0x20, 0x21, 0x22};

    ByteArray_AppendData(&save_buf, sim_ram, sizeof(sim_ram));
    ByteArray_AppendData(&save_buf, sim_sram, sizeof(sim_sram));

    /* Simulate loading: read back in same order */
    uint8 load_ram[5], load_sram[3];
    size_t offset = 0;

    memcpy(load_ram, save_buf.data + offset, 5);
    offset += 5;
    memcpy(load_sram, save_buf.data + offset, 3);

    ASSERT(memcmp(sim_ram, load_ram, 5) == 0, "ram data should round-trip");
    ASSERT(memcmp(sim_sram, load_sram, 3) == 0, "sram data should round-trip");

    ByteArray_Destroy(&save_buf);
}

/* ---- 3. Submodule dispatch tests ---------------------------------- */

/* The dispatch pattern: an array of function pointers indexed by
 * a module/submodule index, exactly like kMainRouting in misc.c.
 * We test the pattern itself: correct indexing, bounds safety,
 * and that all entries are non-NULL. */

typedef void (*DispatchHandler)(void);

static int g_dispatch_call_count = 0;
static int g_last_dispatched    = -1;

static void handler_0(void) { g_dispatch_call_count++; g_last_dispatched = 0; }
static void handler_1(void) { g_dispatch_call_count++; g_last_dispatched = 1; }
static void handler_2(void) { g_dispatch_call_count++; g_last_dispatched = 2; }
static void handler_3(void) { g_dispatch_call_count++; g_last_dispatched = 3; }

#define DISPATCH_TABLE_SIZE 4
static const DispatchHandler kDispatchTable[DISPATCH_TABLE_SIZE] = {
    &handler_0, &handler_1, &handler_2, &handler_3,
};

/* Mirrors Module_MainRouting() in misc.c */
static void DispatchByIndex(int index) {
    if (index < 0 || index >= DISPATCH_TABLE_SIZE) return;
    kDispatchTable[index]();
}

TEST(test_dispatch_all_entries_nonnull) {
    for (int i = 0; i < DISPATCH_TABLE_SIZE; i++) {
        ASSERT(kDispatchTable[i] != NULL,
               "dispatch entry %d must not be NULL", i);
    }
}

TEST(test_dispatch_correct_indexing) {
    g_dispatch_call_count = 0;
    for (int i = 0; i < DISPATCH_TABLE_SIZE; i++) {
        DispatchByIndex(i);
        ASSERT_EQ(g_last_dispatched, i);
    }
    ASSERT_EQ(g_dispatch_call_count, DISPATCH_TABLE_SIZE);
}

TEST(test_dispatch_bounds_check) {
    /* Out-of-range indices must not crash (no dispatch) */
    int before = g_dispatch_call_count;
    DispatchByIndex(-1);
    DispatchByIndex(DISPATCH_TABLE_SIZE);
    DispatchByIndex(999);
    ASSERT_EQ(g_dispatch_call_count, before);  /* count should not change */
}

TEST(test_dispatch_table_compile_time_size) {
    /* This mirrors the _Static_assert(countof(kMainRouting) == 28)
     * that is enforced at compile time in misc.c.
     * Here we verify the same pattern for our test table. */
    _Static_assert(DISPATCH_TABLE_SIZE == 4,
                   "dispatch table must have exactly 4 entries");
    ASSERT_EQ((int)(sizeof(kDispatchTable) / sizeof(kDispatchTable[0])),
              DISPATCH_TABLE_SIZE);
}

/* ---- main --------------------------------------------------------- */

int main(void) {
    fprintf(stderr, "\n=== INI Parsing Tests ===\n");
    RUN(test_ParseBool_true_values);
    RUN(test_ParseBool_false_values);
    RUN(test_ParseBool_invalid);
    RUN(test_StringEqualsNoCase);
    RUN(test_StringStartsWithNoCase);
    RUN(test_NextDelim);
    RUN(test_SplitKeyValue);
    RUN(test_NextLineStripComments);
    RUN(test_SkipPrefix);

    fprintf(stderr, "\n=== Save State Serialization Tests ===\n");
    RUN(test_ByteArray_basic);
    RUN(test_ByteArray_append_data);
    RUN(test_ByteArray_resize);
    RUN(test_FindIndexInMemblk_16bit);
    RUN(test_FindIndexInMemblk_empty);
    RUN(test_save_load_roundtrip);

    fprintf(stderr, "\n=== Submodule Dispatch Tests ===\n");
    RUN(test_dispatch_all_entries_nonnull);
    RUN(test_dispatch_correct_indexing);
    RUN(test_dispatch_bounds_check);
    RUN(test_dispatch_table_compile_time_size);

    fprintf(stderr, "\n=== Summary ===\n");
    fprintf(stderr, "  Total: %d\n", g_tests_run);
    fprintf(stderr, "  Passed: %d\n", g_tests_pass);
    fprintf(stderr, "  Failed: %d\n", g_tests_fail);

    return g_tests_fail > 0 ? 1 : 0;
}
