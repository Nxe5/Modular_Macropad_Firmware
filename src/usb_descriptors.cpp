#include "tusb.h"
#include "HIDDescriptorHelpers.h"

#define TUD_HID_REPORT_DESC_MEDIA_CONTROL \
    HID_USAGE_PAGE(HID_USAGE_PAGE_CONSUMER), \
    HID_USAGE(HID_USAGE_CONSUMER_CONTROL), \
    HID_COLLECTION(HID_COLLECTION_APPLICATION), \
        HID_LOGICAL_MIN(0), \
        HID_LOGICAL_MAX(1), \
        HID_REPORT_COUNT(16), \
        HID_REPORT_SIZE(1), \
        HID_USAGE(HID_USAGE_CONSUMER_PLAY_PAUSE), \
        HID_USAGE(HID_USAGE_CONSUMER_SCAN_NEXT_TRACK), \
        HID_USAGE(HID_USAGE_CONSUMER_SCAN_PREV_TRACK), \
        HID_USAGE(HID_USAGE_CONSUMER_STOP), \
        HID_USAGE(HID_USAGE_CONSUMER_VOLUME_UP), \
        HID_USAGE(HID_USAGE_CONSUMER_VOLUME_DOWN), \
        HID_USAGE(HID_USAGE_CONSUMER_MUTE), \
        HID_USAGE(HID_USAGE_CONSUMER_MENU), \
        HID_USAGE(HID_USAGE_CONSUMER_MENU_PICK), \
        HID_USAGE(HID_USAGE_CONSUMER_MENU_UP), \
        HID_USAGE(HID_USAGE_CONSUMER_MENU_DOWN), \
        HID_USAGE(HID_USAGE_CONSUMER_MENU_LEFT), \
        HID_USAGE(HID_USAGE_CONSUMER_MENU_RIGHT), \
        HID_USAGE(HID_USAGE_CONSUMER_MENU_ESCAPE), \
        HID_INPUT(HID_INPUT_ARRAY), \
    HID_COLLECTION_END

#define TUD_HID_REPORT_DESC_EXTRA_BUTTONS \
    HID_USAGE_PAGE(HID_USAGE_PAGE_CONSUMER), \
    HID_USAGE(HID_USAGE_CONSUMER_CONTROL), \
    HID_COLLECTION(HID_COLLECTION_APPLICATION), \
        HID_LOGICAL_MIN(0), \
        HID_LOGICAL_MAX(255), \
        HID_REPORT_COUNT(16), \
        HID_REPORT_SIZE(8), \
        HID_USAGE(HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_1), \
        HID_USAGE(HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_2), \
        HID_USAGE(HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_3), \
        HID_USAGE(HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_4), \
        HID_USAGE(HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_5), \
        HID_USAGE(HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_6), \
        HID_USAGE(HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_7), \
        HID_USAGE(HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_8), \
        HID_USAGE(HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_9), \
        HID_USAGE(HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_10), \
        HID_USAGE(HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_11), \
        HID_USAGE(HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_12), \
        HID_USAGE(HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_13), \
        HID_USAGE(HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_14), \
        HID_USAGE(HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_15), \
        HID_USAGE(HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_16), \
        HID_INPUT(HID_INPUT_ARRAY), \
    HID_COLLECTION_END


