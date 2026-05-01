#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "events_test.h"
#include "zephyr_events_test_lib.h"

// ZTEST(state_machine_tests, test_01_default_frequencies)
// {
//     start_main(1000);
    
//     assert_led_blink_freq(&heartbeat_led, 4000, 1, 1, "heartbeat");
//     assert_led_blink_freq(&iv_pump_led, 4000, 2, 1, "iv_pump");
//     assert_led_blink_freq(&buzzer_led, 4000, 2, 1, "buzzer");
//     assert_led_off(&error_led, "error");
// }

ZTEST(events_tests, test_led_toggles)
{
    start_main(1000);
    
    bool first = is_led_on(&blinking_led);
    k_msleep(1100);  // slightly more than period
    bool second = is_led_on(&blinking_led);

    zassert_not_equal(first, second,
                      "LED did not toggle");
}

ZTEST(events_tests, test_blink_frequency)
{
    start_main(1000);
    
    assert_led_blink_freq(&blinking_led,
                          4000,   /* observe for 4 seconds */
                          1,      /* expected 1 Hz */
                          1,      /* allow small tolerance */
                          "blinking LED");
}

/* Verify duty cycle ≈ 50% */
ZTEST(events_tests, test_duty_cycle)
{
    start_main(1000);
    
    assert_led_duty_cycle(&blinking_led,
                          "blinking LED",
                          4000,   /* observation window */
                          50,     /* expected 50% */
                          10);    /* ±10% tolerance */
}

ZTEST(events_tests, test_led_on_event_matches_state)
{
    start_main(1000);

    uint32_t events = k_event_wait(&button_events,
                               EVENT_LED_ON,
                               false,
                               K_MSEC(2000));

    zassert_equal(events & EVENT_LED_ON, EVENT_LED_ON,
                  "Did not receive LED_ON event");

    assert_led_on(&blinking_led, "blinking LED");
}

ZTEST(events_tests, test_led_off_event_matches_state)
{
    start_main(1000);

    uint32_t events = k_event_wait(&button_events,
                               EVENT_LED_OFF,
                               false,
                               K_MSEC(2000));

    zassert_equal(events & EVENT_LED_OFF, EVENT_LED_OFF,
                  "Did not receive LED_OFF event");

    assert_led_off(&blinking_led, "blinking LED");
}

ZTEST(events_tests, test_led_toggles_events)
{
    start_main(1000);

    uint32_t events = k_event_wait(&button_events,
                               EVENT_LED_OFF,
                               false,
                               K_MSEC(2000));

    zassert_equal(events & EVENT_LED_OFF, EVENT_LED_OFF,
                  "Did not receive LED_OFF event");

    events = k_event_wait(&button_events,
                               EVENT_LED_ON,
                               false,
                               K_MSEC(2000));

    zassert_equal(events & EVENT_LED_ON, EVENT_LED_ON,
                  "Did not receive LED_ON event");
}

ZTEST(events_tests, test_button_event_generated)
{
    start_main(200);

    simulate_button_click(&button);

    uint32_t events = k_event_wait(&button_events,
                               EVENT_BUTTON,
                               false,
                               K_MSEC(1000));

    zassert_equal(events & EVENT_BUTTON, EVENT_BUTTON,
                  "Button event not generated");
}

ZTEST_SUITE(events_tests, NULL, NULL, before, after, NULL);