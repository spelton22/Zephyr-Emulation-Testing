/*
 * main.c — Example ADC Phase 1 application
 *
 * Behaviour under test
 * ────────────────────
 *  INIT   → configure all GPIOs, ADC channel, attach button callbacks
 *         → transition to IDLE
 *
 *  IDLE   → wait for read_button / sleep_button / reset_button
 *
 *  READING → (entered on read_button)
 *            • read single-ended AIN0
 *            • convert raw counts to millivolts
 *            • map millivolts to blink frequency:
 *                freq = (mv / MAX_V_MV) * (MAX_HZ - MIN_HZ) + MIN_HZ
 *            • compute on/off times for 10 % duty cycle
 *            • if mv out of range → ERROR, else → BLINKING
 *
 *  BLINKING → blink blinker_led at the computed frequency
 *             for exactly BLINKING_TIME_MS milliseconds, then → IDLE
 *
 *  SLEEP  → pause; resume on sleep_button or reset_button
 *
 *  RESET  → return cleanly to IDLE
 *
 *  ERROR  → turn on error_led; wait for reset_button
 *
 * Instrumentation macros from zephyr_adc_emul_lib.h are the only
 * dependency added beyond the normal Zephyr drivers.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/smf.h>
#include <zephyr/logging/log.h>

#include "zephyr_adc_emul_lib.h"

LOG_MODULE_REGISTER(adc_app, LOG_LEVEL_DBG);

/* ─────────────────────────────────────────────────────────────────── */
/*  Configuration macros                                               */
/* ─────────────────────────────────────────────────────────────────── */
#define MS_PER_HZ           1000
#define HB_ON_TIME          250          /* heartbeat on-time (ms)     */
#define MAX_FREQ_HZ         5
#define MIN_FREQ_HZ         1
#define MIN_V_MV            0
#define MAX_V_MV            2990
#define BLINKING_TIME_MS    5000

/** Duty cycle for blinker_led: 10 % on, 90 % off */
#define BLINK_DUTY_PCT      10

/* Convenience macro to initialise an adc_dt_spec from an alias */
#define ADC_DT_SPEC_GET_BY_ALIAS(alias)                                 \
{                                                                       \
    .dev        = DEVICE_DT_GET(DT_PARENT(DT_ALIAS(alias))),           \
    .channel_id = DT_REG_ADDR(DT_ALIAS(alias)),                        \
    ADC_CHANNEL_CFG_FROM_DT_NODE(DT_ALIAS(alias))                      \
}

int timer_error = 0;

/* ─────────────────────────────────────────────────────────────────── */
/*  Hardware structs  (GPIOs defined here; declared extern in the .h)  */
/* ─────────────────────────────────────────────────────────────────── */
const struct gpio_dt_spec read_button  = GPIO_DT_SPEC_GET(DT_ALIAS(readbutton),  gpios);
const struct gpio_dt_spec sleep_button = GPIO_DT_SPEC_GET(DT_ALIAS(sleepbutton), gpios);
const struct gpio_dt_spec reset_button = GPIO_DT_SPEC_GET(DT_ALIAS(resetbutton), gpios);

const struct gpio_dt_spec heartbeat_led = GPIO_DT_SPEC_GET(DT_ALIAS(heartbeat), gpios);
const struct gpio_dt_spec blinker_led   = GPIO_DT_SPEC_GET(DT_ALIAS(blinker),   gpios);
const struct gpio_dt_spec error_led     = GPIO_DT_SPEC_GET(DT_ALIAS(error),     gpios);

static const struct adc_dt_spec adc_vadc = ADC_DT_SPEC_GET_BY_ALIAS(vadc);

/* ─────────────────────────────────────────────────────────────────── */
/*  Internal event bits (local to this application)                   */
/* ─────────────────────────────────────────────────────────────────── */
#define READ_EVENT           BIT(0)
#define SLEEP_EVENT          BIT(1)
#define TIMER_COMPLETE_EVENT BIT(2)
#define RESET_EVENT          BIT(3)

K_EVENT_DEFINE(button_events);

/* ─────────────────────────────────────────────────────────────────── */
/*  GPIO callback structs                                              */
/* ─────────────────────────────────────────────────────────────────── */
static struct gpio_callback read_button_cb;
static struct gpio_callback sleep_button_cb;
static struct gpio_callback reset_button_cb;

