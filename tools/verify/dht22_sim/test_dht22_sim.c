#include "unity.h"
#include "dht22.h"
#include "dht22_sim.h"

static DHT22_t dht;

void setUp(void) {
    sim_reset(GPIO_PIN_1);
    DHT22_Init(&dht, &sim_port, GPIO_PIN_1);
}

void tearDown(void) {
    /* A test may move SystemCoreClock to model a mis-identified clock; restore
       the nominal rate so the next test starts from the normal case. */
    SystemCoreClock = 72000000U;
}

/* Encode one sensor frame the way the DHT22 does: humidity and temperature are
 * tenths of a unit, the temperature sign is a magnitude bit (not two's
 * complement), and the fifth byte is the low byte of the sum of the first four. */
static void make_frame(uint8_t d[5], uint16_t humidity10, int negative,
                       uint16_t temperature10) {
    d[0] = (uint8_t)(humidity10 >> 8);
    d[1] = (uint8_t)(humidity10 & 0xFFU);
    d[2] = (uint8_t)((negative ? 0x80U : 0x00U) | ((temperature10 >> 8) & 0x7FU));
    d[3] = (uint8_t)(temperature10 & 0xFFU);
    d[4] = (uint8_t)(d[0] + d[1] + d[2] + d[3]);
}

void test_read_decodes_temperature_and_humidity(void) {
    uint8_t d[5];
    make_frame(d, 650U, 0, 240U); /* 65.0 %RH, 24.0 C */

    sim_load_bytes(d);

    TEST_ASSERT_EQUAL(DHT22_OK, DHT22_Read(&dht));
    TEST_ASSERT_EQUAL(65.0f, DHT22_GetHumidity(&dht));
    TEST_ASSERT_EQUAL(24.0f, DHT22_GetTemperature(&dht));
}

void test_read_decodes_tenths(void) {
    uint8_t d[5];
    make_frame(d, 653U, 0, 234U); /* 65.3 %RH, 23.4 C */

    sim_load_bytes(d);

    TEST_ASSERT_EQUAL(DHT22_OK, DHT22_Read(&dht));
    TEST_ASSERT_EQUAL(65.3f, DHT22_GetHumidity(&dht));
    TEST_ASSERT_EQUAL(23.4f, DHT22_GetTemperature(&dht));
}

void test_read_decodes_negative_temperature(void) {
    uint8_t d[5];
    make_frame(d, 400U, 1, 240U); /* 40.0 %RH, -24.0 C */

    sim_load_bytes(d);

    TEST_ASSERT_EQUAL(DHT22_OK, DHT22_Read(&dht));
    TEST_ASSERT_EQUAL(-24.0f, DHT22_GetTemperature(&dht));
}

void test_read_decodes_zero(void) {
    uint8_t d[5];
    make_frame(d, 0U, 0, 0U);

    sim_load_bytes(d);

    TEST_ASSERT_EQUAL(DHT22_OK, DHT22_Read(&dht));
    TEST_ASSERT_EQUAL(0.0f, DHT22_GetTemperature(&dht));
    TEST_ASSERT_EQUAL(0.0f, DHT22_GetHumidity(&dht));
}

void test_read_records_raw_bytes(void) {
    uint8_t d[5];
    make_frame(d, 512U, 0, 256U);

    sim_load_bytes(d);
    TEST_ASSERT_EQUAL(DHT22_OK, DHT22_Read(&dht));

    TEST_ASSERT_EQUAL(d[0], dht.last_data[0]);
    TEST_ASSERT_EQUAL(d[1], dht.last_data[1]);
    TEST_ASSERT_EQUAL(d[2], dht.last_data[2]);
    TEST_ASSERT_EQUAL(d[3], dht.last_data[3]);
    TEST_ASSERT_EQUAL(d[4], dht.last_data[4]);
}

void test_read_reports_bad_checksum(void) {
    uint8_t d[5];
    make_frame(d, 650U, 0, 240U);
    d[4] = (uint8_t)(d[4] ^ 0xFFU);

    sim_load_bytes(d);

    TEST_ASSERT_EQUAL(DHT22_ERROR, DHT22_Read(&dht));
    TEST_ASSERT_EQUAL(0.0f, DHT22_GetTemperature(&dht));
}

void test_read_times_out_when_no_response(void) {
    sim_set_mode(DHT22_SIM_NONE);

    TEST_ASSERT_EQUAL(DHT22_TIMEOUT, DHT22_Read(&dht));
}

void test_read_times_out_when_line_stuck_low(void) {
    sim_set_mode(DHT22_SIM_STUCK_LOW);

    TEST_ASSERT_EQUAL(DHT22_TIMEOUT, DHT22_Read(&dht));
}

