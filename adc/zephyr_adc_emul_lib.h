#ifndef ZEPHYR_ADC_EMUL_LIB_H
#define ZEPHYR_ADC_EMUL_LIB_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/adc/adc_emul.h>
#include <zephyr/ztest.h>

#define BLINKING_TIME_MS    5000

/* ─────────────────────────────────────────────────────────────────── */
/*  Event-bit definitions                                              */
/*  Post these from your application code at key transitions so the    */
/*  test suite can observe them without polling.                        */
/* ─────────────────────────────────────────────────────────────────── */

#define ADC_READ_TRIGGERED_NOTICE   BIT(7)
#define ADC_READ_COMPLETE_NOTICE    BIT(8)
#define ADC_BLINK_DONE_NOTICE       BIT(9)
#define RESET_TEST_NOTICE   BIT(10)
#define STUDENT_MAIN_STACK_SIZE  4096
#define STUDENT_MAIN_PRIORITY    5

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

extern struct k_thread student_main_thread;
extern k_tid_t         student_main_tid;
extern volatile bool   main_running;
K_THREAD_STACK_DECLARE(student_main_stack, STUDENT_MAIN_STACK_SIZE);
extern const struct device *adc_emul_dev;

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
/*  Public test-helper API                                             */
/*                                                                     */
/*  The implementations live in zephyr_adc_emul_lib.c.  Include that  */
/*  file in your CMakeLists alongside your test .c file.              */
/* ─────────────────────────────────────────────────────────────────── */

void before(void *);
void after(void *);
void student_main_entry(void *, void *, void *);
void stop_main(void);
void start_main(int settle_ms);
bool wait_for_event(uint32_t mask, int timeout_ms);
void led_edge_duty_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void led_edge_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void assert_led_blink_freq(const struct gpio_dt_spec *led, int window_ms, int expected_hz, int tolerance_hz, const char *led_name);
void assert_blink_ontime_pct(int window_ms, int expected_duty, int tolerance);
void assert_blink_total_duration_ms(int expected_ms, int tolerance_ms);
void simulate_button_click(const struct gpio_dt_spec *button);
void assert_led_off(const struct gpio_dt_spec *led, const char *led_name);
void assert_led_on(const struct gpio_dt_spec *led, const char *led_name);
void set_ain0_mv(const struct device *dev, int millivolts);

#endif /* ZEPHYR_ADC_EMUL_LIB_H */