/* ─────────────────────────────────────────────────────────────────── */
/*  ADC sequence buffer                                                */
/* ─────────────────────────────────────────────────────────────────── */
static int16_t          adc_buf;
static struct adc_sequence adc_seq = {
    .buffer      = &adc_buf,
    .buffer_size = sizeof(adc_buf),
};

/* ─────────────────────────────────────────────────────────────────── */
/*  State-machine context                                              */
/* ─────────────────────────────────────────────────────────────────── */
struct s_object {
    struct smf_ctx ctx;

    int32_t millivolts;
    float   freq;
    int     ontime_ms;
    int     offtime_ms;

    int64_t blink_start_ms;
    int64_t blink_end_ms;
};

static struct s_object s_ctx;

/* ─────────────────────────────────────────────────────────────────── */
/*  Timers                                                             */
/* ─────────────────────────────────────────────────────────────────── */
static void blinking_timer_cb(struct k_timer *t);
static void led_on_timer_cb(struct k_timer *t);
static void led_on_stop_cb(struct k_timer *t);
static void led_off_timer_cb(struct k_timer *t);

K_TIMER_DEFINE(blinking_timer, blinking_timer_cb, NULL);
K_TIMER_DEFINE(led_on_timer,   led_on_timer_cb,   led_on_stop_cb);
K_TIMER_DEFINE(led_off_timer,  led_off_timer_cb,  NULL);

/* ─────────────────────────────────────────────────────────────────── */
/*  State forward declarations                                         */
/* ─────────────────────────────────────────────────────────────────── */
enum state { INIT, RESET_ST, IDLE, SLEEP, READING, BLINKING, ERROR_ST };

static void state_init_run(void *o);
static void state_reset_run(void *o);

static void state_idle_entry(void *o);
static void state_idle_run(void *o);
static void state_idle_exit(void *o);

static void state_sleep_run(void *o);

static void state_reading_entry(void *o);
static void state_reading_run(void *o);
static void state_reading_exit(void *o);

static void state_blinking_entry(void *o);
static void state_blinking_run(void *o);
static void state_blinking_exit(void *o);

static void state_error_entry(void *o);
static void state_error_run(void *o);
static void state_error_exit(void *o);

static const struct smf_state states[] = {
    [INIT]      = SMF_CREATE_STATE(NULL,               state_init_run,      NULL,                  NULL, NULL),
    [RESET_ST]  = SMF_CREATE_STATE(NULL,               state_reset_run,     NULL,                  NULL, NULL),
    [IDLE]      = SMF_CREATE_STATE(state_idle_entry,   state_idle_run,      state_idle_exit,       NULL, NULL),
    [SLEEP]     = SMF_CREATE_STATE(NULL,               state_sleep_run,     NULL,                  NULL, NULL),
    [READING]   = SMF_CREATE_STATE(state_reading_entry,state_reading_run,   state_reading_exit,    NULL, NULL),
    [BLINKING]  = SMF_CREATE_STATE(state_blinking_entry,state_blinking_run, state_blinking_exit,   NULL, NULL),
    [ERROR_ST]  = SMF_CREATE_STATE(state_error_entry,  state_error_run,     state_error_exit,      NULL, NULL),
};

/* ─────────────────────────────────────────────────────────────────── */
/*  Heartbeat thread                                                   */
/* ─────────────────────────────────────────────────────────────────── */
static void heartbeat_thread_fn(void *, void *, void *)
{
    while (1) {
        k_msleep(HB_ON_TIME);
        gpio_pin_toggle_dt(&heartbeat_led);
        k_msleep(MS_PER_HZ - HB_ON_TIME);
        gpio_pin_toggle_dt(&heartbeat_led);
    }
}

K_THREAD_DEFINE(heartbeat_tid, 1024, heartbeat_thread_fn, NULL, NULL, NULL, 5, 0, 0);

/* ─────────────────────────────────────────────────────────────────── */
/*  GPIO callbacks                                                     */
/* ─────────────────────────────────────────────────────────────────── */
static void read_button_callback(const struct device *dev,
                                  struct gpio_callback *cb,
                                  uint32_t pins)
{
    ADC_READ_TRIGGERED();
    k_event_post(&button_events, READ_EVENT);
}

