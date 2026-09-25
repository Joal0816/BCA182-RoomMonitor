#include "unity.h"
#include "logic/display_page.h"

/*
 * Rotary-encoder navigation tests (FR-06).
 * These exercise the production implementation in
 * src/app/logic/display_page.c (linked into the native test environment).
 */

void setUp(void) {}
void tearDown(void) {}

void test_nav_initial_page(void) {
    TEST_ASSERT_EQUAL(PAGE_TEMPERATURE, PAGE_TEMPERATURE);
}

void test_nav_next_once(void) {
    TEST_ASSERT_EQUAL(PAGE_HUMIDITY, DisplayPage_Next(PAGE_TEMPERATURE));
}

void test_nav_next_twice(void) {
    TEST_ASSERT_EQUAL(PAGE_LIGHT, DisplayPage_Next(DisplayPage_Next(PAGE_TEMPERATURE)));
}

void test_nav_next_three_times(void) {
    DisplayPage_t p = DisplayPage_Next(DisplayPage_Next(DisplayPage_Next(PAGE_TEMPERATURE)));
    TEST_ASSERT_EQUAL(PAGE_MOTION, p);
}

void test_nav_next_wrap_around(void) {
    TEST_ASSERT_EQUAL(PAGE_TEMPERATURE, DisplayPage_Next(PAGE_MOTION));
}

void test_nav_previous_once(void) {
    TEST_ASSERT_EQUAL(PAGE_TEMPERATURE, DisplayPage_Previous(PAGE_HUMIDITY));
}

void test_nav_previous_wrap_around(void) {
    TEST_ASSERT_EQUAL(PAGE_MOTION, DisplayPage_Previous(PAGE_TEMPERATURE));
}

void test_nav_full_cycle_forward(void) {
    DisplayPage_t p = PAGE_TEMPERATURE;
    for (int i = 0; i < PAGE_COUNT; i++) {
        p = DisplayPage_Next(p);
    }
    TEST_ASSERT_EQUAL(PAGE_TEMPERATURE, p);
}

void test_nav_full_cycle_reverse(void) {
    DisplayPage_t p = PAGE_TEMPERATURE;
    for (int i = 0; i < PAGE_COUNT; i++) {
        p = DisplayPage_Previous(p);
    }
    TEST_ASSERT_EQUAL(PAGE_TEMPERATURE, p);
}

void test_nav_opposite_operations_cancel(void) {
    DisplayPage_t p = DisplayPage_Previous(DisplayPage_Next(PAGE_TEMPERATURE));
    TEST_ASSERT_EQUAL(PAGE_TEMPERATURE, p);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_nav_initial_page);
    RUN_TEST(test_nav_next_once);
    RUN_TEST(test_nav_next_twice);
    RUN_TEST(test_nav_next_three_times);
    RUN_TEST(test_nav_next_wrap_around);
    RUN_TEST(test_nav_previous_once);
    RUN_TEST(test_nav_previous_wrap_around);
    RUN_TEST(test_nav_full_cycle_forward);
    RUN_TEST(test_nav_full_cycle_reverse);
    RUN_TEST(test_nav_opposite_operations_cancel);

    return UNITY_END();
}
