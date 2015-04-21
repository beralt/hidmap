/**
 * Start and stop Kodi through systemd/dbus
 */

#include <stdio.h>

/* dbus stuff */
#include "dbus/dbus.h"
#include "systemd_kodi.h"

DBusConnection *conn;

int systemd_kodi_init(void)
{
    DBusMessage *msg, *response;
    DBusError err;
    const char *unit_name = "kodi.service";
    const char *unit_mode = "replace";

    dbus_error_init(&err);

    conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if(dbus_error_is_set(&err)) {
        fprintf(stderr, "failed to open dbus: %s\n", err.message);
        return -1;
    }

    msg = dbus_message_new_method_call(
            "org.freedesktop.systemd1",
            "/org/freedesktop/systemd1",
            "org.freedesktop.systemd1.Manager",
            "GetUnit");

    if(!msg) {
        fprintf(stderr, "failed to create dbus message\n");
        return -1;
    }
    dbus_message_append_args(msg, DBUS_TYPE_STRING, &unit_name, DBUS_TYPE_INVALID);

    response = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);

    if(dbus_error_is_set(&err)) {
        fprintf(stderr, "failed to get systemd unit: %s\n", err.message);
        dbus_message_unref(msg);
        return -1;
    }

    dbus_message_unref(msg);
    dbus_message_unref(response);
    return 0;
}

int systemd_kodi_start(void)
{
    DBusMessage *msg, *response;
    DBusError err;

    dbus_error_init(&err);

    const char *unit_name = "kodi.service";
    const char *unit_mode = "replace";

    msg = dbus_message_new_method_call("org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager", "StartUnit");
    if(!msg) {
        fprintf(stderr, "failed to create dbus message\n");
        return -1;
    }
    dbus_message_append_args(msg, DBUS_TYPE_STRING, &unit_name, DBUS_TYPE_STRING, &unit_mode, DBUS_TYPE_INVALID);

    response = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
    
    if(dbus_error_is_set(&err)) {
        dbus_message_unref(msg);
        fprintf(stderr, "failed to start kodi: %s\n", err.message);
        return -1;
    }

    dbus_message_unref(msg);
    dbus_message_unref(response);
    return 0;
}

int systemd_kodi_stop(void)
{
    DBusMessage *msg, *response;
    DBusError err;

    dbus_error_init(&err);

    const char *unit_name = "kodi.service";
    const char *unit_mode = "replace";

    msg = dbus_message_new_method_call("org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager", "StopUnit");
    if(!msg) {
        fprintf(stderr, "failed to create dbus message\n");
        return -1;
    }
    dbus_message_append_args(msg, DBUS_TYPE_STRING, &unit_name, DBUS_TYPE_STRING, &unit_mode, DBUS_TYPE_INVALID);

    response = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
    
    if(dbus_error_is_set(&err)) {
        dbus_message_unref(msg);
        fprintf(stderr, "failed to stop kodi: %s\n", err.message);
        return -1;
    }

    dbus_message_unref(msg);
    dbus_message_unref(response);
    return 0;
}