static void sleep_button_callback(const struct device *dev,
                                   struct gpio_callback *cb,
                                   uint32_t pins)
{
    k_event_post(&button_events, SLEEP_EVENT);
}

static void reset_button_callback(const struct device *dev,
                                   struct gpio_callback *cb,
                                   uint32_t pins)
{
    k_event_post(&button_events, RESET_EVENT);
}

/* ─────────────────────────────────────────────────────────────────── */
/*  Timer callbacks                                                    */
/* ─────────────────────────────────────────────────────────────────── */
static void blinking_timer_cb(struct k_timer *t)
{
    s_ctx.blink_end_ms = k_uptime_get();
    ADC_BLINK_COMPLETE();
    k_event_post(&button_events, TIMER_COMPLETE_EVENT);
}

static void led_on_timer_cb(struct k_timer *t)
{
    gpio_pin_set_dt(&blinker_led, 0);
    k_timer_start(&led_off_timer, K_MSEC(s_ctx.offtime_ms), K_NO_WAIT);
}

static void led_on_stop_cb(struct k_timer *t)
{
    /* Ensure LED is off when the on-timer is stopped externally */
    gpio_pin_set_dt(&blinker_led, 0);
}

static void led_off_timer_cb(struct k_timer *t)
{
    gpio_pin_set_dt(&blinker_led, 1);
    k_timer_start(&led_on_timer, K_MSEC(s_ctx.ontime_ms), K_NO_WAIT);
}

/* ─────────────────────────────────────────────────────────────────── */
/*  State implementations                                              */
/* ─────────────────────────────────────────────────────────────────── */
static void state_init_run(void *o)
{
    int err = 0;
    
    /* CHECK INTERFACE READY */
    if (!device_is_ready(read_button.port)) {
        LOG_ERR("gpio0 interface not ready.");
        smf_set_terminate(SMF_CTX(&s_ctx), -1);
    }
    if (!device_is_ready(adc_vadc.dev)) {
        LOG_ERR("ADC controller device(s) not ready");
        smf_set_terminate(SMF_CTX(&s_ctx), -1);
    }
    
    /* CONFIGURE BUTTON GPIO PINS */
    err = gpio_pin_configure_dt(&read_button, GPIO_INPUT);
    if (err < 0) {
        LOG_ERR("Cannot configure sleep button.");
        smf_set_terminate(SMF_CTX(&s_ctx), err);
    }
    err = gpio_pin_configure_dt(&sleep_button, GPIO_INPUT);
    if (err < 0) {
        LOG_ERR("Cannot configure sleep button.");
        smf_set_terminate(SMF_CTX(&s_ctx), err);
    }
    err = gpio_pin_configure_dt(&reset_button, GPIO_INPUT);
    if (err < 0) {
        LOG_ERR("Cannot configure reset button.");
        smf_set_terminate(SMF_CTX(&s_ctx), err);
    }
    
    /* CONFIGURE BUTTON CALLBACKS */
    // read
    err = gpio_pin_interrupt_configure_dt(&read_button, GPIO_INT_EDGE_TO_ACTIVE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw0.");
        smf_set_terminate(SMF_CTX(&s_ctx), err);
    }
    gpio_init_callback(&read_button_cb, read_button_callback, BIT(read_button.pin));
    err = gpio_add_callback_dt(&read_button, &read_button_cb);
    if (err < 0) {
        LOG_ERR("Cannot add callback to sw0.");
        smf_set_terminate(SMF_CTX(&s_ctx), err);
    }
    // sleep
    err = gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_EDGE_TO_ACTIVE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw1.");
        smf_set_terminate(SMF_CTX(&s_ctx), err);
    }
    gpio_init_callback(&sleep_button_cb, sleep_button_callback, BIT(sleep_button.pin));
    err = gpio_add_callback_dt(&sleep_button, &sleep_button_cb);
    if (err < 0) {
        LOG_ERR("Cannot add callback to sw1.");
        smf_set_terminate(SMF_CTX(&s_ctx), err);
    }
    // reset
    err = gpio_pin_interrupt_configure_dt(&reset_button, GPIO_INT_EDGE_TO_ACTIVE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw3.");
        smf_set_terminate(SMF_CTX(&s_ctx), err);
    }
    gpio_init_callback(&reset_button_cb, reset_button_callback, BIT(reset_button.pin));
    err = gpio_add_callback_dt(&reset_button, &reset_button_cb);
    if (err < 0) {
        LOG_ERR("Cannot add callback to sw3.");
        smf_set_terminate(SMF_CTX(&s_ctx), err);
    }
    
    /* CONFIGURE LEDs */
    err = gpio_pin_configure_dt(&heartbeat_led, GPIO_OUTPUT_ACTIVE);
    if (err < 0) {
        LOG_ERR("Cannot configure heartbeat LED.");
        smf_set_terminate(SMF_CTX(&s_ctx), err);
    }
    err = gpio_pin_configure_dt(&blinker_led, GPIO_OUTPUT_INACTIVE);
    if (err < 0) {
        LOG_ERR("Cannot configure iv_pump LED.");
        smf_set_terminate(SMF_CTX(&s_ctx), err);
    }
    err = gpio_pin_configure_dt(&error_led, GPIO_OUTPUT_INACTIVE);
    if (err < 0) {
        LOG_ERR("Cannot configure error LED.");
        smf_set_terminate(SMF_CTX(&s_ctx), err);
    }
    
    /* CONFIGURE ADC CHANNEL */
    err = adc_channel_setup_dt(&adc_vadc);
    if (err < 0) {
        LOG_ERR("Could not setup ADC channel (%d)", err);
        smf_set_terminate(SMF_CTX(&s_ctx), err);
    }
    
    smf_set_state(SMF_CTX(&s_ctx), &states[IDLE]);
}

