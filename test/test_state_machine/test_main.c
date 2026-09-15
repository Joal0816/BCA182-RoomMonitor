#include "unity.h"
#include <stdint.h>
#include <string.h>

#define INACTIVE_TIMEOUT_MS 15000U

typedef enum {
    TEST_STATE_ACTIVE = 0,
    TEST_STATE_INACTIVE
} TestSystemState_t;

typedef struct {
    TestSystemState_t current_state;
    uint32_t last_motion_tick;
    uint32_t timeout_ms;
    uint32_t current_tick;
} TestStateMachine_t;

static void sm_init(TestStateMachine_t *sm, uint32_t timeout_ms) {
    sm->current_state = TEST_STATE_ACTIVE;
    sm->last_motion_tick = 0;
    sm->timeout_ms = timeout_ms;
    sm->current_tick = 0;
}

static TestSystemState_t sm_update(TestStateMachine_t *sm, uint8_t motion_detected) {
    if (motion_detected) {
        sm->current_state = TEST_STATE_ACTIVE;
        sm->last_motion_tick = sm->current_tick;
        return sm->current_state;
    }

    if (sm->current_state == TEST_STATE_ACTIVE) {
        if ((sm->current_tick - sm->last_motion_tick) >= sm->timeout_ms) {
            sm->current_state = TEST_STATE_INACTIVE;
        }
    }

    return sm->current_state;
}

static TestStateMachine_t sm;

void setUp(void) {
    sm_init(&sm, INACTIVE_TIMEOUT_MS);
}

void tearDown(void) {}

void test_state_initial_active(void) {
    TEST_ASSERT_EQUAL(TEST_STATE_ACTIVE, sm.current_state);
}

void test_state_stays_active_on_motion(void) {
    sm.current_tick = 1000;
    sm_update(&sm, 1);
    TEST_ASSERT_EQUAL(TEST_STATE_ACTIVE, sm.current_state);
}

void test_state_resets_timeout_on_motion(void) {
    sm.current_tick = 5000;
    sm_update(&sm, 1);
    sm.current_tick = 10000;
    sm_update(&sm, 0);
    TEST_ASSERT_EQUAL(TEST_STATE_ACTIVE, sm.current_state);
}

void test_state_transitions_to_inactive(void) {
    sm.current_tick = 0;
    sm_update(&sm, 1);
    sm.current_tick = INACTIVE_TIMEOUT_MS + 1;
    sm_update(&sm, 0);
    TEST_ASSERT_EQUAL(TEST_STATE_INACTIVE, sm.current_state);
}

void test_state_stays_inactive_without_motion(void) {
    sm.current_tick = 0;
    sm_update(&sm, 1);
    sm.current_tick = INACTIVE_TIMEOUT_MS + 1;
    sm_update(&sm, 0);
    sm.current_tick = INACTIVE_TIMEOUT_MS + 2000;
    sm_update(&sm, 0);
    TEST_ASSERT_EQUAL(TEST_STATE_INACTIVE, sm.current_state);
}

void test_state_returns_to_active_on_motion(void) {
    sm.current_tick = 0;
    sm_update(&sm, 1);
    sm.current_tick = INACTIVE_TIMEOUT_MS + 1;
    sm_update(&sm, 0);
    TEST_ASSERT_EQUAL(TEST_STATE_INACTIVE, sm.current_state);

    sm.current_tick = INACTIVE_TIMEOUT_MS + 2000;
    sm_update(&sm, 1);
    TEST_ASSERT_EQUAL(TEST_STATE_ACTIVE, sm.current_state);
}

void test_state_continuous_motion_keeps_active(void) {
    for (int i = 0; i < 100; i++) {
        sm.current_tick = i * 1000;
        sm_update(&sm, 1);
    }
    TEST_ASSERT_EQUAL(TEST_STATE_ACTIVE, sm.current_state);
}

void test_state_alternating_motion(void) {
    sm.current_tick = 0;
    sm_update(&sm, 1);

    for (int i = 1; i < 50; i++) {
        sm.current_tick = i * 500;
        sm_update(&sm, (i % 2 == 0) ? 1 : 0);
    }
    TEST_ASSERT_EQUAL(TEST_STATE_ACTIVE, sm.current_state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_state_initial_active);
    RUN_TEST(test_state_stays_active_on_motion);
    RUN_TEST(test_state_resets_timeout_on_motion);
    RUN_TEST(test_state_transitions_to_inactive);
    RUN_TEST(test_state_stays_inactive_without_motion);
    RUN_TEST(test_state_returns_to_active_on_motion);
    RUN_TEST(test_state_continuous_motion_keeps_active);
    RUN_TEST(test_state_alternating_motion);

    return UNITY_END();
}
