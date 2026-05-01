#ifndef ZEPHYR_ADC_EMUL_LIB_H
#define ZEPHYR_ADC_EMUL_LIB_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/adc/adc_emul.h>
#include <zephyr/ztest.h>

/* ─────────────────────────────────────────────────────────────────── */
/*  Event-bit definitions                                              */
/*  Post these from your application code at key transitions so the    */
/*  test suite can observe them without polling.                        */
/* ─────────────────────────────────────────────────────────────────── */

#define ADC_READ_TRIGGERED_NOTICE   BIT(7)
#define ADC_READ_COMPLETE_NOTICE    BIT(8)
#define ADC_BLINK_DONE_NOTICE       BIT(9)

/* Kernel event object shared between application and test code. */
extern struct k_event program_test_events;

/* ─────────────────────────────────────────────────────────────────── */
/*  Shared result variables                                            */
/*  Your application code writes these inside the macros below.        */
/* ─────────────────────────────────────────────────────────────────── */

/** Millivolt reading from the last ADC conversion. */
extern int   student_adc_mv;

/** Blink frequency (Hz) mapped from student_adc_mv. */
extern float student_mapped_freq;

/* ─────────────────────────────────────────────────────────────────── */
/*  Instrumentation macros                                             */
/*  Drop these into your application state-machine at the points       */
/*  described.  They write the shared variables and post the events    */
/*  the test suite listens for.                                        */
/* ─────────────────────────────────────────────────────────────────── */

/**
 * ADC_READ_TRIGGERED()
 *   Place at the very start wherever you begin an ADC acquisition).
 */
#define ADC_READ_TRIGGERED()                                            \
    k_event_post(&program_test_events, ADC_READ_TRIGGERED_NOTICE)

/**
 * ADC_READ_COMPLETE(mv, freq)
 *   Place after computing millivolts and the mapped frequency
 *
 *   @param mv   int32_t millivolt value
 *   @param freq float   mapped blink frequency in Hz
 */
#define ADC_READ_COMPLETE(mv, freq)                 \
    do {                                            \
        student_adc_mv     = (mv);                  \
        student_mapped_freq = (freq);               \
        k_event_post(&program_test_events,          \
                     ADC_READ_COMPLETE_NOTICE);      \
    } while (0)

/**
 * ADC_BLINK_COMPLETE()
 *   Place wherever your code stops the blinker LED.
 */
#define ADC_BLINK_COMPLETE()                                            \
    k_event_post(&program_test_events, ADC_BLINK_DONE_NOTICE)

/* ─────────────────────────────────────────────────────────────────── */
/*  ADC channel ID                                                     */
/* ─────────────────────────────────────────────────────────────────── */

/** Channel index for the single-ended ADC input (matches reg = <0> in overlay). */
#define AIN0_CHANNEL_ID  0

/** Device-tree label for the ADC emulator. */
#define ADC_EMUL_NODE    DT_NODELABEL(adc_emul)

/* ─────────────────────────────────────────────────────────────────── */
/*  Application GPIO symbols                                           */
/*  These must be defined (with GPIO_DT_SPEC_GET) in your main.c.     */
/* ─────────────────────────────────────────────────────────────────── */
extern const struct gpio_dt_spec read_button;
extern const struct gpio_dt_spec sleep_button;
extern const struct gpio_dt_spec reset_button;

extern const struct gpio_dt_spec heartbeat_led;
extern const struct gpio_dt_spec blinker_led;
extern const struct gpio_dt_spec error_led;

/* ─────────────────────────────────────────────────────────────────── */
/*  Student-main entry point                                           */
/*  Renamed via -Dmain=student_main in CMakeLists.txt.                */
/* ─────────────────────────────────────────────────────────────────── */
extern int student_main(void);

/* ─────────────────────────────────────────────────────────────────── */
/*  Test-thread configuration                                          */
/* ─────────────────────────────────────────────────────────────────── */
#define STUDENT_MAIN_STACK_SIZE  4096
#define STUDENT_MAIN_PRIORITY    5