#define TUD_HID_REPORT_DESC_KEYBOARD \
    HID_USAGE_PAGE( HID_USAGE_PAGE_DESKTOP )                   ,\
    HID_USAGE( HID_USAGE_DESKTOP_KEYBOARD )                    ,\
    HID_COLLECTION( HID_COLLECTION_APPLICATION )               ,\
        HID_REPORT_SIZE( 8 )                                   ,\
        HID_REPORT_COUNT( 8 )                                  ,\
        HID_USAGE_PAGE( HID_USAGE_PAGE_KEYBOARD )              ,\
        HID_USAGE_MIN( 224 )                                   ,\
        HID_USAGE_MAX( 231 )                                   ,\
        HID_LOGICAL_MIN( 0 )                                   ,\
        HID_LOGICAL_MAX( 1 )                                   ,\
        HID_INPUT( HID_INPUT_ARRAY )                           ,\
        HID_REPORT_COUNT( 1 )                                  ,\
        HID_REPORT_SIZE( 8 )                                   ,\
        HID_INPUT( HID_INPUT_CONSTANT )                        ,\
        HID_REPORT_COUNT( 5 )                                  ,\
        HID_REPORT_SIZE( 1 )                                   ,\
        HID_USAGE_PAGE( HID_USAGE_PAGE_LED )                   ,\
        HID_USAGE_MIN( 1 )                                     ,\
        HID_USAGE_MAX( 5 )                                     ,\
        HID_OUTPUT( HID_OUTPUT_ARRAY )                         ,\
        HID_REPORT_COUNT( 1 )                                  ,\
        HID_REPORT_SIZE( 3 )                                   ,\
        HID_OUTPUT( HID_OUTPUT_CONSTANT )                      ,\
        HID_REPORT_COUNT( 6 )                                  ,\
        HID_REPORT_SIZE( 8 )                                   ,\
        HID_LOGICAL_MIN( 0 )                                   ,\
        HID_LOGICAL_MAX( 101 )                                 ,\
        HID_USAGE_PAGE( HID_USAGE_PAGE_KEYBOARD )              ,\
        HID_USAGE_MIN( 0 )                                     ,\
        HID_USAGE_MAX( 101 )                                   ,\
        HID_INPUT( HID_INPUT_ARRAY )                           ,\
    HID_COLLECTION_END

// Similarly, define TUD_HID_REPORT_DESC_CONSUMER if needed.


// HID Report Descriptors
#define TUD_HID_REPORT_DESC_KEYBOARD \
    HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP     )                    ,\
    HID_USAGE      ( HID_USAGE_DESKTOP_KEYBOARD )                    ,\
    HID_COLLECTION ( HID_COLLECTION_APPLICATION )                    ,\
        HID_REPORT_SIZE  ( 8 )                                       ,\
        HID_REPORT_COUNT ( 8 )                                       ,\
        HID_USAGE_PAGE   ( HID_USAGE_PAGE_KEYBOARD )                 ,\
        HID_USAGE_MIN    ( 224                     )                 ,\
        HID_USAGE_MAX    ( 231                     )                 ,\
        HID_LOGICAL_MIN  ( 0                       )                 ,\
        HID_LOGICAL_MAX  ( 1                       )                 ,\
        HID_INPUT        ( HID_INPUT_ARRAY        )                  ,\
        HID_REPORT_COUNT ( 1                       )                 ,\
        HID_REPORT_SIZE  ( 8                       )                 ,\
        HID_INPUT        ( HID_INPUT_CONSTANT      )                 ,\
        HID_REPORT_COUNT ( 5                       )                 ,\
        HID_REPORT_SIZE  ( 1                       )                 ,\
        HID_USAGE_PAGE   ( HID_USAGE_PAGE_LED      )                 ,\
        HID_USAGE_MIN    ( 1                       )                 ,\
        HID_USAGE_MAX    ( 5                       )                 ,\
        HID_OUTPUT       ( HID_OUTPUT_ARRAY        )                 ,\
        HID_REPORT_COUNT ( 1                       )                 ,\
        HID_REPORT_SIZE  ( 3                       )                 ,\
        HID_OUTPUT       ( HID_OUTPUT_CONSTANT     )                 ,\
        HID_REPORT_COUNT ( 6                       )                 ,\
        HID_REPORT_SIZE  ( 8                       )                 ,\
        HID_LOGICAL_MIN  ( 0                       )                 ,\
        HID_LOGICAL_MAX  ( 101                     )                 ,\
        HID_USAGE_PAGE   ( HID_USAGE_PAGE_KEYBOARD )                 ,\
        HID_USAGE_MIN    ( 0                       )                 ,\
        HID_USAGE_MAX    ( 101                     )                 ,\
        HID_INPUT        ( HID_INPUT_ARRAY         )                 ,\
    HID_COLLECTION_END                                               ,

