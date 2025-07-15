#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if CONFIG_DONGLE_SCREEN_MIN_BRIGHTNESS > CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS
#error "DONGLE_SCREEN_MIN_BRIGHTNESS must be less than or equal to DONGLE_SCREEN_MAX_BRIGHTNESS!"
#endif

#define BRIGHTNESS_STEP 2
#define BRIGHTNESS_DELAY_MS 10
#define SCREEN_IDLE_TIMEOUT_MS (CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S * 1000)

static const struct device *pwm_leds_dev = DEVICE_DT_GET_ONE(pwm_leds);
#define DISP_BL DT_NODE_CHILD_IDX(DT_NODELABEL(disp_bl))

static int64_t last_activity = 0;
static uint8_t max_brightness = CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS;
static uint8_t min_brightness = CONFIG_DONGLE_SCREEN_MIN_BRIGHTNESS;
static uint8_t user_brightness = CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS;

static uint8_t clamp_brightness(uint8_t value)
{
    if (value > max_brightness)
    {
        return max_brightness;
        LOG_WRN("CLAMPED: Screen brightness %d would be over %d", value, max_brightness);
    }
    if (value < min_brightness)
    {
        LOG_WRN("CLAMPED: Screen brightness %d would be under %d", value, min_brightness);
        return min_brightness;
    }
    return value;
}
static void apply_brightness(uint8_t value)
{
    led_set_brightness(pwm_leds_dev, DISP_BL, value);
    LOG_INF("Screen brightness set to %d", value);
}

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

void set_screen_brightness(uint8_t value)
{
    uint8_t new_brightness = clamp_brightness(value);
    fade_to_brightness(user_brightness, new_brightness);
    user_brightness = new_brightness;
}

#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0 || CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL
// --- Brightness logic ---
static bool screen_on = true;
// --- Screen on/off ---

static void screen_set_on(bool on)
{
    if (on && !screen_on)
    {
        fade_to_brightness(min_brightness, user_brightness);
        screen_on = true;
        LOG_INF("Screen on (smooth)");
    }
    else if (!on && screen_on)
    {
        fade_to_brightness(user_brightness, min_brightness);
        screen_on = false;
        LOG_INF("Screen off (smooth)");
    }
}

#endif

// --- Idle thread ---

#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0

void screen_idle_thread(void)
{
    while (1)
    {
        if (screen_on)
        {
            int64_t now = k_uptime_get();
            int64_t elapsed = now - last_activity;
            int64_t remaining = SCREEN_IDLE_TIMEOUT_MS - elapsed;

            if (remaining <= 0)
            {
                screen_set_on(false);
                // After turning off, sleep until next activity (key event will wake screen)
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
            // If Screen is off, sleep forever (will be interrupted by key event)
            k_sleep(K_FOREVER);
        }
    }
}

K_THREAD_DEFINE(screen_idle_tid, 512, screen_idle_thread, NULL, NULL, NULL, 7, 0, 0);

#endif // CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0

// --- Brightness control via keyboard ---

#if CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL

static void increase_brightness(void)
{
    if (user_brightness < max_brightness)
    {
        set_screen_brightness(user_brightness + CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP);
    }
}

static void decrease_brightness(void)
{
    if (user_brightness > min_brightness)
    {
        int16_t new_brightness = user_brightness - CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP;
        if (new_brightness < min_brightness)
        {
            new_brightness = min_brightness;
        }
        set_screen_brightness((uint8_t)new_brightness);
    }
}

#endif // CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL

#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0 || CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL

// --- Key event listener ---

static int key_listener(const zmk_event_t *eh)
{
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev && ev->state)
    { // Only on key down
        LOG_DBG("Key pressed: keycode=%d", ev->keycode);

#if CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL
        if (ev->keycode == CONFIG_DONGLE_SCREEN_BRIGHTNESS_UP_KEYCODE)
        {
            LOG_INF("Brightness UP key recognized!");
            increase_brightness();
            return 0;
        }
        else if (ev->keycode == CONFIG_DONGLE_SCREEN_BRIGHTNESS_DOWN_KEYCODE)
        {
            LOG_INF("Brightness DOWN key recognized!");
            decrease_brightness();
            return 0;
        }
#endif
    }

#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0
    last_activity = k_uptime_get();
    if (!screen_on)
    {
        screen_set_on(true);
        k_wakeup(screen_idle_tid);
    }
#else
    // Without idle thread: just turn on screen
    if (!screen_on)
    {
        screen_set_on(true);
    }
#endif
    return 0;
}

ZMK_LISTENER(screen_idle, key_listener);
ZMK_SUBSCRIPTION(screen_idle, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(screen_idle, zmk_layer_state_changed);

#endif

#if IS_ENABLED(CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT)

#define AMBIENT_LIGHT_SENSOR_NODE DT_INST(0, avago_apds9960)
static const struct device *ambient_sensor = DEVICE_DT_GET(AMBIENT_LIGHT_SENSOR_NODE);

// Passe diese Werte nach deinen Messungen an!
const int32_t min_sensor = 0;
const int32_t max_sensor = 100; // TODO: Find real values!

static uint8_t ambient_to_brightness(int32_t sensor_value)
{
    if (sensor_value < min_sensor)
        sensor_value = min_sensor;
    if (sensor_value > max_sensor)
        sensor_value = max_sensor;
    uint8_t brightness = min_brightness +
                         ((sensor_value - min_sensor) * (max_brightness - min_brightness)) /
                             (max_sensor - min_sensor);
    return clamp_brightness(brightness);
}

static void ambient_light_thread(void)
{
    struct sensor_value val;
    uint8_t last_brightness = 0xFF; // Invalid initial value to force first update

    while (1)
    {
        // Only adjust brightness if the display is on!
        if (!screen_on)
        {
            k_sleep(K_MSEC(EVALUATION_INTERVAL));
            continue;
        }

        if (!device_is_ready(ambient_sensor))
        {
            LOG_ERR("Ambient light sensor not ready!");
            k_sleep(K_SECONDS(5));
            continue;
        }
        int rc = sensor_sample_fetch(ambient_sensor);
        if (rc == 0)
        {
            rc = sensor_channel_get(ambient_sensor, SENSOR_CHAN_LIGHT, &val);
            if (rc == 0)
            {
                LOG_DBG("APDS9960 raw: %d", val.val1); // Logge den Rohwert!
                uint8_t new_brightness = ambient_to_brightness(val.val1);
                if (new_brightness != last_brightness)
                {
                    set_screen_brightness(new_brightness);
                    LOG_DBG("Ambient light: %d (raw) -> brightness %d", val.val1, new_brightness);
                    last_brightness = new_brightness;
                }
            }
        }
        k_sleep(K_MSEC(CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT_EVALUATION_INTERVAL_MS)); // Adjust interval as needed
    }
}

K_THREAD_DEFINE(ambient_light_tid, 512, ambient_light_thread, NULL, NULL, NULL, 7, 0, 0);

#endif // CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT

// --- Initialization ---

static int init_fixed_brightness(void)
{
    set_screen_brightness(user_brightness);
    last_activity = k_uptime_get();
#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0
    // Wake up the idle thread at boot
    k_wakeup(screen_idle_tid);
#else
    LOG_INF("Screen idle timeout disabled");
#endif
    return 0;
}

SYS_INIT(init_fixed_brightness, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);