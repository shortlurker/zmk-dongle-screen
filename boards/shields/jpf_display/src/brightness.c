#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

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
static bool display_on = true;
static uint8_t max_brightness = CONFIG_DISPLAY_MAX_BRIGHTNESS;
static uint8_t min_brightness = CONFIG_DISPLAY_MIN_BRIGHTNESS;

void set_display_brightness(uint8_t value)
{
    if (value > max_brightness)
        value = max_brightness;
    if (value < min_brightness)
        value = min_brightness;
    led_set_brightness(pwm_leds_dev, DISP_BL, value);
    LOG_INF("Display brightness set to %d", value);
}

#if CONFIG_DISPLAY_IDLE_TIMEOUT_S > 0

static void display_set_on(bool on)
{
    if (on && !display_on)
    {
        uint8_t start = min_brightness;
        if (start > max_brightness)
            start = 0;
        if (start == max_brightness)
        {
            led_set_brightness(pwm_leds_dev, DISP_BL, max_brightness);
        }
        else
        {
            for (uint8_t b = start; b < max_brightness; b += BRIGHTNESS_STEP)
            {
                led_set_brightness(pwm_leds_dev, DISP_BL, b);
                k_msleep(BRIGHTNESS_DELAY_MS);
            }
            led_set_brightness(pwm_leds_dev, DISP_BL, max_brightness); // Ensure target value
        }
        display_on = true;
        LOG_INF("Display on (smooth)");
    }
    else if (!on && display_on)
    {
        uint8_t end = min_brightness;
        if (end > max_brightness)
            end = 0;
        if (end == max_brightness)
        {
            led_set_brightness(pwm_leds_dev, DISP_BL, end);
        }
        else
        {
            for (int b = max_brightness; b > end; b -= BRIGHTNESS_STEP)
            {
                led_set_brightness(pwm_leds_dev, DISP_BL, b);
                k_msleep(BRIGHTNESS_DELAY_MS);
            }
            led_set_brightness(pwm_leds_dev, DISP_BL, end); // Ensure target value
        }
        display_on = false;
        LOG_INF("Display off (smooth)");
    }
}

// Idle thread: checks for inactivity and turns off the display after the configured timeout.
// To save resources, the thread sleeps exactly as long as needed until the next timeout or activity.
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

// Only register the event listener if timeout is enabled
static int key_listener(const zmk_event_t *eh)
{
    last_activity = k_uptime_get();
    if (!display_on)
    {
        display_set_on(true);
        k_wakeup(display_idle_tid); // Wake up the idle thread after re-enabling display
    }
    return 0;
}

ZMK_LISTENER(display_idle, key_listener);
ZMK_SUBSCRIPTION(display_idle, zmk_keycode_state_changed);

#endif // CONFIG_DISPLAY_IDLE_TIMEOUT_S > 0

// Set max brightness at system startup
static int init_fixed_brightness(void)
{
    set_display_brightness(max_brightness);
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