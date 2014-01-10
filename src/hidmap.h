#ifndef HIDMAP_H
#define HIDMAP_H

#include <stdint.h>

typedef struct usbhiddev_s {
    struct libusb_device_handle *handle;
    uint8_t interface_nr;
    uint8_t input_ep;
    uint8_t input_psize;
} usbhiddev_t;

typedef struct hid_kbd_report_s {
    uint8_t modifier;
    uint8_t keycode;
} hid_kbd_report_t;

struct keymap_entry_s {
    hid_kbd_report_t input;
    int output;
};

#endif