#define TUD_HID_REPORT_DESC_CONSUMER \
    HID_USAGE_PAGE ( HID_USAGE_PAGE_CONSUMER    )                    ,\
    HID_USAGE      ( HID_USAGE_CONSUMER_CONTROL )                    ,\
    HID_COLLECTION ( HID_COLLECTION_APPLICATION )                    ,\
        HID_REPORT_COUNT ( 7 )                                       ,\
        HID_REPORT_SIZE  ( 1 )                                       ,\
        HID_LOGICAL_MIN  ( 0 )                                       ,\
        HID_LOGICAL_MAX  ( 1 )                                       ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_SCAN_NEXT_TRACK  )     ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_SCAN_PREV_TRACK  )     ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_STOP             )     ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_PLAY_PAUSE       )     ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_MUTE             )     ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_VOLUME_UP        )     ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_VOLUME_DOWN      )     ,\
        HID_INPUT        ( HID_INPUT_ARRAY         )                 ,\
        HID_REPORT_COUNT ( 1 )                                       ,\
        HID_REPORT_SIZE  ( 1 )                                       ,\
        HID_INPUT        ( HID_INPUT_CONSTANT      )                 ,\
    HID_COLLECTION_END                                               ,

#define TUD_HID_REPORT_DESC_MEDIA_CONTROL \
    HID_USAGE_PAGE  ( HID_USAGE_PAGE_CONSUMER )                      ,\
    HID_USAGE       ( HID_USAGE_CONSUMER_CONTROL )                   ,\
    HID_COLLECTION  ( HID_COLLECTION_APPLICATION )                   ,\
        HID_LOGICAL_MIN  ( 0 )                                       ,\
        HID_LOGICAL_MAX  ( 1 )                                       ,\
        HID_REPORT_COUNT ( 16 )                                      ,\
        HID_REPORT_SIZE  ( 1 )                                       ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_PLAY_PAUSE )           ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_SCAN_NEXT_TRACK )      ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_SCAN_PREV_TRACK )      ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_STOP )                 ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_VOLUME_UP )            ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_VOLUME_DOWN )          ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_MUTE )                 ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_MENU )                 ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_MENU_PICK )            ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_MENU_UP )              ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_MENU_DOWN )            ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_MENU_LEFT )            ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_MENU_RIGHT )           ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_MENU_ESCAPE )          ,\
        HID_INPUT        ( HID_INPUT_ARRAY )                         ,\
    HID_COLLECTION_END                                               ,

#define TUD_HID_REPORT_DESC_EXTRA_BUTTONS \
    HID_USAGE_PAGE  ( HID_USAGE_PAGE_CONSUMER )                      ,\
    HID_USAGE       ( HID_USAGE_CONSUMER_CONTROL )                   ,\
    HID_COLLECTION  ( HID_COLLECTION_APPLICATION )                   ,\
        HID_LOGICAL_MIN  ( 0 )                                       ,\
        HID_LOGICAL_MAX  ( 255 )                                     ,\
        HID_REPORT_COUNT ( 16 )                                      ,\
        HID_REPORT_SIZE  ( 8 )                                       ,\
        HID_USAGE        ( HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_1 ),\
        HID_USAGE        ( HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_2 ),\
        HID_USAGE        ( HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_3 ),\
        HID_USAGE        ( HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_4 ),\
        HID_USAGE        ( HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_5 ),\
        HID_USAGE        ( HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_6 ),\
        HID_USAGE        ( HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_7 ),\
        HID_USAGE        ( HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_8 ),\
        HID_USAGE        ( HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_9 ),\
        HID_USAGE        ( HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_10),\
        HID_USAGE        ( HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_11),\
        HID_USAGE        ( HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_12),\
        HID_USAGE        ( HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_13),\
        HID_USAGE        ( HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_14),\
        HID_USAGE        ( HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_15),\
        HID_USAGE        ( HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTON_16),\
        HID_INPUT        ( HID_INPUT_ARRAY )                         ,\
    HID_COLLECTION_END                                               ,

