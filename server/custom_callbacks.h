#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "dial_server.h"

int init_dbus(void);
void free_dbus(void);

int isAppRunning(char *pzName, char *pzCommandPattern);

DIALStatus youtube_start(DIALServer *ds, const char *appname,
                         const char *payload, const char *additionalDataUrl,
                         DIAL_run_t *run_id, void *callback_data);

DIALStatus youtube_hide(DIALServer *ds, const char *app_name,
                        DIAL_run_t *run_id, void *callback_data);

DIALStatus youtube_status(DIALServer *ds, const char *appname,
                          DIAL_run_t run_id, int *pCanStop, void *callback_data);

void youtube_stop(DIALServer *ds, const char *appname, DIAL_run_t run_id,
                  void *callback_data);

static char *spAppYouTube = "https://www.youtube.com/";
static char *spAppYouTubeMatch = "chrome.*google-chrome-dial";
