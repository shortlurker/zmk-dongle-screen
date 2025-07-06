# ZMK Dongle Screen YADS (Yet another Dongle Screen)

This project provides a Zephyr module for a dongle display shield based on the ST7789V display and the Seeeduino XAIO BLE microcontroller and the LVGL graphics library. It offers various widgets for current output, displaying layer, mod, WPM, and battery status, as well as brightness adjustments via keyboard, automatic dimming after inactivity, and a customizable status screen for ZMK-based keyboards.

**This project is inspired by [prospector-zmk-module](https://github.com/carrefinho/prospector-zmk-module) and [zmk-dongle-display](https://github.com/englmaxi/zmk-dongle-display). Thanks for your awesome work!**

## Demo

![Sample Screen of zmk-dongle-screen](/docs/images/screen.jpg)



https://github.com/user-attachments/assets/86c33af6-d83e-4e2a-9766-fc8836e896f1



## Building a dongle

To build a dongle yourself you can use the build guide by **carrefinho** ([prospector project](https://github.com/carrefinho/prospector)).  
This repository only contains a module and no build guides or suggestions.

## Widgets Overview

This module provides several widgets to visualize the current state of your ZMK-based keyboard:

- **Output Widget**  
  Indicates the current output state of the keyboard (USB or BLE profiles). The currently used interface (USB or BLE) is indicated with an arrow.
  - **USB:**
    - **White:** USB HID is ready and active (dongle is connected to a computer and working as a keyboard).
    - **Red:** USB HID is not ready (dongle is powered, e.g. via wall plug or power bank, but not connected to a computer).
  - **BLE:**  
    For the currently selected Bluetooth profile (the number is shown in the next line):
    - **Green:** Connected (active BLE connection established)
    - **Blue:** Bonded (device is paired, but not currently connected)
    - **White:** Profile is free (no device paired or connected for this profile)

- **Layer Widget**  
  Displays the currently active keyboard layer. Useful for quickly identifying which layer is active.

- **Mod Widget**  
  Shows the status of modifier keys (e.g., Shift, Ctrl, Alt, GUI). Indicates which modifiers are currently pressed.

- **WPM Widget**  
  Displays the current words per minute (WPM) typing speed in real time.

- **Battery Widget**  
  Shows the battery level of the dongle and/or the keyboard, if supported.

## General Features

- **Custom Status Screen**  
  Combine and arrange widgets as you like for a fully customizable status display. (Code changes and recompiling are needed for this.)

- **Brightness Control**  
  Adjust the display brightness via keyboard shortcuts. By default, F23 and F24 are mapped to this. You'll just have to assign this in your keyboard keymap.

- **Configurable Display Orientation**  
  Set the screen orientation to match your keyboard or desk setup (horizontal or vertical). Additionally, the screen can be flipped to match the orientation of the display in your casing.

- **Idle Timeout**  
  Automatically turns off or dims the display after a configurable period of inactivity (no keystrokes). It automatically turns on when the first keystroke is detected again.  
  The idle timeout can be set in seconds. If set to `0`, the display will never dim or turn off automatically.  
  When the idle timeout is reached, the display brightness will be set to the value defined in `DONGLE_SCREEN_MIN_BRIGHTNESS`.  
  When activity resumes, the brightness will be restored to the last value (up to `DONGLE_SCREEN_MAX_BRIGHTNESS`).  
  Make sure that `DONGLE_SCREEN_MIN_BRIGHTNESS` is set to a value that is visible enough for your use case (0 means the display is completely off).

## Installation

1. This guide assumes that you have already implemented a basic dongle setup as described [here](https://zmk.dev/docs/development/hardware-integration/dongle).
2. Once this is done, add this repository to your `west.yaml`.  
   Example:

   ```yaml
   manifest:
     remotes:
       - name: zmkfirmware
         url-base: https://github.com/zmkfirmware
       - name: janpfischer
         url-base: https://github.com/janpfischer
     projects:
       - name: zmk
         remote: zmkfirmware
         revision: main
         import: app/west.yml
       - name: zmk-dongle-screen
         remote: janpfischer
         revision: main
     self:
       path: config
   ```

3. The shield must be included in your build configuration for the dongle you set up in step 1.  
   Example `build.yaml` snippet:

   ```yaml
   include:
     - board: seeeduino_xiao_ble
       shield: [YOUR_CONFIGURED_DONGLE] dongle_screen
       #cmake-args: -DCONFIG_LOG_PROCESS_THREAD_STARTUP_DELAY_MS=8000 #optional if logging is enabled
       #snippet: zmk-usb-logging #only enable for debugging
       artifact-name: dongle-screen
   ```

4. Keyboard splits must be configured as peripherals.  
   Example `build.yaml` snippet:

   ```yaml
   include:
     - board: seeeduino_xiao_ble
       shield: split_left
       cmake-args: -DCONFIG_ZMK_SPLIT=y -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n
       artifact-name: split-dongle-left
     - board: seeeduino_xiao_ble
       shield: split_right
       cmake-args: -DCONFIG_ZMK_SPLIT=y -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n
       artifact-name: split-dongle-right
   ```

5. Adjust the desired configuration options in your `[YOUR_CONFIGURED_DONGLE].conf` (see table below).

### Configuration sample

A sample `build.yaml` based on `seeeduino_xiao_ble` boards for the keyboard and the dongle including a `settings_reset` firmware could look like this:

```yaml
include:
  - board: seeeduino_xiao_ble
    shield: totem_left
    cmake-args: -DCONFIG_ZMK_SPLIT=y -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n
    artifact-name: totem-dongle-left
  - board: seeeduino_xiao_ble
    shield: totem_right
    cmake-args: -DCONFIG_ZMK_SPLIT=y -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n
    artifact-name: totem-dongle-right
  - board: seeeduino_xiao_ble
    shield: totem_dongle dongle_screen
    cmake-args: -DCONFIG_LOG_PROCESS_THREAD_STARTUP_DELAY_MS=8000
    snippet: zmk-usb-logging
    artifact-name: totem-dongle-screen
  - board: seeeduino_xiao_ble
    shield: settings_reset
    artifact-name: totem-settings-reset
```

## Configuration Options

| Name                                        | Type | Default | Description                                                                                                                                                      |
| ------------------------------------------- | ---- | ------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `DONGLE_SCREEN_HORIZONTAL`                  | bool | y       | Orientation of the screen. By default, it is horizontal (laying on the side).                                                                                    |
| `DONGLE_SCREEN_FLIPPED`                     | bool | n       | Should the screen orientation be flipped in horizontal or vertical orientation?                                                                                  |
| `DONGLE_SCREEN_IDLE_TIMEOUT_S`              | int  | 600     | Screen idle timeout in seconds (0 = never off). Time in seconds after which the screen turns off when idle.                                                      |
| `DONGLE_SCREEN_MAX_BRIGHTNESS`              | int  | 80      | Maximum screen brightness (1-100). This is the brightness used when the dongle is powered on and the maximum used by the dimmer.                                 |
| `DONGLE_SCREEN_MIN_BRIGHTNESS`              | int  | 0       | Minimum screen brightness (0-99, 0 = off). This is also the brightness used by the automatic dimmer. If greater than 0, the screen will not turn off completely. |
| `DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL` | bool | y       | Allows controlling the screen brightness via keyboard (e.g., F23/F24).                                                                                           |
| `DONGLE_SCREEN_BRIGHTNESS_UP_KEYCODE`       | int  | 115     | Keycode for increasing screen brightness (default: F24).                                                                                                         |
| `DONGLE_SCREEN_BRIGHTNESS_DOWN_KEYCODE`     | int  | 114     | Keycode for decreasing screen brightness (default: F23).                                                                                                         |
| `DONGLE_SCREEN_BRIGHTNESS_STEP`             | int  | 10      | Step for brightness adjustment with keyboard. How much brightness (range MIN_BRIGHTNESS to MAX_BRIGHTNESS) should be applied per keystroke.                      |

## Example Configuration (`prj.conf`)

```conf
CONFIG_DONGLE_SCREEN_HORIZONTAL=y
CONFIG_DONGLE_SCREEN_FLIPPED=n
CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S=300
CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS=90
CONFIG_DONGLE_SCREEN_MIN_BRIGHTNESS=10
CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL=y
CONFIG_DONGLE_SCREEN_BRIGHTNESS_UP_KEYCODE=115
CONFIG_DONGLE_SCREEN_BRIGHTNESS_DOWN_KEYCODE=114
CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP=5
```

## Development

If you want to develop new features or change the layout of the screen you'll have to clone this repo and build it on your own.  
Refer to the [ZMK Local toolchain](https://zmk.dev/docs/development/local-toolchain/build-flash) documentation for this.

A command for building locally _can_ look something like this:

```
west build -p -s /workspaces/zmk/app -d "/workspaces/zmk-build-output/totem_dongle" -b "seeeduino_xiao_ble" -S zmk-usb-logging -- -DZMK_CONFIG=/workspaces/zmk-config/config -DSHIELD="totem_dongle dongle_screen" -DZMK_EXTRA_MODULES=/workspaces/zmk-modules/zmk-dongle-screen/
```

_Note: a matching entry for `-DSHIELD` must already be present in your `build.yaml` in your configuration, which is given as the `-DZMK_CONFIG` argument._

## License

MIT License

---

_This project is part of the ZMK community and licensed under the MIT License._
