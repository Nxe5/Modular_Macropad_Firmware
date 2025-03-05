// include/tinyusb_hid.h

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "tusb.h"
#include "usb_descriptors.h" // Include this to get the REPORT_ID definitions

/**
 * @brief Initialize tinyusb HID device.
 *
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_NO_MEM: No memory
 */
esp_err_t tinyusb_hid_init(void);

/**
 * @brief Report delta movement of mouse.
 *
 * @param x Current delta x movement of the mouse
 * @param y Current delta y movement on the mouse
 * @param vertical Current delta wheel movement on the mouse
 * @param horizontal using AC Pan
 */
void tinyusb_hid_mouse_move_report(int8_t x, int8_t y, int8_t vertical, int8_t horizontal);

/**
 * @brief Report button click in the mouse, using bitmap here.
 * eg. MOUSE_BUTTON_LEFT | MOUSE_BUTTON_RIGHT, if click left and right button at same time.
 *
 * @param buttons hid mouse button bit mask
 */
void tinyusb_hid_mouse_button_report(uint8_t buttons_map);

/**
 * @brief Report key press in the keyboard, using array here
 *
 * @param keycode hid keyboard code array
 */
void tinyusb_hid_keyboard_report(uint8_t modifier, uint8_t keycode[]);

// Mouse button definitions
#define MOUSE_BUTTON_LEFT     TU_BIT(0)
#define MOUSE_BUTTON_RIGHT    TU_BIT(1)
#define MOUSE_BUTTON_MIDDLE   TU_BIT(2)
#define MOUSE_BUTTON_BACKWARD TU_BIT(3)
#define MOUSE_BUTTON_FORWARD  TU_BIT(4)

// Common HID keycodes
#define HID_KEY_NONE          0x00
#define HID_KEY_A             0x04
#define HID_KEY_B             0x05
#define HID_KEY_C             0x06
#define HID_KEY_D             0x07
#define HID_KEY_E             0x08
#define HID_KEY_F             0x09
#define HID_KEY_G             0x0A
#define HID_KEY_H             0x0B
#define HID_KEY_I             0x0C
#define HID_KEY_J             0x0D
#define HID_KEY_K             0x0E
#define HID_KEY_L             0x0F
#define HID_KEY_M             0x10
#define HID_KEY_N             0x11
#define HID_KEY_O             0x12
#define HID_KEY_P             0x13
#define HID_KEY_Q             0x14
#define HID_KEY_R             0x15
#define HID_KEY_S             0x16
#define HID_KEY_T             0x17
#define HID_KEY_U             0x18
#define HID_KEY_V             0x19
#define HID_KEY_W             0x1A
#define HID_KEY_X             0x1B
#define HID_KEY_Y             0x1C
#define HID_KEY_Z             0x1D
#define HID_KEY_1             0x1E
#define HID_KEY_2             0x1F
#define HID_KEY_3             0x20
#define HID_KEY_4             0x21
#define HID_KEY_5             0x22
#define HID_KEY_6             0x23
#define HID_KEY_7             0x24
#define HID_KEY_8             0x25
#define HID_KEY_9             0x26
#define HID_KEY_0             0x27
#define HID_KEY_ENTER         0x28
#define HID_KEY_ESCAPE        0x29
#define HID_KEY_BACKSPACE     0x2A
#define HID_KEY_TAB           0x2B
#define HID_KEY_SPACE         0x2C

#ifdef __cplusplus
}
#endif