#include "tusb.h"

// Calculate total configuration size
// = configuration descriptor + HID descriptor + CDC descriptor
#define TUD_CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_CDC_DESC_LEN)

// Device Descriptor
tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A,  // Espressif vendor ID
    .idProduct = 0x1001, // Your product ID
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

// HID Report Descriptor
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(2))
};

// Configuration Descriptor
uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, TUD_CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), 0x81, CFG_TUD_HID_EP_BUFSIZE, 10),
    TUD_CDC_DESCRIPTOR(1, 4, 0x82, 8, 0x03, 0x83, 64)
};

// String Descriptors
char const* string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 }, // 0: supported language is English (0x0409)
    "NXE5",                        // 1: Manufacturer
    "Custom MacroPad",            // 2: Product
    "123456",                      // 3: Serial number
};

// TinyUSB descriptors callback functions
extern "C" {
    uint8_t const * tud_descriptor_device_cb(void) {
        return (uint8_t const *) &desc_device;
    }

    uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
        return desc_configuration;
    }

    uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
        static uint16_t desc_str[32];
        uint8_t len;

        if (index == 0) {
            memcpy(&desc_str[1], string_desc_arr[0], 2);
            len = 1;
        } else {
            const char* str = string_desc_arr[index];
            len = strlen(str);
            for (uint8_t i=0; i<len; i++) {
                desc_str[1+i] = str[i];
            }
        }

        desc_str[0] = (TUSB_DESC_STRING << 8) | (2 + 2*len);
        return desc_str;
    }

    uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
        return desc_hid_report;
    }
}