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
#include <signal.h>

#include "libusb.h"
#include <linux/input.h>
#include <linux/uinput.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "config.h"
#include "hidmap.h"
#include "keymap.h"
#include "systemd_xbmc.h" // in order to stop/start xbmc

/* optional stuff for turning HDMI link on/off */
#ifdef HAVE_LIBBCM_HOST
#include "vc_tvservice.h"
#endif

/* defines */
#define DEBUG
#define DEVVID 0x6253
#define DEVPID 0x0100
#ifdef DEBUG
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define LOG(...) do {} while (0)
#endif

#define FIX_XBMC // use this to fix 20 key min bug in XBMC

/* globals */
int last_key_pressed = 0;
int xbmc_status = 0; // start with xbmc off

/* needed to find the device using udev */
int open_source_device(usbhiddev_t *dev);

int create_input_device(void);
int map_to_uinput(int fd, int modifier, int keycode);
void close_input_device(int fd);

void sighandler(int sig)
{
    //
    return;
}

static void daemonize(void)
{
    pid_t pid;
    pid = fork();
    if(pid < 0)
        exit(EXIT_FAILURE);
    if(pid > 0)
        exit(EXIT_SUCCESS);
    if(setsid() < 0)
        exit(EXIT_FAILURE);
    signal(SIGCHLD, sighandler);
    signal(SIGHUP, sighandler);
    pid = fork();
    if(pid < 0)
        exit(EXIT_FAILURE);
    if(pid > 0)
        exit(EXIT_SUCCESS);
    umask(0);
    chdir("/");
    int fd;
    for(fd = sysconf(_SC_OPEN_MAX); fd > 0; fd--)
        close(fd);
    //openlog("hidmap", LOG_PID, LOG_DAEMON);
}

