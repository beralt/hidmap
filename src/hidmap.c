/*
 * hidmap
 *
 * hidmap is a very simple attempt to map raw HID reports to scancodes.
 * based on hid-example.c in the linux kernel tree.
 * based on uinput-example from the linux tree
 */

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "libusb.h"
#include <linux/input.h>
#include <linux/uinput.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "hidmap.h"
#include "keymap.h"

/* defines */
#define DEVVID 0x6253
#define DEVPID 0x0100

/* globals */
int last_key_pressed = 0;

/* needed to find the device using udev */
int open_source_device(usbhiddev_t *dev);

int create_input_device(void);
int map_to_uinput(int fd, int modifier, int keycode);
void close_input_device(int fd);

int main(int argc, char *argv[])
{
    int ret, ufd, len, i;
    usbhiddev_t source = {NULL, 0, 0, 0};
    unsigned char buf[16];

    libusb_init(NULL);
    memset(buf, 0x0, sizeof(buf));

    if((ret = open_source_device(&source)) < 0) {
        fprintf(stderr, "failed to open source device: %s\n", libusb_error_name(ret));
        return 1;
    }
    fprintf(stderr, "device opened: endpoint 0x%hhx\n", source.input_ep);

    /* setup uinput */
    ufd = create_input_device();
    if(ufd < 0) {
        fprintf(stderr, "Failed to setup uinput: %d\n", ufd);
        return 1;
    }
    fprintf(stderr, "FD uinput %d\n", ufd);

    /* start reading from the interrupt endpoint input */
    while(1) {
        ret = libusb_interrupt_transfer(source.handle, source.input_ep, buf, source.input_psize, &len, 1800);
        if(ret != 0) {
            fprintf(stderr, "failed to read input interrupt endpoint: %s\n", libusb_error_name(ret));
            return 1;
        }

        fprintf(stderr, "got [%d]: ", len);
        for(i = 0; i < len; ++i)
            fprintf(stderr, "0x%hhx ", buf[i]);
        fprintf(stderr,"\n");

        map_to_uinput(ufd, buf[0], buf[2]);
    }

    close_input_device(ufd);
    return 0;
}

int open_source_device(usbhiddev_t *dev)
{
    libusb_device **list;
    ssize_t cnt, ret, i = 0, j = 0, k = 0;
    struct libusb_device_descriptor desc;
    struct libusb_config_descriptor *config;

    cnt = libusb_get_device_list(NULL, &list);
    if(cnt < 0) {
        fprintf(stderr, "found no usb devices\n");
        return cnt;
    }

    for(i = 0; i < cnt; ++i) {
        libusb_get_device_descriptor(list[i], &desc);
        if(desc.idProduct == DEVPID && desc.idVendor == DEVVID) {
            if(libusb_get_active_config_descriptor(list[i], &config) < 0) {
                libusb_free_device_list(list, 1);
                fprintf(stderr, "failed to get active configuration\n");
                return -1;
            }

            if((ret = libusb_open(list[i], &(dev->handle))) < 0) {
                libusb_free_device_list(list, 1);
                libusb_free_config_descriptor(config);
                return ret;
            }

            /* find the keyboard interface */
            fprintf(stderr, "device has %d interfaces\n", config->bNumInterfaces);
            for(j = 0; j < config->bNumInterfaces; ++j) {
                const struct libusb_interface *interface = &config->interface[j];
                fprintf(stderr, "interface has %d alternate settings\n", interface->num_altsetting);
                for(k = 0; k < interface->num_altsetting; ++k) {
                    const struct libusb_interface_descriptor *interface_desc = &interface->altsetting[k];
                    fprintf(stderr, "interface description 0x%hhx\n", interface_desc->bInterfaceClass);
                    fprintf(stderr, "interface protocol 0x%hhx\n", interface_desc->bInterfaceProtocol);
                    if(interface_desc->bInterfaceProtocol == 0x01) { // is it a keyboard?
                        if(libusb_kernel_driver_active(dev->handle, interface_desc->bInterfaceNumber) == 1) {
                            fprintf(stderr, "keyboard interface has driver attached\n");
                            if(libusb_detach_kernel_driver(dev->handle, interface_desc->bInterfaceNumber) < 0) {
                                libusb_close(dev->handle);
                                libusb_free_device_list(list, 1);
                                libusb_free_config_descriptor(config);
                                fprintf(stderr, "failed to detach kernel driver\n");
                                return -1;
                            }
                        }

                        if(libusb_claim_interface(dev->handle, interface_desc->bInterfaceNumber) < 0) {
                            libusb_close(dev->handle);
                            libusb_free_device_list(list, 1);
                            libusb_free_config_descriptor(config);
                            fprintf(stderr, "failed to claim interface \n");
                            return -1;
                        }

                        fprintf(stderr, "claimed interface\n");

                        /* setup endpoints */
                        /* had only one endpoint, hacky */
                        dev->input_ep = interface_desc->endpoint[0].bEndpointAddress;
                        dev->input_psize = interface_desc->endpoint[0].wMaxPacketSize;
                        libusb_free_config_descriptor(config);
                        libusb_free_device_list(list, 1);

                        return 0;
                    }
                }
            }
        }
    }

    fprintf(stderr, "source device not found\n");
    return -1;
}

