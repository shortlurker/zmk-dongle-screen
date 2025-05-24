#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>

LOG_MODULE_REGISTER(als, 4);

#if CONFIG_DISPLAY_MIN_BRIGHTNESS > CONFIG_DISPLAY_MAX_BRIGHTNESS
#error "DISPLAY_MIN_BRIGHTNESS must be less than or equal to DISPLAY_MAX_BRIGHTNESS!"
#endif

#define BRIGHTNESS_STEP 2
#define BRIGHTNESS_DELAY_MS 10
#define DISPLAY_IDLE_TIMEOUT_MS (CONFIG_DISPLAY_IDLE_TIMEOUT_S * 1000)

static const struct device *pwm_leds_dev = DEVICE_DT_GET_ONE(pwm_leds);
#define DISP_BL DT_NODE_CHILD_IDX(DT_NODELABEL(disp_bl))

static int64_t last_activity = 0;
static uint8_t max_brightness = CONFIG_DISPLAY_MAX_BRIGHTNESS;
static uint8_t min_brightness = CONFIG_DISPLAY_MIN_BRIGHTNESS;
static uint8_t user_brightness = CONFIG_DISPLAY_MAX_BRIGHTNESS;

static uint8_t clamp_brightness(uint8_t value)
{
    if (value > max_brightness)
    {
        return max_brightness;
        LOG_WRN("CLAMPED: Display brightness %d would be over %d", value, max_brightness);
    }
    if (value < min_brightness)
    {
        LOG_WRN("CLAMPED: Display brightness %d would be under %d", value, min_brightness);
        return min_brightness;
    }
    return value;
}
static void apply_brightness(uint8_t value)
{
    led_set_brightness(pwm_leds_dev, DISP_BL, value);
    LOG_INF("Display brightness set to %d", value);
}

void set_display_brightness(uint8_t value)
{
    user_brightness = clamp_brightness(value);
    apply_brightness(user_brightness);
}

#if CONFIG_DISPLAY_IDLE_TIMEOUT_S > 0 || CONFIG_DISPLAY_BRIGHTNESS_KEYBOARD_CONTROL
// --- Brightness logic ---
static bool display_on = true;

static void fade_to_brightness(uint8_t from, uint8_t to)
{
    if (from == to)
    {
        apply_brightness(to);
        return;
    }
    if (from < to)
    {
        for (uint8_t b = from; b < to; b += BRIGHTNESS_STEP)
        {
            apply_brightness(b);
            k_msleep(BRIGHTNESS_DELAY_MS);
        }
    }
    else
    {
        for (int b = from; b > to; b -= BRIGHTNESS_STEP)
        {
            apply_brightness(b);
            k_msleep(BRIGHTNESS_DELAY_MS);
        }
    }
    apply_brightness(to);
}

// --- Display on/off ---

static void display_set_on(bool on)
{
    if (on && !display_on)
    {
        fade_to_brightness(min_brightness, user_brightness);
        display_on = true;
        LOG_INF("Display on (smooth)");
    }
    else if (!on && display_on)
    {
        fade_to_brightness(user_brightness, min_brightness);
        display_on = false;
        LOG_INF("Display off (smooth)");
    }
}

#endif

// --- Idle thread ---

#if CONFIG_DISPLAY_IDLE_TIMEOUT_S > 0

void display_idle_thread(void)
{
    while (1)
    {
        if (display_on)
        {
            int64_t now = k_uptime_get();
            int64_t elapsed = now - last_activity;
            int64_t remaining = DISPLAY_IDLE_TIMEOUT_MS - elapsed;

            if (remaining <= 0)
            {
                display_set_on(false);
                // After turning off, sleep until next activity (key event will wake display)
                k_sleep(K_FOREVER);
            }
            else
            {
                // Sleep exactly as long as needed until timeout or next key event
                k_sleep(K_MSEC(remaining));
            }
        }
        else
        {
            // If display is off, sleep forever (will be interrupted by key event)
            k_sleep(K_FOREVER);
        }
    }
}

K_THREAD_DEFINE(display_idle_tid, 512, display_idle_thread, NULL, NULL, NULL, 7, 0, 0);

#endif // CONFIG_DISPLAY_IDLE_TIMEOUT_S > 0

// --- Brightness control via keyboard ---

#if CONFIG_DISPLAY_BRIGHTNESS_KEYBOARD_CONTROL

static void increase_brightness(void)
{
    if (user_brightness < max_brightness)
    {
        set_display_brightness(user_brightness + CONFIG_DISPLAY_BRIGHTNESS_STEP);
    }
}

static void decrease_brightness(void)
{
    if (user_brightness > min_brightness)
    {
        int16_t new_brightness = user_brightness - CONFIG_DISPLAY_BRIGHTNESS_STEP;
        if (new_brightness < min_brightness)
        {
            new_brightness = min_brightness;
        }
        set_display_brightness((uint8_t)new_brightness);
    }
}

#endif // CONFIG_DISPLAY_BRIGHTNESS_KEYBOARD_CONTROL

#if CONFIG_DISPLAY_IDLE_TIMEOUT_S > 0 || CONFIG_DISPLAY_BRIGHTNESS_KEYBOARD_CONTROL

// --- Key event listener ---

static int key_listener(const zmk_event_t *eh)
{
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev && ev->state)
    { // Only on key down
        LOG_INF("Key pressed: keycode=%d", ev->keycode);

#if CONFIG_DISPLAY_BRIGHTNESS_KEYBOARD_CONTROL
        if (ev->keycode == CONFIG_DISPLAY_BRIGHTNESS_UP_KEYCODE)
        {
            LOG_INF("Brightness UP key recognized!");
            increase_brightness();
            return 0;
        }
        else if (ev->keycode == CONFIG_DISPLAY_BRIGHTNESS_DOWN_KEYCODE)
        {
            LOG_INF("Brightness DOWN key recognized!");
            decrease_brightness();
            return 0;
        }
#endif
    }

#if CONFIG_DISPLAY_IDLE_TIMEOUT_S > 0
    last_activity = k_uptime_get();
    if (!display_on)
    {
        display_set_on(true);
        k_wakeup(display_idle_tid);
    }
#else
    // Without idle thread: just turn on display
    if (!display_on)
    {
        display_set_on(true);
    }
#endif
    return 0;
}

ZMK_LISTENER(display_idle, key_listener);
ZMK_SUBSCRIPTION(display_idle, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(display_idle, zmk_layer_state_changed);

#endif

// --- Initialization ---

static int init_fixed_brightness(void)
{
    set_display_brightness(user_brightness);
    last_activity = k_uptime_get();
#if CONFIG_DISPLAY_IDLE_TIMEOUT_S > 0
    // Wake up the idle thread at boot
    k_wakeup(display_idle_tid);
#else
    LOG_INF("Display idle timeout disabled");
#endif
    return 0;
}

SYS_INIT(init_fixed_brightness, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);