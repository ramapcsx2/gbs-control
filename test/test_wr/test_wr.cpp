#include <unity.h>
#include "wr_queue.h"
#include "wr_parse.h"

// ---------------------------------------------------------------------------
// Ring buffer tests
// ---------------------------------------------------------------------------

void test_queue_empty_on_init(void)
{
    uint8_t head = 0, tail = 0;
    TEST_ASSERT_TRUE(wrQueueEmpty(head, tail));
    TEST_ASSERT_FALSE(wrQueueFull(head, tail));
}

void test_queue_push_one(void)
{
    uint8_t head = 0, tail = 0;
    head = (head + 1) % WR_QUEUE_SIZE;
    TEST_ASSERT_FALSE(wrQueueEmpty(head, tail));
    TEST_ASSERT_FALSE(wrQueueFull(head, tail));
}

void test_queue_full_at_capacity(void)
{
    uint8_t head = 0, tail = 0;
    // fill WR_QUEUE_SIZE-1 slots (one slot always unused in circular buffer)
    for (int i = 0; i < WR_QUEUE_SIZE - 1; i++) {
        TEST_ASSERT_FALSE(wrQueueFull(head, tail));
        head = (head + 1) % WR_QUEUE_SIZE;
    }
    TEST_ASSERT_TRUE(wrQueueFull(head, tail));
}

void test_queue_pop_all_to_empty(void)
{
    uint8_t head = 0, tail = 0;
    // push 5 items
    for (int i = 0; i < 5; i++) {
        head = (head + 1) % WR_QUEUE_SIZE;
    }
    // pop all 5
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_FALSE(wrQueueEmpty(head, tail));
        tail = (tail + 1) % WR_QUEUE_SIZE;
    }
    TEST_ASSERT_TRUE(wrQueueEmpty(head, tail));
}

void test_queue_wraparound(void)
{
    uint8_t head = WR_QUEUE_SIZE - 1, tail = WR_QUEUE_SIZE - 1;
    // push wraps head to 0
    head = (head + 1) % WR_QUEUE_SIZE;
    TEST_ASSERT_EQUAL_UINT8(0, head);
    TEST_ASSERT_FALSE(wrQueueEmpty(head, tail));
    // pop wraps tail to 0
    tail = (tail + 1) % WR_QUEUE_SIZE;
    TEST_ASSERT_EQUAL_UINT8(0, tail);
    TEST_ASSERT_TRUE(wrQueueEmpty(head, tail));
}

void test_queue_full_prevents_overwrite(void)
{
    uint8_t head = 0, tail = 0;
    for (int i = 0; i < WR_QUEUE_SIZE - 1; i++) {
        head = (head + 1) % WR_QUEUE_SIZE;
    }
    TEST_ASSERT_TRUE(wrQueueFull(head, tail));
    // pop one, should no longer be full
    tail = (tail + 1) % WR_QUEUE_SIZE;
    TEST_ASSERT_FALSE(wrQueueFull(head, tail));
}

void test_queue_entry_fields(void)
{
    WrEntry entries[WR_QUEUE_SIZE];
    uint8_t head = 0, tail = 0;

    // push a raw entry
    WrEntry &raw = entries[head];
    raw.isRaw = true;
    raw.seg = 3;
    raw.addr = 0x10;
    raw.val = 108;
    raw.reg[0] = '\0';
    head = (head + 1) % WR_QUEUE_SIZE;

    // push a named entry
    WrEntry &named = entries[head];
    named.isRaw = false;
    strncpy(named.reg, "hscale", sizeof(named.reg) - 1);
    named.reg[sizeof(named.reg) - 1] = '\0';
    named.val = 620;
    head = (head + 1) % WR_QUEUE_SIZE;

    // pop and verify raw entry
    WrEntry &e1 = entries[tail];
    TEST_ASSERT_TRUE(e1.isRaw);
    TEST_ASSERT_EQUAL_UINT8(3, e1.seg);
    TEST_ASSERT_EQUAL_UINT8(0x10, e1.addr);
    TEST_ASSERT_EQUAL_UINT16(108, e1.val);
    tail = (tail + 1) % WR_QUEUE_SIZE;

    // pop and verify named entry
    WrEntry &e2 = entries[tail];
    TEST_ASSERT_FALSE(e2.isRaw);
    TEST_ASSERT_EQUAL_STRING("hscale", e2.reg);
    TEST_ASSERT_EQUAL_UINT16(620, e2.val);
    tail = (tail + 1) % WR_QUEUE_SIZE;

    TEST_ASSERT_TRUE(wrQueueEmpty(head, tail));
}

// ---------------------------------------------------------------------------
// isKnownRegister tests
// ---------------------------------------------------------------------------

void test_known_register_all_61(void)
{
    const char *regs[] = {
        "ht", "vt", "hsst", "hssp", "hbst", "hbsp", "hbstd", "hbspd",
        "vsst", "vssp", "vbst", "vbsp", "vbstd", "vbspd", "hscale", "vscale",
        "sog", "ifhbst", "ifhbsp", "ifhbinst", "ifhbinsp", "ifvbst", "ifvbsp",
        "exthbst", "exthbsp", "extvbst", "extvbsp", "hdhbst", "hdhbsp",
        "hdvbst", "hdvbsp", "pbfetch", "pboffset", "rfffetch", "rffoffset",
        "ifhsyncrst", "ifhbst2", "ifhbsp2", "iflinest", "iflinesp",
        "ifinist", "ifldst", "vdsdsp", "ygain", "yofst", "uofst", "vofst", "synclev",
        "rgain", "ggain", "bgain", "roff", "goff", "boff",
        "ucgain", "vcgain", "pklb", "pklh",
        "slstr", "slsoft", "slbrt"
    };
    for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) {
        TEST_ASSERT_TRUE_MESSAGE(isKnownRegister(regs[i]), regs[i]);
    }
}

