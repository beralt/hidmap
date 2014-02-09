#include <stdio.h>

#include "config.h"

#define DEBUG
#ifdef DEBUG
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define LOG(...) do {} while (0)
#endif

#ifdef HAVE_LIBBCM_HOST
/**
 * Based on the RPi userland app named
 * tvservice.
 */

#include "interface/vmcs_host/vc_tvservice.h"
#include "vc_tvservice.h"

VCHI_INSTANCE_T vchi_instance;
VCHI_CONNECTION_T *vchi_connection;

int vc_tvservice_init()
{
    int res;

    vcos_init();

    if((res = vchi_initialise(&vchi_instance)) != 0) {
        LOG("failed to initialize VCHI\n");
        return -1;
    }

    if((res = vchi_connect(NULL, 0, vchi_instance)) != 0) {
        LOG("failed to connect to VCHI\n");
        return -1;
    }

    vc_vchi_tv_init(vchi_instance, &vchi_connection, 1);
}

void vc_tvservice_stop()
{
    vc_vchi_tv_stop();
    vchi_disconnect(vchi_instance);
}

int vc_tvservice_poweron()
{
    int res;

    if((res = vc_tv_hdmi_power_on_preferred()) != 0) {
        LOG("Failed to power on TV with preferred settings: %d\n", res);
        return res;
    }

    return 0;
}

int vc_tvservice_poweroff()
{
    int res;

    if((res = vc_tv_power_off()) != 0) {
        LOG("Failed to power off TV: %d\n", res);
        return res;
    }

    return 0;
}
#endif
