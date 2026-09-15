#include "unity.h"
#include <stdint.h>

#define PAGE_COUNT 4

typedef enum {
    TEST_PAGE_TEMPERATURE = 0,
    TEST_PAGE_HUMIDITY,
    TEST_PAGE_LIGHT,
    TEST_PAGE_MOTION
} TestDisplayPage_t;

static TestDisplayPage_t encoder_position = TEST_PAGE_TEMPERATURE;

static void encoder_increment(void) {
    encoder_position = (TestDisplayPage_t)((encoder_position + 1) % PAGE_COUNT);
}

static void encoder_decrement(void) {
    encoder_position = (TestDisplayPage_t)((encoder_position + PAGE_COUNT - 1) % PAGE_COUNT);
}

void setUp(void) {
    encoder_position = TEST_PAGE_TEMPERATURE;
}

void tearDown(void) {}

void test_encoder_initial_position(void) {
    TEST_ASSERT_EQUAL(TEST_PAGE_TEMPERATURE, encoder_position);
}

void test_encoder_increment_once(void) {
    encoder_increment();
    TEST_ASSERT_EQUAL(TEST_PAGE_HUMIDITY, encoder_position);
}

void test_encoder_increment_twice(void) {
    encoder_increment();
    encoder_increment();
    TEST_ASSERT_EQUAL(TEST_PAGE_LIGHT, encoder_position);
}

void test_encoder_increment_three_times(void) {
    encoder_increment();
    encoder_increment();
    encoder_increment();
    TEST_ASSERT_EQUAL(TEST_PAGE_MOTION, encoder_position);
}

void test_encoder_wrap_around_cw(void) {
    encoder_position = TEST_PAGE_MOTION;
    encoder_increment();
    TEST_ASSERT_EQUAL(TEST_PAGE_TEMPERATURE, encoder_position);
}

void test_encoder_decrement_once(void) {
    encoder_position = TEST_PAGE_HUMIDITY;
    encoder_decrement();
    TEST_ASSERT_EQUAL(TEST_PAGE_TEMPERATURE, encoder_position);
}

void test_encoder_wrap_around_ccw(void) {
    encoder_decrement();
    TEST_ASSERT_EQUAL(TEST_PAGE_MOTION, encoder_position);
}

void test_encoder_full_cycle_cw(void) {
    for (int i = 0; i < PAGE_COUNT; i++) {
        encoder_increment();
    }
    TEST_ASSERT_EQUAL(TEST_PAGE_TEMPERATURE, encoder_position);
}

void test_encoder_full_cycle_ccw(void) {
    for (int i = 0; i < PAGE_COUNT; i++) {
        encoder_decrement();
    }
    TEST_ASSERT_EQUAL(TEST_PAGE_TEMPERATURE, encoder_position);
}

void test_encoder_mixed_operations(void) {
    encoder_increment();
    encoder_increment();
    encoder_decrement();
    TEST_ASSERT_EQUAL(TEST_PAGE_HUMIDITY, encoder_position);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_encoder_initial_position);
    RUN_TEST(test_encoder_increment_once);
    RUN_TEST(test_encoder_increment_twice);
    RUN_TEST(test_encoder_increment_three_times);
    RUN_TEST(test_encoder_wrap_around_cw);
    RUN_TEST(test_encoder_decrement_once);
    RUN_TEST(test_encoder_wrap_around_ccw);
    RUN_TEST(test_encoder_full_cycle_cw);
    RUN_TEST(test_encoder_full_cycle_ccw);
    RUN_TEST(test_encoder_mixed_operations);

    return UNITY_END();
}