// Define report descriptors
uint8_t const desc_hid_report_keyboard[] = { TUD_HID_REPORT_DESC_KEYBOARD };
uint8_t const desc_hid_report_consumer[] = { TUD_HID_REPORT_DESC_CONSUMER };
uint8_t const desc_hid_report_media_control[] = { TUD_HID_REPORT_DESC_MEDIA_CONTROL };
uint8_t const desc_hid_report_extra_buttons[] = { TUD_HID_REPORT_DESC_EXTRA_BUTTONS };

// Configuration Descriptor
uint8_t const desc_configuration[] = {
    // Configuration descriptor
    TUD_CONFIG_DESCRIPTOR(1,                     // Configuration number
                          4,                     // Number of interfaces
                          0,                     // String index of configuration description
                          TUD_CONFIG_DESC_LEN + (TUD_HID_DESC_LEN * 4), // Total length
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, // Attributes
                          100),                  // Power in mA

    // Keyboard HID Interface
    TUD_HID_DESCRIPTOR(0,                        // Interface number
                       0,                        // String index
                       HID_ITF_PROTOCOL_KEYBOARD,// Protocol
                       sizeof(desc_hid_report_keyboard), // Report descriptor length
                       0x81,                     // Endpoint address (IN)
                       CFG_TUD_HID_EP_BUFSIZE,   // Max packet size
                       10),                      // Polling interval

    // Consumer Control HID Interface
    TUD_HID_DESCRIPTOR(1,                        // Interface number
                       0,                        // String index
                       HID_ITF_PROTOCOL_NONE,    // Protocol
                       sizeof(desc_hid_report_consumer), // Report descriptor length
                       0x82,                     // Endpoint address (IN)
                       CFG_TUD_HID_EP_BUFSIZE,   // Max packet size
                       10),                      // Polling interval

    // Media Control HID Interface
    TUD_HID_DESCRIPTOR(2,                        // Interface number
                       0,                        // String index
                       HID_ITF_PROTOCOL_NONE,    // Protocol
                       sizeof(desc_hid_report_media_control), // Report descriptor length
                       0x83,                     // Endpoint address (IN)
                       CFG_TUD_HID_EP_BUFSIZE,   // Max packet size
                       10),                      // Polling interval

    // Extra Buttons HID Interface
    TUD_HID_DESCRIPTOR(3,                        // Interface number
                       0,                        // String index
                       HID_ITF_PROTOCOL_NONE,    // Protocol
                       sizeof(desc_hid_report_extra_buttons), // Report descriptor length
                       0x84,                     // Endpoint address (IN)
                       CFG_TUD_HID_EP_BUFSIZE,   // Max packet size
                       10)                       // Polling interval
};

// Device descriptor
tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A,
    .idProduct = 0x1001,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

// String descriptors
char const* string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 }, // 0: supported language is English (0x0409)
    "NXE5",                        // 1: Manufacturer
    "Modular MacroPad",            // 2: Product
    "123456",                      // 3: Serial Number
};

// Standard callbacks
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
            if (index >= sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) return NULL;
            
            const char* str = string_desc_arr[index];
            len = strlen(str);
            for (uint8_t i = 0; i < len; i++) {
                desc_str[1+i] = str[i];
            }
        }

        desc_str[0] = (TUSB_DESC_STRING << 8) | (2 + 2*len);
        return desc_str;
    }

    uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
        switch (instance) {
            case 0: return desc_hid_report_keyboard;
            case 1: return desc_hid_report_consumer;
            case 2: return desc_hid_report_media_control;
            case 3: return desc_hid_report_extra_buttons;
            default: return NULL;
        }
    }
}