#include "unity.h"
#include "logic/state_machine.h"

/*
 * ACTIVE / INACTIVE state machine tests.
 * These exercise the production implementation in
 * src/app/logic/state_machine.c (linked into the native test environment).
 */

#define TEST_TIMEOUT_MS 15000U

static StateMachine_t sm;

void setUp(void) {
    StateMachine_Init(&sm, TEST_TIMEOUT_MS, 0);
}

void tearDown(void) {}

void test_state_initial_active(void) {
    TEST_ASSERT_EQUAL(STATE_ACTIVE, StateMachine_GetState(&sm));
}

void test_state_stays_active_on_motion(void) {
    TEST_ASSERT_EQUAL(STATE_ACTIVE, StateMachine_Update(&sm, 1, 1000));
}

void test_state_resets_timeout_on_motion(void) {
    StateMachine_Update(&sm, 1, 5000);
    TEST_ASSERT_EQUAL(STATE_ACTIVE, StateMachine_Update(&sm, 0, 10000));
}

void test_state_transitions_to_inactive(void) {
    StateMachine_Update(&sm, 1, 0);
    TEST_ASSERT_EQUAL(STATE_INACTIVE, StateMachine_Update(&sm, 0, TEST_TIMEOUT_MS + 1));
}

void test_state_boundary_exact_timeout(void) {
    StateMachine_Update(&sm, 1, 0);
    TEST_ASSERT_EQUAL(STATE_INACTIVE, StateMachine_Update(&sm, 0, TEST_TIMEOUT_MS));
}

void test_state_stays_inactive_without_motion(void) {
    StateMachine_Update(&sm, 1, 0);
    StateMachine_Update(&sm, 0, TEST_TIMEOUT_MS + 1);
    TEST_ASSERT_EQUAL(STATE_INACTIVE, StateMachine_Update(&sm, 0, TEST_TIMEOUT_MS + 2000));
}

void test_state_returns_to_active_on_motion(void) {
    StateMachine_Update(&sm, 1, 0);
    StateMachine_Update(&sm, 0, TEST_TIMEOUT_MS + 1);
    TEST_ASSERT_EQUAL(STATE_ACTIVE, StateMachine_Update(&sm, 1, TEST_TIMEOUT_MS + 2000));
}

void test_state_continuous_motion_keeps_active(void) {
    for (uint32_t t = 0; t < 100000; t += 1000) {
        StateMachine_Update(&sm, 1, t);
    }
    TEST_ASSERT_EQUAL(STATE_ACTIVE, StateMachine_GetState(&sm));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_state_initial_active);
    RUN_TEST(test_state_stays_active_on_motion);
    RUN_TEST(test_state_resets_timeout_on_motion);
    RUN_TEST(test_state_transitions_to_inactive);
    RUN_TEST(test_state_boundary_exact_timeout);
    RUN_TEST(test_state_stays_inactive_without_motion);
    RUN_TEST(test_state_returns_to_active_on_motion);
    RUN_TEST(test_state_continuous_motion_keeps_active);

    return UNITY_END();
}
