#include <glib.h>

#include <wydbus.h>

#include "custom_callbacks.h"

#include "com/wyplay/dial/notify/c_adaptor.h"

#define DBUS_SRV_NAME "com.wyplay.wlauncher"
#define DBUS_OBJ_PATH "/com/wyplay/wlauncher/browser"
#define DBUS_IFACE_NAME "com.wyplay.wlauncher.browser"

#define DIAL_DBUS_PATH "/com/wyplay/dial"
//#define DBUS_STANDALONE

typedef struct {
    wydbus_t *bus;
    wydbus_ref_t *ref_browser;
    struct {
        void (*load_url) (char *);
        char * (*get_url) (void);
    } api;

    dial_notify_adaptor_t *adaptor;
} ctx_t;

ctx_t dbus_ctx;

int init_dbus(void)
{
    memset(&dbus_ctx, 0, sizeof(ctx_t));
    dbus_ctx.bus = wydbus_new(DBUS_BUS_SYSTEM, "");
    dbus_ctx.ref_browser = wydbus_reference2(dbus_ctx.bus, DBUS_SRV_NAME, DBUS_OBJ_PATH, DBUS_IFACE_NAME);

    dbus_ctx.api.load_url = wydbus_method(dbus_ctx.ref_browser, "load_url", "s", "", NULL);
    dbus_ctx.api.get_url = wydbus_method(dbus_ctx.ref_browser, "get_url", "", "s", NULL);

    dbus_ctx.adaptor = dial_notify_adaptor_create();
    if (!dbus_ctx.adaptor)
        return -1;
    if (dial_notify_adaptor_register(dbus_ctx.adaptor, dbus_ctx.bus, DIAL_DBUS_PATH, NULL)) {
        return -2;
    }

    wydbus_run(dbus_ctx.bus);
    return 0;
}

void free_dbus(void)
{
    wydbus_exit(dbus_ctx.bus);
    wydbus_free_proxy(dbus_ctx.api.load_url);
    wydbus_free_proxy(dbus_ctx.api.get_url);
    dial_notify_adaptor_delete(dbus_ctx.adaptor);
    wydbus_free_reference(dbus_ctx.ref_browser);
    wydbus_free(dbus_ctx.bus);
}

/*
 * This function will walk /proc and look for the application in
 * /proc/<PID>/comm. and /proc/<PID>/cmdline to find it's command (executable
 * name) and command line (if needed).
 * Implementors can override this function with an equivalent.
 */
int isAppRunning( char *pzName, char *pzCommandPattern ) {
    char *current_url = NULL;
    if (strncmp(pzName, spAppYouTube, strlen(spAppYouTube)) == 0) {
        current_url = dbus_ctx.api.get_url();
        return !strncmp(current_url, spAppYouTube, strlen(spAppYouTube));
    }
    return 0;
}

DIALStatus youtube_start(DIALServer *ds, const char *appname,
                                const char *payload, const char *additionalDataUrl,
                                DIAL_run_t *run_id, void *callback_data) {
    printf("\n\n ** LAUNCH YouTube **\n\n");

    char url[512] = {0,};
    if (strlen(payload) && strlen(additionalDataUrl)){
        snprintf( url, 512, "https://www.youtube.com/tv?%s&%s", payload, additionalDataUrl);
    }else if (strlen(payload)){
        snprintf( url, 512, "https://www.youtube.com/tv?%s", payload);
    }else{
        snprintf( url, 512, "https://www.youtube.com/tv");
    }

#ifdef DBUS_STANDALONE
    dbus_ctx.api.load_url(url);
#else
    dbus_ctx.adaptor->signals.start_requested(appname, payload, additionalDataUrl);
#endif

    return kDIALStatusRunning;
}

DIALStatus youtube_hide(DIALServer *ds, const char *app_name,
                                        DIAL_run_t *run_id, void *callback_data)
{
    return (isAppRunning( spAppYouTube, spAppYouTubeMatch )) ? kDIALStatusRunning : kDIALStatusStopped;
}

DIALStatus youtube_status(DIALServer *ds, const char *appname,
                                 DIAL_run_t run_id, int *pCanStop, void *callback_data) {
    // YouTube can stop
    *pCanStop = 1;
    return isAppRunning( spAppYouTube, spAppYouTubeMatch ) ? kDIALStatusRunning : kDIALStatusStopped;
}

void youtube_stop(DIALServer *ds, const char *appname, DIAL_run_t run_id,
                         void *callback_data) {
    printf("\n\n ** KILL YouTube **\n\n");
    if ((isAppRunning( spAppYouTube, spAppYouTubeMatch ))) {
#ifdef DBUS_STANDALONE
        dbus_ctx.api.load_url("about:blank");
#else
        dbus_ctx.adaptor->signals.stop_requested(appname);
#endif
    }
}
