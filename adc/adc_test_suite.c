#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "zephyr_adc_emul_lib.h"

/*
 * test_p1_01 — pressing read_button triggers an ADC read.
 * Both ADC_READ_TRIGGERED_NOTICE and ADC_READ_COMPLETE_NOTICE must fire,
 * and student_adc_mv must be within ±200 mV of the injected value.
 */
ZTEST(adc_single_sample_tests, test_p1_01_read_button_triggers_adc)
{
    set_ain0_mv(adc_emul_dev, 1500);
    k_event_clear(&program_test_events,
                  ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE);

    simulate_button_click(&read_button);

    uint32_t events = k_event_wait(&program_test_events,
                                   ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE,
                                   false, K_MSEC(1000));

    zassert_true(events & ADC_READ_TRIGGERED_NOTICE,
        "ADC_READ_TRIGGERED_NOTICE never fired (events=0x%x)", events);

    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 1000),
        "ADC_READ_COMPLETE_NOTICE never fired");

    zassert_within(student_adc_mv, 1500, 200, "student_adc_mv mismatch");
}

/*
 * test_p1_02 — 0 V maps to 1 Hz blink rate.
 */
ZTEST(adc_single_sample_tests, test_p1_02_zero_volts_maps_to_1hz)
{
    set_ain0_mv(adc_emul_dev, 0);
    k_event_clear(&program_test_events,
                  ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE);

    simulate_button_click(&read_button);
    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800),
        "ADC_READ_COMPLETE_NOTICE never fired");

    assert_led_blink_freq(&blinker_led,    3000, 1, 1, "blinker");
    assert_led_blink_freq(&heartbeat_led,  2000, 1, 1, "heartbeat");
}

/*
 * test_p1_03 — ~3000 mV maps to 5 Hz blink rate.
 */
ZTEST(adc_single_sample_tests, test_p1_03_full_volts_maps_to_5hz)
{
    set_ain0_mv(adc_emul_dev, 2985);
    k_event_clear(&program_test_events,
                  ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE);

    simulate_button_click(&read_button);
    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800),
        "ADC_READ_COMPLETE_NOTICE never fired");

    assert_led_blink_freq(&blinker_led, 2000, 5, 1, "blinker");
}

/*
 * test_p1_04 — 1500 mV maps to 3 Hz blink rate (midpoint).
 */
ZTEST(adc_single_sample_tests, test_p1_04_mid_volts_maps_to_3hz)
{
    set_ain0_mv(adc_emul_dev, 1500);
    k_event_clear(&program_test_events,
                  ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE);

    simulate_button_click(&read_button);
    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800),
        "ADC_READ_COMPLETE_NOTICE never fired");

    assert_led_blink_freq(&blinker_led, 2000, 3, 1, "blinker");
}

/*
 * test_p1_05 — 1500 mV → ~10% duty cycle on blinker_led.
 */
ZTEST(adc_single_sample_tests, test_p1_05_duty_cycle_10pct)
{
    set_ain0_mv(adc_emul_dev, 1500);
    k_event_clear(&program_test_events,
                  ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE);

    simulate_button_click(&read_button);
    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800),
        "ADC_READ_COMPLETE_NOTICE never fired");

    assert_blink_ontime_pct(2000, 10, 5);  /* ±5% */
}

/*
 * test_p1_06 — blinker_led stays active for ~5 seconds then turns off.
 */
ZTEST(adc_single_sample_tests, test_p1_06_blink_duration_5s)
{
    set_ain0_mv(adc_emul_dev, 1500);
    k_event_clear(&program_test_events,
                  ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE);

    simulate_button_click(&read_button);
    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800),
        "ADC_READ_COMPLETE_NOTICE never fired");

    assert_blink_total_duration_ms(BLINKING_TIME_MS, 300);

    k_msleep(100);
    assert_led_off(&blinker_led, "blinker (after 5 s)");
}

/*
 * test_p1_07 — second read_button press during active blink is ignored.
 */