static void state_reset_run(void *o)
{
    smf_set_state(SMF_CTX(&s_ctx), &states[IDLE]);
}

static void state_idle_entry(void *o)
{
    gpio_pin_interrupt_configure_dt(&read_button, GPIO_INT_EDGE_TO_ACTIVE);
}

static void state_idle_run(void *o)
{
    uint32_t events = k_event_wait(&button_events, READ_EVENT | SLEEP_EVENT | RESET_EVENT, true, K_FOREVER);
    if (events & READ_EVENT) {
        LOG_INF("Read button pressed");
        // ADC_READ_TRIGGERED();
        smf_set_state(SMF_CTX(&s_ctx), &states[READING]);
    }
    if (events & SLEEP_EVENT) {
        LOG_INF("Sleep button pressed");
        smf_set_state(SMF_CTX(&s_ctx), &states[SLEEP]);
    }
    if (events & RESET_EVENT) {
        LOG_INF("Reset button pressed");
        smf_set_state(SMF_CTX(&s_ctx), &states[RESET_ST]);
    }
}

static void state_idle_exit(void *o)
{
    gpio_pin_interrupt_configure_dt(&read_button, GPIO_INT_DISABLE);
}

static void state_sleep_run(void *o)
{
    uint32_t events = k_event_wait(&button_events, SLEEP_EVENT | RESET_EVENT, true, K_FOREVER);
    if (events & SLEEP_EVENT) {
        LOG_INF("Sleep button pressed");
        smf_set_state(SMF_CTX(&s_ctx), &states[IDLE]);
    }
    if (events & RESET_EVENT) {
        LOG_INF("Reset button pressed");
        smf_set_state(SMF_CTX(&s_ctx), &states[RESET_ST]);
    }
}

static void state_reading_entry(void *o)
{
    /* Disable sleep/reset while reading to avoid race conditions */
    gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_DISABLE);
    gpio_pin_interrupt_configure_dt(&reset_button, GPIO_INT_DISABLE);

    (void)adc_sequence_init_dt(&adc_vadc, &adc_seq);
}

