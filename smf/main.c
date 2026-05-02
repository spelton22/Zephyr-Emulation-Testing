#include <zephyr/kernel.h>
#include <zephyr/smf.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

/* ---------------- GPIO ---------------- */

const struct gpio_dt_spec led_left  = GPIO_DT_SPEC_GET(DT_ALIAS(ledtest0), gpios);
const struct gpio_dt_spec led_right = GPIO_DT_SPEC_GET(DT_ALIAS(ledtest1), gpios);

const struct gpio_dt_spec btn_left  = GPIO_DT_SPEC_GET(DT_ALIAS(buttontest0), gpios);
const struct gpio_dt_spec btn_right = GPIO_DT_SPEC_GET(DT_ALIAS(buttontest1), gpios);
const struct gpio_dt_spec btn_stop  = GPIO_DT_SPEC_GET(DT_ALIAS(buttontest2), gpios);

/* ---------------- Unified event bus ---------------- */

struct k_event program_events;

/* Input events */
#define EVT_SELECT_LEFT   BIT(0)
#define EVT_SELECT_RIGHT  BIT(1)
#define EVT_STOP          BIT(2)

/* State entry events */
#define EVT_ENTER_IDLE        BIT(8)
#define EVT_ENTER_BLINK_LEFT  BIT(9)
#define EVT_ENTER_BLINK_RIGHT BIT(10)

/* ---------------- SMF ---------------- */

enum state_ids {
    INIT,
    IDLE,
    BLINK_LEFT,
    BLINK_RIGHT,
};

struct app_ctx {
    struct smf_ctx ctx;
};

struct app_ctx s_ctx;

/* Forward declaration */
const struct smf_state states[];

/* ---------------- ISR callbacks ---------------- */

void btn_left_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    k_event_set(&program_events, EVT_SELECT_LEFT);
}

void btn_right_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    k_event_set(&program_events, EVT_SELECT_RIGHT);
}

void btn_stop_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    k_event_set(&program_events, EVT_STOP);
}

static struct gpio_callback cb_left, cb_right, cb_stop;

/* ---------------- STATE: INIT ---------------- */

void state_init_run(void *o)
{
    k_event_init(&program_events);

    gpio_pin_configure_dt(&led_left, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_right, GPIO_OUTPUT_INACTIVE);

    gpio_pin_configure_dt(&btn_left, GPIO_INPUT);
    gpio_pin_configure_dt(&btn_right, GPIO_INPUT);
    gpio_pin_configure_dt(&btn_stop, GPIO_INPUT);

    gpio_pin_interrupt_configure_dt(&btn_left, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_pin_interrupt_configure_dt(&btn_right, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_pin_interrupt_configure_dt(&btn_stop, GPIO_INT_EDGE_TO_ACTIVE);

    gpio_init_callback(&cb_left, btn_left_cb, BIT(btn_left.pin));
    gpio_init_callback(&cb_right, btn_right_cb, BIT(btn_right.pin));
    gpio_init_callback(&cb_stop, btn_stop_cb, BIT(btn_stop.pin));

    gpio_add_callback(btn_left.port, &cb_left);
    gpio_add_callback(btn_right.port, &cb_right);
    gpio_add_callback(btn_stop.port, &cb_stop);
    
    smf_set_state(SMF_CTX(&s_ctx.ctx), &states[IDLE]);
}

/* ---------------- STATE: IDLE ---------------- */

void state_idle_entry(void *o)
{
    ARG_UNUSED(o);
    k_event_set(&program_events, EVT_ENTER_IDLE);
    
    gpio_pin_set_dt(&led_left, 0);
    gpio_pin_set_dt(&led_right, 0);
}

void state_idle_run(void *o)
{
    uint32_t ev = k_event_wait(&program_events,
                               EVT_SELECT_LEFT | EVT_SELECT_RIGHT,
                               true,
                               K_FOREVER);

    if (ev & EVT_SELECT_LEFT) {
        smf_set_state(SMF_CTX(&s_ctx.ctx), &states[BLINK_LEFT]);
    } else if (ev & EVT_SELECT_RIGHT) {
        smf_set_state(SMF_CTX(&s_ctx.ctx), &states[BLINK_RIGHT]);
    }
}

/* ---------------- STATE: BLINK LEFT ---------------- */

void state_blink_left_entry(void *o)
{
    ARG_UNUSED(o);
    k_event_set(&program_events, EVT_ENTER_BLINK_LEFT);
    
    gpio_pin_set_dt(&led_right, 0);
}

void state_blink_left_run(void *o)
{
    static bool toggle;
    toggle = !toggle;

    gpio_pin_set_dt(&led_left, toggle);

    uint32_t ev = k_event_wait(&program_events,
                               EVT_SELECT_RIGHT | EVT_STOP,
                               true,
                               K_MSEC(500));

    if (ev & EVT_SELECT_RIGHT) {
        smf_set_state(SMF_CTX(&s_ctx.ctx), &states[BLINK_RIGHT]);
    } else if (ev & EVT_STOP) {
        smf_set_state(SMF_CTX(&s_ctx.ctx), &states[IDLE]);
    }
}

/* ---------------- STATE: BLINK RIGHT ---------------- */

void state_blink_right_entry(void *o)
{
    ARG_UNUSED(o);
    k_event_set(&program_events, EVT_ENTER_BLINK_RIGHT);
    
    gpio_pin_set_dt(&led_left, 0);
}

void state_blink_right_run(void *o)
{
    static bool toggle;
    toggle = !toggle;

    gpio_pin_set_dt(&led_right, toggle);

    uint32_t ev = k_event_wait(&program_events,
                               EVT_SELECT_LEFT | EVT_STOP,
                               true,
                               K_MSEC(500));

    if (ev & EVT_SELECT_LEFT) {
        smf_set_state(SMF_CTX(&s_ctx.ctx), &states[BLINK_LEFT]);
    } else if (ev & EVT_STOP) {
        smf_set_state(SMF_CTX(&s_ctx.ctx), &states[IDLE]);
    }
}

/* ---------------- State table ---------------- */

const struct smf_state states[] = {
    [INIT]        = SMF_CREATE_STATE(NULL, state_init_run, NULL, NULL, NULL),
    [IDLE]        = SMF_CREATE_STATE(state_idle_entry, state_idle_run, NULL, NULL, NULL),
    [BLINK_LEFT]  = SMF_CREATE_STATE(state_blink_left_entry, state_blink_left_run, NULL, NULL, NULL),
    [BLINK_RIGHT] = SMF_CREATE_STATE(state_blink_right_entry, state_blink_right_run, NULL, NULL, NULL),
};

/* ---------------- MAIN ---------------- */

int main(void)
{
    LOG_INF("SMF LED mode controller starting");

    smf_set_initial(SMF_CTX(&s_ctx.ctx), &states[INIT]);

    while (1) {
        smf_run_state(SMF_CTX(&s_ctx.ctx));
    }
}