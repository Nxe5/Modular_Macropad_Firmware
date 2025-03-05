// include/usb_descriptors.h

#ifndef _USB_DESCRIPTORS_H_
#define _USB_DESCRIPTORS_H_

// Report IDs
#define REPORT_ID_KEYBOARD    1
#define REPORT_ID_MOUSE       2

// Interface numbers
enum {
    ITF_NUM_HID = 0,
    ITF_NUM_TOTAL
};

// Endpoint numbers
enum {
    EPNUM_DEFAULT = 0,
    EPNUM_HID_DATA,
};

#endif /* _USB_DESCRIPTORS_H_ */