void test_read_times_out_when_reply_truncated(void) {
    sim_set_mode(DHT22_SIM_TRUNCATED);

    TEST_ASSERT_EQUAL(DHT22_TIMEOUT, DHT22_Read(&dht));
}

void test_line_is_released_after_frame(void) {
    uint8_t d[5];
    make_frame(d, 650U, 0, 240U);
    sim_load_bytes(d);

    TEST_ASSERT_EQUAL(DHT22_OK, DHT22_Read(&dht));

    /* PB1 -> CRL bits [7:4]; CNF=10 (input with pull-up) + MODE=00 is 0x8. */
    TEST_ASSERT_EQUAL(0x0080U, sim_port.CRL & 0x00F0U);
}

void test_decode_threshold_accepts_48us_as_zero(void) {
    uint8_t d[5];
    make_frame(d, 650U, 0, 240U); /* first transmitted bit is 0 */
    sim_load_bytes(d);
    sim_set_width(0, 48U); /* 48 us is still "not greater than 48" -> a 0 bit */

    TEST_ASSERT_EQUAL(DHT22_OK, DHT22_Read(&dht));
}

void test_decode_threshold_accepts_49us_as_one(void) {
    uint8_t d[5];
    make_frame(d, 650U, 0, 240U); /* first transmitted bit is 0 */
    sim_load_bytes(d);
    sim_set_width(0, 49U); /* now decodes as a 1, so the checksum no longer holds */

    TEST_ASSERT_EQUAL(DHT22_ERROR, DHT22_Read(&dht));
}

void test_read_is_repeatable(void) {
    uint8_t d[5];
    make_frame(d, 650U, 0, 240U);
    sim_load_bytes(d);

    TEST_ASSERT_EQUAL(DHT22_OK, DHT22_Read(&dht));
    TEST_ASSERT_EQUAL(24.0f, DHT22_GetTemperature(&dht));

    sim_load_bytes(d);
    TEST_ASSERT_EQUAL(DHT22_OK, DHT22_Read(&dht));
    TEST_ASSERT_EQUAL(24.0f, DHT22_GetTemperature(&dht));
}

void test_read_recovers_after_timeout(void) {
    uint8_t d[5];

    sim_set_mode(DHT22_SIM_NONE);
    TEST_ASSERT_EQUAL(DHT22_TIMEOUT, DHT22_Read(&dht));

    make_frame(d, 650U, 0, 240U);
    sim_load_bytes(d);
    TEST_ASSERT_EQUAL(DHT22_OK, DHT22_Read(&dht));
    TEST_ASSERT_EQUAL(24.0f, DHT22_GetTemperature(&dht));
}

/* The firmware can believe it runs at one clock while the counter advances at
 * another: Wokwi drives the core at the board's nominal 72 MHz whatever RCC
 * prescalers the firmware programmed, so a build that ends up on the 8 MHz HSI
 * fallback sees SystemCoreClock and the counter disagree by 9x.  The decode
 * must take its scale from the reply, not from SystemCoreClock, or every bit
 * reads as a 1 and the checksum fails. */
void test_read_decodes_when_counter_rate_disagrees_with_system_clock(void) {
    uint8_t d[5];
    make_frame(d, 650U, 0, 240U); /* 65.0 %RH, 24.0 C */
    sim_load_bytes(d);

    SystemCoreClock = 8000000U;         /* firmware thinks the core is 8 MHz */
    sim_set_counter_cycles_per_us(72U); /* the counter really ticks at 72 MHz */

    TEST_ASSERT_EQUAL(DHT22_OK, DHT22_Read(&dht));
    TEST_ASSERT_EQUAL(65.0f, DHT22_GetHumidity(&dht));
    TEST_ASSERT_EQUAL(24.0f, DHT22_GetTemperature(&dht));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_read_decodes_temperature_and_humidity);
    RUN_TEST(test_read_decodes_tenths);
    RUN_TEST(test_read_decodes_negative_temperature);
    RUN_TEST(test_read_decodes_zero);
    RUN_TEST(test_read_records_raw_bytes);
    RUN_TEST(test_read_reports_bad_checksum);
    RUN_TEST(test_read_times_out_when_no_response);
    RUN_TEST(test_read_times_out_when_line_stuck_low);
    RUN_TEST(test_read_times_out_when_reply_truncated);
    RUN_TEST(test_line_is_released_after_frame);
    RUN_TEST(test_decode_threshold_accepts_48us_as_zero);
    RUN_TEST(test_decode_threshold_accepts_49us_as_one);
    RUN_TEST(test_read_is_repeatable);
    RUN_TEST(test_read_recovers_after_timeout);
    RUN_TEST(test_read_decodes_when_counter_rate_disagrees_with_system_clock);
    return UNITY_END();
}