static void state_reading_run(void *o)
{
    int ret = adc_read(adc_vadc.dev, &adc_seq);
    if (ret < 0) {
        LOG_ERR("Could not read (%d)", ret);
        smf_set_state(SMF_CTX(&s_ctx), &states[ERROR_ST]);
        return;
    } else {
        LOG_DBG("Raw ADC Buffer: %d", adc_buf);
    }

    int32_t val_mv;
    val_mv = adc_buf;
    ret = adc_raw_to_millivolts_dt(&adc_vadc, &val_mv);
    if (ret < 0) {
        LOG_ERR("Buffer cannot be converted to mV; returning raw buffer value.");
    } else {
        LOG_INF("ADC Value (mV): %d", val_mv);
        s_ctx.millivolts = val_mv;
    }

    /* Linear frequency map: 0 mV → MIN_FREQ_HZ, MAX_V_MV → MAX_FREQ_HZ */
    float freq = ((float)val_mv * (MAX_FREQ_HZ - MIN_FREQ_HZ)) / (float)MAX_V_MV + MIN_FREQ_HZ;
    s_ctx.freq = freq;
    LOG_INF("Mapped freq: %.2f Hz", (double)freq);

    /* 10 % duty cycle */
    float period_ms  = (float)MS_PER_HZ / freq;
    s_ctx.ontime_ms  = (int)(period_ms * (BLINK_DUTY_PCT / 100.0f));
    s_ctx.offtime_ms = (int)(period_ms - s_ctx.ontime_ms);

    ADC_READ_COMPLETE(val_mv, freq);
    // k_yield(); /* give test thread a chance to observe the event */

    if (val_mv < MIN_V_MV || val_mv > MAX_V_MV) {
        smf_set_state(SMF_CTX(&s_ctx), &states[ERROR_ST]);
    } else {
        smf_set_state(SMF_CTX(&s_ctx), &states[BLINKING]);
    }
}

static void state_reading_exit(void *o)
{
    gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_pin_interrupt_configure_dt(&reset_button, GPIO_INT_EDGE_TO_ACTIVE);
}

static void state_blinking_entry(void *o)
{
    gpio_pin_interrupt_configure_dt(&read_button, GPIO_INT_DISABLE);

    gpio_pin_set_dt(&blinker_led, 1);

    k_timer_start(&led_on_timer,
                  K_MSEC(s_ctx.ontime_ms), K_NO_WAIT);
    k_timer_start(&blinking_timer,
                  K_MSEC(BLINKING_TIME_MS), K_NO_WAIT);

    s_ctx.blink_start_ms = k_uptime_get();
}

static void state_blinking_run(void *o)
{
    uint32_t ev = k_event_wait(&button_events,
                               TIMER_COMPLETE_EVENT | SLEEP_EVENT | RESET_EVENT,
                               true, K_FOREVER);
    if (ev & TIMER_COMPLETE_EVENT) {
        smf_set_state(SMF_CTX(&s_ctx), &states[IDLE]);
    } else if (ev & SLEEP_EVENT) {
        smf_set_state(SMF_CTX(&s_ctx), &states[SLEEP]);
    } else if (ev & RESET_EVENT) {
        smf_set_state(SMF_CTX(&s_ctx), &states[RESET_ST]);
    }
}

static void state_blinking_exit(void *o)
{
    k_timer_stop(&blinking_timer);
    k_timer_stop(&led_on_timer);
    k_timer_stop(&led_off_timer);
    gpio_pin_set_dt(&blinker_led, 0);

    gpio_pin_interrupt_configure_dt(&read_button, GPIO_INT_EDGE_TO_ACTIVE);

    LOG_INF("Blink window: %lld ms",
            s_ctx.blink_end_ms - s_ctx.blink_start_ms);
}

static void state_error_entry(void *o)
{
    gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_DISABLE);
    gpio_pin_set_dt(&error_led, 1);
}

static void state_error_run(void *o)
{
    uint32_t ev = k_event_wait(&button_events,
                               RESET_EVENT, true, K_FOREVER);
    if (ev & RESET_EVENT) {
        smf_set_state(SMF_CTX(&s_ctx), &states[RESET_ST]);
    }
}

static void state_error_exit(void *o)
{
    gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_pin_set_dt(&error_led, 0);
}

int main(void)
{
    smf_set_initial(SMF_CTX(&s_ctx), &states[INIT]);

    while (1) {
        if (timer_error < 0) {
            return timer_error;
        }
        
        int err = smf_run_state(SMF_CTX(&s_ctx));
        if (err) {
            /* handle return code and terminate state machine */
            smf_set_terminate(SMF_CTX(&s_ctx), err);
            break;
        }
    }
    
    return 0;
}


