#include "unity.h"

#define TEMP_LOW_THRESHOLD  18.0f
#define TEMP_HIGH_THRESHOLD 30.0f

typedef enum {
    TEMP_LOW = 0,
    TEMP_NORMAL,
    TEMP_HIGH
} TempStatus_t;

TempStatus_t EvaluateTemperature(float temperature) {
    if (temperature < TEMP_LOW_THRESHOLD) {
        return TEMP_LOW;
    } else if (temperature > TEMP_HIGH_THRESHOLD) {
        return TEMP_HIGH;
    }
    return TEMP_NORMAL;
}

const char* Temperature_GetStatusString(TempStatus_t status) {
    switch (status) {
        case TEMP_LOW:    return "LOW";
        case TEMP_NORMAL: return "NORMAL";
        case TEMP_HIGH:   return "HIGH";
        default:          return "UNKNOWN";
    }
}

uint8_t Temperature_IsAlarm(TempStatus_t status) {
    return (status != TEMP_NORMAL);
}

void setUp(void) {}
void tearDown(void) {}

void test_temp_below_low_boundary(void) {
    TEST_ASSERT_EQUAL(TEMP_LOW, EvaluateTemperature(17.9f));
}

void test_temp_at_low_boundary(void) {
    TEST_ASSERT_EQUAL(TEMP_NORMAL, EvaluateTemperature(18.0f));
}

void test_temp_just_above_low(void) {
    TEST_ASSERT_EQUAL(TEMP_NORMAL, EvaluateTemperature(18.1f));
}

void test_temp_mid_normal(void) {
    TEST_ASSERT_EQUAL(TEMP_NORMAL, EvaluateTemperature(24.0f));
}

void test_temp_at_high_boundary(void) {
    TEST_ASSERT_EQUAL(TEMP_NORMAL, EvaluateTemperature(30.0f));
}

void test_temp_just_above_high(void) {
    TEST_ASSERT_EQUAL(TEMP_HIGH, EvaluateTemperature(30.1f));
}

void test_temp_extreme_high(void) {
    TEST_ASSERT_EQUAL(TEMP_HIGH, EvaluateTemperature(50.0f));
}

void test_temp_extreme_low(void) {
    TEST_ASSERT_EQUAL(TEMP_LOW, EvaluateTemperature(-10.0f));
}

void test_temp_zero(void) {
    TEST_ASSERT_EQUAL(TEMP_LOW, EvaluateTemperature(0.0f));
}

void test_status_string_low(void) {
    TEST_ASSERT_EQUAL_STRING("LOW", Temperature_GetStatusString(TEMP_LOW));
}

void test_status_string_normal(void) {
    TEST_ASSERT_EQUAL_STRING("NORMAL", Temperature_GetStatusString(TEMP_NORMAL));
}

void test_status_string_high(void) {
    TEST_ASSERT_EQUAL_STRING("HIGH", Temperature_GetStatusString(TEMP_HIGH));
}

void test_is_alarm_low(void) {
    TEST_ASSERT_TRUE(Temperature_IsAlarm(TEMP_LOW));
}

void test_is_alarm_normal(void) {
    TEST_ASSERT_FALSE(Temperature_IsAlarm(TEMP_NORMAL));
}

void test_is_alarm_high(void) {
    TEST_ASSERT_TRUE(Temperature_IsAlarm(TEMP_HIGH));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_temp_below_low_boundary);
    RUN_TEST(test_temp_at_low_boundary);
    RUN_TEST(test_temp_just_above_low);
    RUN_TEST(test_temp_mid_normal);
    RUN_TEST(test_temp_at_high_boundary);
    RUN_TEST(test_temp_just_above_high);
    RUN_TEST(test_temp_extreme_high);
    RUN_TEST(test_temp_extreme_low);
    RUN_TEST(test_temp_zero);
    RUN_TEST(test_status_string_low);
    RUN_TEST(test_status_string_normal);
    RUN_TEST(test_status_string_high);
    RUN_TEST(test_is_alarm_low);
    RUN_TEST(test_is_alarm_normal);
    RUN_TEST(test_is_alarm_high);

    return UNITY_END();
}