/* create the input device using uinput */
int create_input_device(void)
{
    int fd, ret, i;
    struct uinput_user_dev uidev;

    fd = open("/dev/uinput", O_WRONLY|O_NONBLOCK);

    if(fd < 0) {
        perror("Failed to open uinput");
        return fd;
    }

    ret = ioctl(fd, UI_SET_EVBIT, EV_KEY);
    if(ret < 0) {
        perror("Failed to set EV_KEY on uinput");
        return -1;
    }

    ret = ioctl(fd, UI_SET_EVBIT, EV_SYN);
    if(ret < 0) {
        perror("Failed to set EV_SYN on uinput");
        return -1;
    }

    for(i = 0; i < KEYMAP_SIZE; ++i) {
        fprintf(stderr, "enabling key %hhx\n", keymap[i].output);
        ret = ioctl(fd, UI_SET_KEYBIT, keymap[i].output);
        if(ret < 0) {
            perror("Failed to set keybit on uinput");
            return ret;
        }
    }

    memset(&uidev, 0x0, sizeof(uidev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "hidmap-uinput");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor  = 0x1;
    uidev.id.product = 0x1;
    uidev.id.version = 1;

    ret = write(fd, &uidev, sizeof(uidev));
    if(ret < 0) {
        perror("Failed to write uinput device descriptor");
        return -1;
    }

    ret = ioctl(fd, UI_DEV_CREATE);
    if(ret < 0) {
        perror("Failed to ioctl UI_DEV_CREATE");
        return -1;
    }
        
    return fd;
}

int map_to_uinput(int fd, int modifier, int keycode)
{
    int ret, i;
    struct input_event ev;

    if(keycode == 0x00) {
        if(last_key_pressed > 0) {
            memset(&ev, 0x0, sizeof(ev));
            ev.type = EV_KEY;
            ev.code = last_key_pressed;
            ev.value = 0;
            ret = write(fd, &ev, sizeof(ev));
            if(ret < 0)
                perror("failed to write event to uinput");

            ev.type = EV_SYN;
            ev.code = SYN_REPORT;
            ev.value = 0;
            ret = write(fd, &ev, sizeof(ev));
            if(ret < 0)
                perror("failed to write event to uinput");

            last_key_pressed = 0;

            return ret;
        } else {
            /* this happens all the time */
            return 0;
        }
    }

    memset(&ev, 0x0, sizeof(ev));

    for(i = 0; i < KEYMAP_SIZE; ++i) {
        if(keymap[i].input.modifier == modifier && keymap[i].input.keycode == keycode) {
            fprintf(stderr, "got %hhx\n", keymap[i].output);
            ev.type = EV_KEY;
            ev.code = last_key_pressed = keymap[i].output;
            ev.value = 1;
            ret = write(fd, &ev, sizeof(ev));
            if(ret < 0)
                perror("failed to write event to uinput");

            ev.type = EV_SYN;
            ev.code = SYN_REPORT;
            ev.value = 0;
            ret = write(fd, &ev, sizeof(ev));
            if(ret < 0)
                perror("failed to write event to uinput");

            return ret;
        }
    }

    return 0;
}

void close_input_device(int fd)
{
    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
}
