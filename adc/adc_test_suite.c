#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "zephyr_adc_emul_lib.h"


ZTEST(adc_single_sample_tests, test_p1_01_read_button_triggers_adc)
{
    set_ain0_mv(adc_emul_dev, 1500);

    k_event_clear(&program_test_events, ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE);

    simulate_button_click(&read_button);

    uint32_t events = k_event_wait(&program_test_events,
                                   ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE,
                                   false, K_MSEC(1000));

    if(events & ADC_READ_TRIGGERED_NOTICE){
        printk("events & ADC_READ_TRIGGERED_NOTICE \n");
    } else if (events & ADC_READ_COMPLETE_NOTICE){
        printk("events & ADC_READ_COMPLETE_NOTICE\n");
    }

    zassert_true(events & ADC_READ_TRIGGERED_NOTICE, "ADC_READ_TRIGGERED_NOTICE never fired (events=0x%x)", events);

    k_msleep(1000);

    zassert_true(events & ADC_READ_COMPLETE_NOTICE, "ADC_READ_COMPLETE_NOTICE never fired (events=0x%x)", events);

    zassert_within(student_adc_mv, 1500, 200, "student_adc_mv mismatch");
}

/*
 * 0 V → should map to 1 Hz (minimum).
 */
ZTEST(adc_single_sample_tests, test_p1_02_zero_volts_maps_to_1hz)
{
    set_ain0_mv(adc_emul_dev, 0);
    k_event_clear(&program_test_events, ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE);

    simulate_button_click(&read_button);
    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800), "ADC_READ_COMPLETE_NOTICE never fired");

    assert_led_blink_freq(&blinker_led, 3000, 1, 1, "blinker");
    assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat");
}

/*
 * 3000 mV → should map to 5 Hz (maximum).
 */
ZTEST(adc_single_sample_tests, test_p1_03_full_volts_maps_to_5hz)
{
    set_ain0_mv(adc_emul_dev, 2985);
    k_event_clear(&program_test_events, ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE);

    simulate_button_click(&read_button);
    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800), "ADC_READ_COMPLETE_NOTICE never fired");

    assert_led_blink_freq(&blinker_led, 2000,  5, 1, "blinker");
}

/*
 * 1500 mV → should map to 3 Hz (midpoint).
 */
ZTEST(adc_single_sample_tests, test_p1_04_mid_volts_maps_to_3hz)
{
    set_ain0_mv(adc_emul_dev, 1500);
    k_event_clear(&program_test_events, ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE);

    simulate_button_click(&read_button);
    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800), "ADC_READ_COMPLETE_NOTICE never fired");

    assert_led_blink_freq(&blinker_led, 2000, 3, 1, "blinker");
}

/*
 * 1500 mV → verify ~10% duty cycle on blinker_led.
 * Measure over 2 s to accumulate enough on/off cycles.
 */
ZTEST(adc_single_sample_tests, test_p1_05_duty_cycle_10pct)
{
    set_ain0_mv(adc_emul_dev, 1500);
    k_event_clear(&program_test_events, ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE);

    simulate_button_click(&read_button);
    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800), "ADC_READ_COMPLETE_NOTICE never fired");

    assert_blink_ontime_pct(2000, 10, 5);  /* ±5% tolerance */
}

/*
 * Verify blinker_led stays on for ~5 seconds then turns off.
 * ADC_BLINK_DONE_NOTICE must fire.
 */
ZTEST(adc_single_sample_tests, test_p1_06_blink_duration_5s)
{
    set_ain0_mv(adc_emul_dev, 1500);
    k_event_clear(&program_test_events, ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE);

    simulate_button_click(&read_button);
    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800), "ADC_READ_COMPLETE_NOTICE never fired");

    assert_blink_total_duration_ms(BLINKING_TIME_MS, 300); /* ±300 ms */

    k_msleep(100);
    assert_led_off(&blinker_led, "blinker (after 5s)");
}

/*
 * Second read_button press during active blink should be ignored
 * (interrupt is disabled in READING state, not re-enabled until IDLE).
 */
ZTEST(adc_single_sample_tests, test_p1_07_read_button_disabled_during_blink)
{
    set_ain0_mv(adc_emul_dev, 1500);
    k_event_clear(&program_test_events, ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE);

    simulate_button_click(&read_button);

    zassert_true(wait_for_event(ADC_READ_TRIGGERED_NOTICE, 500), "First press not detected");

    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800), "ADC_READ_COMPLETE_NOTICE never fired");

    k_event_clear(&program_test_events, ADC_READ_TRIGGERED_NOTICE);

    /* Second press — should be ignored */
    simulate_button_click(&read_button);
    k_msleep(100);

    uint32_t events = k_event_wait(&program_test_events, ADC_READ_TRIGGERED_NOTICE, false, K_MSEC(100));

    zassert_false(events & ADC_READ_TRIGGERED_NOTICE, "read_button was not disabled: second ADC_READ_TRIGGERED fired");
}