void test_unknown_register(void)
{
    TEST_ASSERT_FALSE(isKnownRegister("bogus"));
    TEST_ASSERT_FALSE(isKnownRegister("foobarbaz"));
}

void test_register_case_sensitive(void)
{
    TEST_ASSERT_FALSE(isKnownRegister("HT"));
    TEST_ASSERT_FALSE(isKnownRegister("Ht"));
    TEST_ASSERT_FALSE(isKnownRegister("HSCALE"));
}

void test_register_empty_string(void)
{
    TEST_ASSERT_FALSE(isKnownRegister(""));
}

void test_register_prefix_no_match(void)
{
    TEST_ASSERT_FALSE(isKnownRegister("h"));
    TEST_ASSERT_FALSE(isKnownRegister("hsc"));
    TEST_ASSERT_FALSE(isKnownRegister("ifhb"));
}

void test_register_longest_name(void)
{
    TEST_ASSERT_TRUE(isKnownRegister("ifhsyncrst"));
}

// ---------------------------------------------------------------------------
// parseUint tests
// ---------------------------------------------------------------------------

void test_parse_valid_decimal(void)
{
    unsigned long val;
    TEST_ASSERT_EQUAL(WR_PARSE_OK, parseUint("123", &val, 255));
    TEST_ASSERT_EQUAL_UINT32(123, val);
}

void test_parse_valid_hex(void)
{
    unsigned long val;
    TEST_ASSERT_EQUAL(WR_PARSE_OK, parseUint("0xFF", &val, 255));
    TEST_ASSERT_EQUAL_UINT32(255, val);
}

void test_parse_valid_octal(void)
{
    unsigned long val;
    TEST_ASSERT_EQUAL(WR_PARSE_OK, parseUint("010", &val, 255));
    TEST_ASSERT_EQUAL_UINT32(8, val);
}

void test_parse_zero(void)
{
    unsigned long val;
    TEST_ASSERT_EQUAL(WR_PARSE_OK, parseUint("0", &val, 255));
    TEST_ASSERT_EQUAL_UINT32(0, val);
}

void test_parse_boundary_max(void)
{
    unsigned long val;
    TEST_ASSERT_EQUAL(WR_PARSE_OK, parseUint("255", &val, 255));
    TEST_ASSERT_EQUAL_UINT32(255, val);
}

void test_parse_boundary_max_plus_one(void)
{
    unsigned long val;
    TEST_ASSERT_EQUAL(WR_PARSE_RANGE, parseUint("256", &val, 255));
}

void test_parse_non_numeric(void)
{
    unsigned long val;
    TEST_ASSERT_EQUAL(WR_PARSE_INVALID, parseUint("abc", &val, 255));
}

void test_parse_trailing_chars(void)
{
    unsigned long val;
    TEST_ASSERT_EQUAL(WR_PARSE_INVALID, parseUint("123abc", &val, 255));
}

void test_parse_empty_string(void)
{
    unsigned long val;
    TEST_ASSERT_EQUAL(WR_PARSE_INVALID, parseUint("", &val, 255));
}

void test_parse_null_pointer(void)
{
    unsigned long val;
    TEST_ASSERT_EQUAL(WR_PARSE_INVALID, parseUint(NULL, &val, 255));
}

void test_parse_named_register_boundary(void)
{
    unsigned long val;
    TEST_ASSERT_EQUAL(WR_PARSE_OK, parseUint("4095", &val, 4095));
    TEST_ASSERT_EQUAL_UINT32(4095, val);
    TEST_ASSERT_EQUAL(WR_PARSE_RANGE, parseUint("4096", &val, 4095));
}

void test_parse_negative(void)
{
    unsigned long val;
    TEST_ASSERT_EQUAL(WR_PARSE_INVALID, parseUint("-1", &val, 255));
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    UNITY_BEGIN();

    // ring buffer
    RUN_TEST(test_queue_empty_on_init);
    RUN_TEST(test_queue_push_one);
    RUN_TEST(test_queue_full_at_capacity);
    RUN_TEST(test_queue_pop_all_to_empty);
    RUN_TEST(test_queue_wraparound);
    RUN_TEST(test_queue_full_prevents_overwrite);
    RUN_TEST(test_queue_entry_fields);

    // isKnownRegister
    RUN_TEST(test_known_register_all_61);
    RUN_TEST(test_unknown_register);
    RUN_TEST(test_register_case_sensitive);
    RUN_TEST(test_register_empty_string);
    RUN_TEST(test_register_prefix_no_match);
    RUN_TEST(test_register_longest_name);

    // parseUint
    RUN_TEST(test_parse_valid_decimal);
    RUN_TEST(test_parse_valid_hex);
    RUN_TEST(test_parse_valid_octal);
    RUN_TEST(test_parse_zero);
    RUN_TEST(test_parse_boundary_max);
    RUN_TEST(test_parse_boundary_max_plus_one);
    RUN_TEST(test_parse_non_numeric);
    RUN_TEST(test_parse_trailing_chars);
    RUN_TEST(test_parse_empty_string);
    RUN_TEST(test_parse_null_pointer);
    RUN_TEST(test_parse_named_register_boundary);
    RUN_TEST(test_parse_negative);

    return UNITY_END();
}
