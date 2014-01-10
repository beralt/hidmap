HIDMAP
================================

This small utility allows the use of HID USB devices without any kernel driver. To do so it uses a user-space driver with the help of libusb(x). It detaches existing kernel drivers.

Why?
----

I was using a TwinHan receiver, but the kernel driver is buggy and I did not want to write another kernel driver for it. Using lirc was another option, but I seriously lost track of how to configure it without having to blacklist the kernel driver (yes it does work a bit, mapping some buttons). I know about kernel keymaps, but that only works if you have a good driver.


Known stuff
-----------

There are a few shortcuts, i.e. the enumeration of the usb device. There is no config file, see keymap.h. There might be some memory leaks on certain code paths. Probably some other bugs too.
