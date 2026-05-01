#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "smf_test.h"
#include "zephyr_smf_test_lib.h"

/* ------------------------------------------------------------ */
/*  IDLE state behavior                                         */
/* ------------------------------------------------------------ */

ZTEST(smf_tests, test_starts_in_idle_state)
{
    start_main(1000);

    uint32_t ev = k_event_wait(&program_events,
                               EVT_ENTER_IDLE,
                               false,
                               K_MSEC(1000));

    zassert_equal(ev & EVT_ENTER_IDLE, EVT_ENTER_IDLE,
                  "Did not enter IDLE state");

    assert_led_off(&led_left, "LED_LEFT");
    assert_led_off(&led_right, "LED_RIGHT");
}

/* ------------------------------------------------------------ */
/*  Button → BLINK_LEFT transition                              */
/* ------------------------------------------------------------ */

ZTEST(smf_tests, test_left_button_enters_blink_left)
{
    start_main(3000);

    simulate_button_click(&btn_left);

    uint32_t ev = k_event_wait(&program_events,
                               EVT_ENTER_BLINK_LEFT,
                               false,
                               K_MSEC(1000));

    zassert_equal(ev & EVT_ENTER_BLINK_LEFT, EVT_ENTER_BLINK_LEFT,
                  "Did not transition to BLINK_LEFT");

    k_msleep(250);

    /* Left LED should now be active */
    assert_led_blink_freq(&led_left,
                          4000,
                          1,
                          1,
                          "LED_LEFT");
}

/* ------------------------------------------------------------ */
/*  Button → BLINK_RIGHT transition                             */
/* ------------------------------------------------------------ */

ZTEST(smf_tests, test_right_button_enters_blink_right)
{
    start_main(3000);

    simulate_button_click(&btn_right);

    uint32_t ev = k_event_wait(&program_events,
                               EVT_ENTER_BLINK_RIGHT,
                               false,
                               K_MSEC(1000));

    zassert_equal(ev & EVT_ENTER_BLINK_RIGHT, EVT_ENTER_BLINK_RIGHT,
                  "Did not transition to BLINK_RIGHT");

    k_msleep(250);

    assert_led_blink_freq(&led_right,
                          4000,
                          1,
                          1,
                          "LED_RIGHT");
}

/* ------------------------------------------------------------ */
/*  STOP button returns to IDLE                                 */
/* ------------------------------------------------------------ */

ZTEST(smf_tests, test_stop_returns_to_idle_from_left)
{
    start_main(3000);

    /* go to BLINK_LEFT first */
    simulate_button_click(&btn_left);

    k_msleep(300);

    /* then stop */
    simulate_button_click(&btn_stop);

    uint32_t ev = k_event_wait(&program_events,
                               EVT_ENTER_IDLE,
                               false,
                               K_MSEC(1000));

    zassert_equal(ev & EVT_ENTER_IDLE, EVT_ENTER_IDLE,
                  "Did not return to IDLE");

    assert_led_off(&led_left, "LED_LEFT");
    assert_led_off(&led_right, "LED_RIGHT");
}

/* ------------------------------------------------------------ */
/*  LEFT ↔ RIGHT switching                                      */
/* ------------------------------------------------------------ */

ZTEST(smf_tests, test_switch_left_to_right)
{
    start_main(3000);

    simulate_button_click(&btn_left);
    k_msleep(500);
    
    assert_led_blink_freq(&led_left,
                          4000,
                          1,
                          1,
                          "LED_LEFT");

    simulate_button_click(&btn_right);

    uint32_t ev = k_event_wait(&program_events,
                               EVT_ENTER_BLINK_RIGHT,
                               false,
                               K_MSEC(1000));

    zassert_equal(ev & EVT_ENTER_BLINK_RIGHT, EVT_ENTER_BLINK_RIGHT,
                  "Did not switch to BLINK_RIGHT");

    assert_led_blink_freq(&led_right,
                          4000,
                          1,
                          1,
                          "LED_RIGHT");
}

/* ------------------------------------------------------------ */
/*  STOP always overrides state                                 */
/* ------------------------------------------------------------ */

ZTEST(smf_tests, test_stop_from_right_state)
{
    start_main(3000);

    simulate_button_click(&btn_right);
    k_msleep(250);

    simulate_button_click(&btn_stop);

    uint32_t ev = k_event_wait(&program_events,
                               EVT_ENTER_IDLE,
                               false,
                               K_MSEC(1000));

    zassert_equal(ev & EVT_ENTER_IDLE, EVT_ENTER_IDLE,
                  "STOP did not return to IDLE");

    assert_led_off(&led_left, "LED_LEFT");
    assert_led_off(&led_right, "LED_RIGHT");
}

/* ------------------------------------------------------------ */
/*  Suite registration                                          */
/* ------------------------------------------------------------ */

ZTEST_SUITE(smf_tests, NULL, NULL, NULL, NULL, NULL);






















ZTEST_SUITE(smf_tests, NULL, NULL, before, after, NULL);