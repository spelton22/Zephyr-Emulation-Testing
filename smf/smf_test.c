#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "smf_test.h"
#include "zephyr_smf_test_lib.h"

/* ------------------------------------------------------------ */
/*  IDLE state behavior                                         */
/* ------------------------------------------------------------ */

ZTEST(app_test, test_initial_state_idle)
{
    start_main(1000);

    assert_state(&states[IDLE]);

    assert_led_off(&led_left, "left");
    assert_led_off(&led_right, "right");
}

/* ------------------------------------------------------------ */
/*  Button → BLINK_LEFT transition                              */
/* ------------------------------------------------------------ */

ZTEST(app_test, test_left_button_enters_blink_left)
{
    start_main(1000);

    simulate_button_click(&btn_left);
    k_msleep(100);

    assert_state(&states[BLINK_LEFT]);

    /* Right LED must be off in this state */
    assert_led_off(&led_right, "right");
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

ZTEST(app_test, test_right_button_enters_blink_right)
{
    start_main(1000);

    simulate_button_click(&btn_right);
    k_msleep(100);

    assert_state(&states[BLINK_RIGHT]);

    /* Left LED must be off */
    assert_led_off(&led_left, "left");
    /* Right LED should now be active */
    assert_led_blink_freq(&led_right,
                          4000,
                          1,
                          1,
                          "LED_RIGHT");
}

/* ------------------------------------------------------------ */
/*  STOP button returns to IDLE                                 */
/* ------------------------------------------------------------ */

ZTEST(app_test, test_stop_returns_to_idle_from_left)
{
    start_main(1000);

    simulate_button_click(&btn_left);
    k_msleep(100);
    assert_state(&states[BLINK_LEFT]);

    simulate_button_click(&btn_stop);
    k_msleep(100);

    assert_state(&states[IDLE]);

    assert_led_off(&led_left, "left");
    assert_led_off(&led_right, "right");
}

ZTEST(app_test, test_stop_returns_to_idle_from_right)
{
    start_main(1000);

    simulate_button_click(&btn_right);
    k_msleep(100);
    assert_state(&states[BLINK_RIGHT]);

    simulate_button_click(&btn_stop);
    k_msleep(100);

    assert_state(&states[IDLE]);

    assert_led_off(&led_left, "left");
    assert_led_off(&led_right, "right");
}

/* ------------------------------------------------------------ */
/*  LEFT ↔ RIGHT switching                                      */
/* ------------------------------------------------------------ */

ZTEST(app_test, test_left_to_right_transition)
{
    start_main(1000);

    simulate_button_click(&btn_left);
    k_msleep(100);
    assert_state(&states[BLINK_LEFT]);

    simulate_button_click(&btn_right);
    k_msleep(100);

    assert_state(&states[BLINK_RIGHT]);

    /* Ensure left LED stops blinking */
    assert_led_off(&led_left, "left");
}

/* ------------------------------------------------------------ */
/*  Suite registration                                          */
/* ------------------------------------------------------------ */

ZTEST_SUITE(smf_tests, NULL, NULL, before, after, NULL);