/* ─────────────────────────────────────────────────────────────────── */
/*  Public test-helper API                                             */
/*                                                                     */
/*  The implementations live in zephyr_adc_emul_lib.c.  Include that  */
/*  file in your CMakeLists alongside your test .c file.              */
/* ─────────────────────────────────────────────────────────────────── */

static void before(void *);
static void after(void *);

static void student_main_entry(void *, void *, void *);

/**
 * stop_main()
 *   Call from your ztest after() fixture.
 *   Aborts the student_main thread cleanly.
 */
static void stop_main(void);

/**
 * start_main()
 *   Call once from your ztest before() fixture.
 *   - Resets buttons to logic-0.
 *   - Grabs the ADC emulator device handle.
 *   - Starts student_main in its own thread and waits settle_ms for
 *     the state machine to reach IDLE.
 *   - Clears all ADC event bits.
 *
 *   @param settle_ms  Milliseconds to wait after spawning the thread
 *                     before returning (500 ms is usually sufficient).
 */
static void start_main(int settle_ms);

/**
 * wait_for_event()
 *   Poll program_test_events until the requested bit(s) appear or
 *   timeout_ms elapses.
 *
 *   @param mask        OR of ADC_*_NOTICE bits to wait for.
 *   @param timeout_ms  Maximum wait time in milliseconds.
 *   @return true if the event fired, false on timeout.
 */
static bool wait_for_event(uint32_t mask, int timeout_ms);

static void led_edge_duty_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
static void led_edge_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);

/**
 * assert_led_blink_freq()
 *   Attach a GPIO interrupt callback to led, count rising+falling edges
 *   for window_ms, and assert the measured frequency is within
 *   tolerance_hz of expected_hz.
 *
 *   @param led          LED to measure.
 *   @param window_ms    Observation window in ms.
 *   @param expected_hz  Target frequency in Hz.
 *   @param tolerance_hz Allowed error in Hz.
 *   @param led_name     String used in assertion failure messages.
 */
static void assert_led_blink_freq(const struct gpio_dt_spec *led, int window_ms, int expected_hz, int tolerance_hz, const char *led_name);

/**
 * assert_blink_duty_pct()
 *   Measure the on-time duty cycle of blinker_led over window_ms and
 *   assert it is within tolerance_pct of expected_pct.
 *
 *   @param window_ms    Observation window in ms.
 *   @param expected_pct Expected duty cycle (0–100).
 *   @param tolerance_pct Allowed error in percentage points.
 */
static void assert_blink_ontime_pct(int window_ms, int expected_duty, int tolerance);

/**
 * assert_blink_duration_ms()
 *   Wait for ADC_BLINK_DONE_NOTICE, then assert the elapsed time
 *   from the first blinker edge to the notice is within tolerance_ms
 *   of expected_ms.
 *
 *   @param expected_ms   Expected blink window length in ms (e.g. 5000).
 *   @param tolerance_ms  Allowed error in ms (e.g. 300).
 */
static void assert_blink_total_duration_ms(int expected_ms, int tolerance_ms);

/**
 * simulate_button_click()
 *   Drive a GPIO-emulated button: set → 5 ms → clear.
 *   This fires the student's gpio interrupt callback.
 *
 *   @param button  Pointer to a gpio_dt_spec (e.g. &read_button).
 */
static void simulate_button_click(const struct gpio_dt_spec *button);

/**
 * assert_led_off()  / assert_led_on()
 *   Read the emulated GPIO output level and assert it is 1 (on) or 0 (off).
 */
static void assert_led_off(const struct gpio_dt_spec *led, const char *led_name);
static void assert_led_on(const struct gpio_dt_spec *led, const char *led_name);

/**
 * set_ain0_mv()
 *   Inject a constant millivolt level on AIN0 for the next read.
 *   Uses adc_emul_const_value_set internally.
 *
 *   @param millivolts  Value in mV; clamped by the emulator to [0, ref_mv].
 */
static void set_ain0_mv(const struct device *dev, int millivolts);

#endif /* ZEPHYR_ADC_EMUL_LIB_H */