ZTEST(adc_single_sample_tests, test_p1_07_read_button_disabled_during_blink)
{
    set_ain0_mv(adc_emul_dev, 1500);
    k_event_clear(&program_test_events,
                  ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE);

    simulate_button_click(&read_button);

    zassert_true(wait_for_event(ADC_READ_TRIGGERED_NOTICE, 500),
        "First press not detected");
    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800),
        "ADC_READ_COMPLETE_NOTICE never fired");

    k_event_clear(&program_test_events, ADC_READ_TRIGGERED_NOTICE);

    /* Second press — must be ignored */
    simulate_button_click(&read_button);
    k_msleep(100);

    uint32_t events = k_event_wait(&program_test_events,
                                   ADC_READ_TRIGGERED_NOTICE,
                                   false, K_MSEC(100));
    zassert_false(events & ADC_READ_TRIGGERED_NOTICE,
        "read_button was not disabled: second ADC_READ_TRIGGERED fired");
}

/*
 * test_p1_08 — out-of-range voltage → ERROR state → reset clears it.
 */
ZTEST(adc_single_sample_tests, test_p1_08_error_on_bad_voltage)
{
    k_event_clear(&program_test_events,
                  ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE);

    set_ain0_mv(adc_emul_dev, 15000);   /* well above MAX_V_MV */

    simulate_button_click(&read_button);

    zassert_true(wait_for_event(ADC_READ_TRIGGERED_NOTICE, 500),
        "ADC_READ_TRIGGERED_NOTICE never fired");

    k_msleep(500);

    assert_led_on(&error_led,   "error");
    assert_led_off(&blinker_led, "blinker");

    simulate_button_click(&reset_button);

    zassert_true(wait_for_event(RESET_TEST_NOTICE, 500),
        "RESET_TEST_NOTICE not fired");

    k_msleep(500);
    assert_led_off(&error_led, "error (after reset)");
}

/*
 * test_p1_09 — linearity sweep: 0, 750, 1500, 2250, 3000 mV
 *              must produce monotonically increasing Hz values
 *              matching 1, 2, 3, 4, 5 Hz respectively.
 */
ZTEST(adc_single_sample_tests, test_p1_09_linearity_sweep)
{
    int   voltages_mv[] = { 0, 750, 1500, 2250, 3000 };
    int   expected_hz[] = { 1,   2,    3,    4,    5 };
    float prev_freq = -1.0f;

    for (int i = 0; i < 5; i++) {

        stop_main();
        k_msleep(50);
        start_main(500);

        k_event_clear(&program_test_events,
            ADC_READ_TRIGGERED_NOTICE |
            ADC_READ_COMPLETE_NOTICE  |
            ADC_BLINK_DONE_NOTICE);

        set_ain0_mv(adc_emul_dev, voltages_mv[i]);

        simulate_button_click(&read_button);

        uint32_t events = k_event_wait(&program_test_events,
                                       ADC_READ_TRIGGERED_NOTICE,
                                       false, K_MSEC(300));
        zassert_true(events & ADC_READ_TRIGGERED_NOTICE,
            "Sweep[%d]: ADC_READ_TRIGGERED never fired", i);

        events = k_event_wait(&program_test_events,
                              ADC_READ_COMPLETE_NOTICE,
                              false, K_MSEC(700));
        zassert_true(events & ADC_READ_COMPLETE_NOTICE,
            "Sweep[%d]: ADC_READ_COMPLETE never fired", i);

        zassert_true(student_mapped_freq > prev_freq,
            "Sweep[%d]: freq %.2f not > prev %.2f",
            i, (double)student_mapped_freq, (double)prev_freq);

        prev_freq = student_mapped_freq;

        zassert_within((int)(student_mapped_freq * 100),
                       expected_hz[i] * 100, 75,
            "Sweep[%d]: expected ~%d Hz, got %.2f Hz",
            i, expected_hz[i], (double)student_mapped_freq);
    }
}

/*
 * test_p1_10 — heartbeat LED continues at 1 Hz while blinker is active.
 */
ZTEST(adc_single_sample_tests, test_p1_10_heartbeat_unaffected)
{
    set_ain0_mv(adc_emul_dev, 1500);
    k_event_clear(&program_test_events,
                  ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE);

    simulate_button_click(&read_button);
    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800),
        "ADC_READ_COMPLETE_NOTICE never fired");

    assert_led_blink_freq(&heartbeat_led, 3000, 1, 1,
                          "heartbeat (during blink)");
}

/* ------------------------------------------------------------------ */
ZTEST_SUITE(adc_single_sample_tests, NULL, NULL, before, after, NULL);