/*
 * Out-of-range voltage → ERROR state.
 * Inject a raw value just above 4095 by setting mv > MAX_V_MV.
 * (The student's reading_run checks val_mv < 0 || val_mv > 3000.)
 *
 * We simulate this by setting AIN0 to produce a raw value the
 * student's adc_raw_to_millivolts_dt maps above 3000 mV.
 * Easiest approach: temporarily override the emulator to return 4096
 * (out-of-range for a 12-bit converter) by setting mv to > MAX_V_MV
 * in the raw count calculation.
 */
static int ain0_over_range_cb(const struct device *dev,
                              unsigned int chan,
                              void *data,
                              uint32_t *result)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(chan);
    ARG_UNUSED(data);
    *result = 4095;  /* max raw; at 3 V ref this hits exactly 3000 mV */
    return 0;
}

ZTEST(adc_single_sample_tests, test_p1_08_error_on_bad_voltage)
{
    k_event_clear(&program_test_events, ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE);

    set_ain0_mv(adc_emul_dev, 15000);

    simulate_button_click(&read_button);

    zassert_true(wait_for_event(ADC_READ_TRIGGERED_NOTICE, 500),
        "ADC_READ_TRIGGERED_NOTICE never fired");

    k_msleep(2000);

    assert_led_on(&error_led, "error");
    assert_led_off(&blinker_led, "blinker");

    simulate_button_click(&reset_button);

    zassert_true(wait_for_event(RESET_TEST_NOTICE, 500), "RESET_TEST_NOTICE not fired");

    k_msleep(2000);
    assert_led_off(&error_led, "error (after reset)");
}

ZTEST(adc_single_sample_tests, test_p1_09_linearity_sweep)
{
    int voltages_mv[] = {0, 750, 1500, 2250, 3000};
    int expected_hz[] = {1, 2, 3, 4, 5};

    float prev_freq = -1.0f;

    for (int i = 0; i < 5; i++) {

        /* --- Reset system cleanly --- */
        stop_main();
        k_msleep(50);

        start_main(500);

        k_event_clear(&program_test_events,
            ADC_READ_TRIGGERED_NOTICE |
            ADC_READ_COMPLETE_NOTICE |
            ADC_BLINK_DONE_NOTICE);

        /* --- Inject ADC value --- */
        set_ain0_mv(adc_emul_dev, voltages_mv[i]);

        /* --- Trigger read --- */
        simulate_button_click(&read_button);

        /* --- Wait for TRIGGERED --- */
        uint32_t events = k_event_wait(&program_test_events,
                                       ADC_READ_TRIGGERED_NOTICE,
                                       false, K_MSEC(300));

        zassert_true(events & ADC_READ_TRIGGERED_NOTICE,
            "Sweep[%d]: ADC_READ_TRIGGERED never fired", i);

        /* --- Wait for COMPLETE --- */
        events = k_event_wait(&program_test_events,
                              ADC_READ_COMPLETE_NOTICE,
                              false, K_MSEC(700));

        zassert_true(events & ADC_READ_COMPLETE_NOTICE,
            "Sweep[%d]: ADC_READ_COMPLETE never fired", i);

        /* --- Monotonic check --- */
        zassert_true(student_mapped_freq > prev_freq,
            "Sweep[%d]: freq %.2f not > prev %.2f",
            i, (double)student_mapped_freq, (double)prev_freq);

        prev_freq = student_mapped_freq;

        /* --- Value check (scaled to int) --- */
        zassert_within((int)(student_mapped_freq * 100),
                       expected_hz[i] * 100, 75,
            "Sweep[%d]: expected ~%d Hz, got %.2f Hz",
            i, expected_hz[i], (double)student_mapped_freq);
    }
}

ZTEST(adc_single_sample_tests, test_p1_10_heartbeat_unaffected)
{
    set_ain0_mv(adc_emul_dev, 1500);
    k_event_clear(&program_test_events, ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE);

    simulate_button_click(&read_button);
    k_event_wait(&program_test_events, ADC_READ_COMPLETE_NOTICE, true, K_MSEC(500));

    assert_led_blink_freq(&heartbeat_led, 3000, 1, 1, "heartbeat (during blink)");
}

/* ================================================================== */
/*  Register suites                                                   */
/* ================================================================== */
ZTEST_SUITE(adc_single_sample_tests, NULL, NULL, before, after, NULL);