#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

LOG_MODULE_REGISTER(als, 4);

#ifndef CONFIG_DISPLAY_IDLE_TIMEOUT_S
#define CONFIG_DISPLAY_IDLE_TIMEOUT_S 30
#endif

#ifndef CONFIG_DISPLAY_INITIAL_BRIGHTNESS
#define CONFIG_DISPLAY_INITIAL_BRIGHTNESS 10
#endif

#define BRIGHTNESS_STEP 2
#define BRIGHTNESS_DELAY_MS 10

#define DISPLAY_IDLE_TIMEOUT_MS (CONFIG_DISPLAY_IDLE_TIMEOUT_S * 1000)

static const struct device *pwm_leds_dev = DEVICE_DT_GET_ONE(pwm_leds);
#define DISP_BL DT_NODE_CHILD_IDX(DT_NODELABEL(disp_bl))

static int64_t last_activity = 0;
static bool display_on = true;
static uint8_t last_brightness = CONFIG_DISPLAY_INITIAL_BRIGHTNESS;

void set_display_brightness(uint8_t value)
{
    if (value > 100)
        value = 100;
    if (value < 1)
        value = 1;
    last_brightness = value;
    led_set_brightness(pwm_leds_dev, DISP_BL, value);
    LOG_INF("Display brightness set to %d", value);
}

#if CONFIG_DISPLAY_IDLE_TIMEOUT_S > 0

static void display_set_on(bool on)
{
    if (on && !display_on)
    {
        // Smooth fade-in
        for (uint8_t b = 0; b <= last_brightness; b += BRIGHTNESS_STEP)
        {
            led_set_brightness(pwm_leds_dev, DISP_BL, b);
            k_msleep(BRIGHTNESS_DELAY_MS);
        }
        led_set_brightness(pwm_leds_dev, DISP_BL, last_brightness); // Ensure target value
        display_on = true;
        LOG_INF("Display on (smooth)");
    }
    else if (!on && display_on)
    {
        // Smooth fade-out
        for (int b = last_brightness; b >= 0; b -= BRIGHTNESS_STEP)
        {
            led_set_brightness(pwm_leds_dev, DISP_BL, b);
            k_msleep(BRIGHTNESS_DELAY_MS);
        }
        led_set_brightness(pwm_leds_dev, DISP_BL, 0); // Ensure target value
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

// Set initial brightness at system startup
static int init_fixed_brightness(void)
{
    set_display_brightness(last_brightness);
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