static void parse_cmdline(int argc, char *argv[])
{
    int opt;

    while((opt = getopt(argc, argv, "d")) != -1) {
        switch(opt) {
            case 'd':
                daemonize();
                break;
            default:
                fprintf(stderr, "Usage %s [-d]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char *argv[])
{
    int ret, ufd, len;
    usbhiddev_t source = {NULL, 0, 0, 0};
    unsigned char buf[16];

    parse_cmdline(argc, argv);

    libusb_init(NULL);
    memset(buf, 0x0, sizeof(buf));

    if((ret = open_source_device(&source)) < 0) {
        LOG("failed to open source device: %s\n", libusb_error_name(ret));
        return 1;
    }

    /* setup uinput */
    ufd = create_input_device();
    if(ufd < 0) {
        LOG("failed to setup uinput: %d\n", ufd);
        return 1;
    }

    /* init dbus stuff */
    if((ret = systemd_xbmc_init()) < 0) {
        LOG("failed to initialize dbus/systemd, power key will not start stop XBMC\n");
    }

#ifdef HAVE_LIBBCM_HOST
    /* initialise all the Broadcom VideoCore stuff */
    vc_tvservice_init();
#endif

    /* start reading from the interrupt endpoint input */
    while(1) {
        ret = libusb_interrupt_transfer(source.handle, source.input_ep, buf, source.input_psize, &len, 1800);
        if(ret != 0)
            LOG("failed to read input interrupt endpoint: %s\n", libusb_error_name(ret));
        else
            map_to_uinput(ufd, buf[0], buf[2]);
    }

    close_input_device(ufd);

#ifdef HAVE_LIBBCM_HOST
    /* close VideoCore stuff */
    vc_tvservice_stop();
#endif

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
        LOG("found no usb devices\n");
        return cnt;
    }

    for(i = 0; i < cnt; ++i) {
        libusb_get_device_descriptor(list[i], &desc);
        if(desc.idProduct == DEVPID && desc.idVendor == DEVVID) {
            if(libusb_get_active_config_descriptor(list[i], &config) < 0) {
                libusb_free_device_list(list, 1);
                LOG("failed to get active configuration\n");
                return -1;
            }

            if((ret = libusb_open(list[i], &(dev->handle))) < 0) {
                libusb_free_device_list(list, 1);
                libusb_free_config_descriptor(config);
                return ret;
            }

            /* find the keyboard interface */
            for(j = 0; j < config->bNumInterfaces; ++j) {
                const struct libusb_interface *interface = &config->interface[j];
                for(k = 0; k < interface->num_altsetting; ++k) {
                    const struct libusb_interface_descriptor *interface_desc = &interface->altsetting[k];
                    if(interface_desc->bInterfaceProtocol == 0x01) { // is it a keyboard?
                        if(libusb_kernel_driver_active(dev->handle, interface_desc->bInterfaceNumber) == 1) {
                            LOG("keyboard interface has driver attached, detaching...\n");
                            if(libusb_detach_kernel_driver(dev->handle, interface_desc->bInterfaceNumber) < 0) {
                                libusb_close(dev->handle);
                                libusb_free_device_list(list, 1);
                                libusb_free_config_descriptor(config);
                                LOG("failed to detach kernel driver\n");
                                return -1;
                            }
                        }

                        if(libusb_claim_interface(dev->handle, interface_desc->bInterfaceNumber) < 0) {
                            libusb_close(dev->handle);
                            libusb_free_device_list(list, 1);
                            libusb_free_config_descriptor(config);
                            LOG("failed to claim interface \n");
                            return -1;
                        }

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

    LOG("source device not found\n");
    return -1;
}

/* create the input device using uinput */
int create_input_device(void)
{
    int fd, ret, i;
    struct uinput_user_dev uidev;

    fd = open("/dev/uinput", O_WRONLY|O_NONBLOCK);

    if(fd < 0) {
        perror("failed to open uinput");
        return fd;
    }

    ret = ioctl(fd, UI_SET_EVBIT, EV_KEY);
    if(ret < 0) {
        perror("failed to set EV_KEY on uinput");
        return -1;
    }

    ret = ioctl(fd, UI_SET_EVBIT, EV_SYN);
    if(ret < 0) {
        perror("failed to set EV_SYN on uinput");
        return -1;
    }

#ifdef FIX_XBMC
    for(i = 0; i < KEY_MAX; ++i)
        ret = ioctl(fd, UI_SET_KEYBIT, i);
#else
    for(i = 0; i < KEYMAP_SIZE; ++i) {
        LOG("enabling key %hhx\n", keymap[i].output);
        ret = ioctl(fd, UI_SET_KEYBIT, keymap[i].output);
        if(ret < 0) {
            perror("failed to set keybit on uinput");
            return ret;
        }
    }
#endif

    memset(&uidev, 0x0, sizeof(uidev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "hidmap-uinput");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor  = 0x1;
    uidev.id.product = 0x1;
    uidev.id.version = 1;

    ret = write(fd, &uidev, sizeof(uidev));
    if(ret < 0) {
        perror("failed to write uinput device descriptor");
        return -1;
    }

    ret = ioctl(fd, UI_DEV_CREATE);
    if(ret < 0) {
        perror("failed to ioctl UI_DEV_CREATE");
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

    /* some keycodes are trapped here (i.e. the power button) */
    if(keycode == 0x3f && modifier == 0x5) {
        /* start/stop XBMC */
        if(xbmc_status > 0) {
            LOG("stopping XBMC\n");
            if(systemd_xbmc_stop() < 0) {
                LOG("failed to stop XBMC\n");
                return -1;
            }
#ifdef HAVE_LIBBCM_HOST
            vc_tvservice_poweroff();
#endif
            xbmc_status = 0;
        } else {
#ifdef HAVE_LIBBCM_HOST
            vc_tvservice_poweron();
#endif
            LOG("starting XBMC\n");
            if(systemd_xbmc_start() < 0) {
                LOG("failed to start XBMC\n");
                return -1;
            }
            xbmc_status = 1;
        }
        return 0;
    }

    memset(&ev, 0x0, sizeof(ev));

    for(i = 0; i < KEYMAP_SIZE; ++i) {
        if(keymap[i].input.modifier == modifier && keymap[i].input.keycode == keycode) {
            LOG("got %hhx\n", keymap[i].output);
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

    LOG("had no mapping for modifier 0x%x and keycode 0x%x\n", modifier, keycode);

    return 0;
}

void close_input_device(int fd)
{
    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
}
