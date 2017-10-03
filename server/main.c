/*
 * Copyright (c) 2014 Netflix, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETFLIX, INC. AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NETFLIX OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <regex.h>

#include "dial_server.h"
#include "dial_options.h"
#include <signal.h>
#include <stdbool.h>

#include "url_lib.h"
#include "nf_callbacks.h"

#define BUFSIZE 256

#include <wydbus.h>
#include <glib.h>
#define DBUS_SRV_NAME "com.wyplay.wlauncher"
#define DBUS_OBJ_PATH "/com/wyplay/wlauncher/browser"
#define DBUS_IFACE_NAME "com.wyplay.wlauncher.browser"

typedef struct {
    wydbus_t *bus;
    wydbus_ref_t *ref_browser;
    struct {
        void (*load_url) (char *);
        char * (*get_url) (void);

        int (*open_layer) (int, bool, GArray*);
        void (*close_layer) (int);
        void (*set_layer_url) (int, char*);
    } api;

    int active_layer;
} ctx_t;

ctx_t dbus_ctx;

int init_dbus(void)
{
    dbus_ctx.bus = wydbus_new(DBUS_BUS_SYSTEM, "");
    dbus_ctx.ref_browser = wydbus_reference2(dbus_ctx.bus, DBUS_SRV_NAME, DBUS_OBJ_PATH, DBUS_IFACE_NAME);

    dbus_ctx.api.load_url = wydbus_method(dbus_ctx.ref_browser, "load_url", "s", "", NULL);
    dbus_ctx.api.get_url = wydbus_method(dbus_ctx.ref_browser, "get_url", "", "s", NULL);

    dbus_ctx.api.open_layer = wydbus_method(dbus_ctx.ref_browser, "open_layer", "ibas", "i", NULL);
    dbus_ctx.api.close_layer = wydbus_method(dbus_ctx.ref_browser, "close_layer", "i", "", NULL);
    dbus_ctx.api.set_layer_url = wydbus_method(dbus_ctx.ref_browser, "set_layer_url", "is", "", NULL);

    dbus_ctx.active_layer = -1;
    wydbus_run(dbus_ctx.bus);
    return 0;
}

void free_dbus(void)
{
    wydbus_exit(dbus_ctx.bus);
    wydbus_free_proxy(dbus_ctx.api.open_layer);
    wydbus_free_proxy(dbus_ctx.api.close_layer);
    wydbus_free_proxy(dbus_ctx.api.set_layer_url);
    wydbus_free_reference(dbus_ctx.ref_browser);
    wydbus_free(dbus_ctx.bus);
}

char *spAppNetflix = "netflix";      // name of the netflix executable
static char *spDefaultNetflix = "../../../src/platform/qt/netflix";
static char *spDefaultData="../../../src/platform/qt/data";
static char *spNfDataDir = "NF_DATA_DIR=";
static char *spDefaultFriendlyName = "DIAL server sample";
static char *spDefaultModelName = "NOT A VALID MODEL NAME";
static char *spDefaultUuid = "deadbeef-dead-beef-dead-beefdeadbeef";
static char spDataDir[BUFSIZE];
char spNetflix[BUFSIZE];
static char spFriendlyName[BUFSIZE];
static char spModelName[BUFSIZE];
static char spUuid[BUFSIZE];
extern bool wakeOnWifiLan;
static int gDialPort;

static char *spAppYouTube = "https://www.youtube.com/";
static char *spAppYouTubeMatch = "chrome.*google-chrome-dial";
static int spAppYouTubeZOrder = 80;

static char *spDefaultApp = "file:///usr/share/webapps/transparent-body/index.html";

int doesMatch( char* pzExp, char* pzStr)
{
    regex_t exp;
    int ret;
    int match = 0;
    if ((ret = regcomp( &exp, pzExp, REG_EXTENDED ))) {
        char errbuf[1024] = {0,};
        regerror(ret, &exp, errbuf, sizeof(errbuf));
        fprintf( stderr, "regexp error: %s", errbuf );
    } else {
        regmatch_t matches[1];
        if( regexec( &exp, pzStr, 1, matches, 0 ) == 0 ) {
            match = 1;
        }
    }
    regfree(&exp);
    return match;
}

void signalHandler(int signal)
{
    switch(signal)
    {
        case SIGTERM:
            // just ignore this, we don't want to die
            break;
    }
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

pid_t runApplication( const char * const args[], DIAL_run_t *run_id ) {
  pid_t pid = fork();
  if (pid != -1) {
    if (!pid) { // child
      putenv(spDataDir);
      printf("Execute:\n");
      for(int i = 0; args[i]; ++i) {
        printf(" %d) %s\n", i, args[i]);
      }
      if( execv(*args, (char * const *) args) == -1) {
		printf("%s failed to launch\n", *args);
		perror("Failed to Launch \n");
      }
    } else {
      *run_id = (void *)(long)pid; // parent PID
    }
    return kDIALStatusRunning;
  } else {
    return kDIALStatusStopped;
  }
}


/* Compare the applications last launch parameters with the new parameters.
 * If they match, return false
 * If they don't match, return true
 */
int shouldRelaunch(
    DIALServer *pServer,
    const char *pAppName,
    const char *args )
{
    return ( strncmp( DIAL_get_payload(pServer, pAppName), args, DIAL_MAX_PAYLOAD ) != 0 );
}

static DIALStatus youtube_start(DIALServer *ds, const char *appname,
                                const char *payload, const char *additionalDataUrl,
                                DIAL_run_t *run_id, void *callback_data) {
    printf("\n\n ** LAUNCH YouTube ** with\n - payload: '%s'\n - additionalDataUrl: '%s'\n\n", payload, additionalDataUrl);

    char url[512] = {0,};
    if (strlen(payload) && strlen(additionalDataUrl)){
        sprintf( url, "https://www.youtube.com/tv?%s&%s", payload, additionalDataUrl);
    }else if (strlen(payload)){
        sprintf( url, "https://www.youtube.com/tv?%s", payload);
    }else{
        sprintf( url, "https://www.youtube.com/tv");
    }

    int layer_id = -1;
    GArray *forward_keys = g_array_new(FALSE, TRUE, sizeof(char*));
    char *key = g_strdup("f4"); /* standby */
    g_array_append_val(forward_keys, key);

    /* layer API
    if (dbus_ctx.active_layer < 0) {
        layer_id = dbus_ctx.api.open_layer(spAppYouTubeZOrder, true, forward_keys);
    }
    else {
        layer_id = dbus_ctx.active_layer;
    }

    dbus_ctx.api.set_layer_url(layer_id, url);
    dbus_ctx.active_layer = layer_id;
    */
    dbus_ctx.api.load_url(url);

    g_array_free(forward_keys, TRUE);
    g_free(key);

    return kDIALStatusRunning;
}

static DIALStatus youtube_hide(DIALServer *ds, const char *app_name,
                                        DIAL_run_t *run_id, void *callback_data)
{
    return (isAppRunning( spAppYouTube, spAppYouTubeMatch )) ? kDIALStatusRunning : kDIALStatusStopped;
}
        
static DIALStatus youtube_status(DIALServer *ds, const char *appname,
                                 DIAL_run_t run_id, int *pCanStop, void *callback_data) {
    // YouTube can stop
    *pCanStop = 1;
    return isAppRunning( spAppYouTube, spAppYouTubeMatch ) ? kDIALStatusRunning : kDIALStatusStopped;
}

static void youtube_stop(DIALServer *ds, const char *appname, DIAL_run_t run_id,
                         void *callback_data) {
    printf("\n\n ** KILL YouTube **\n\n");
    if ((isAppRunning( spAppYouTube, spAppYouTubeMatch ))) {
        /* layer API
        dbus_ctx.api.close_layer(dbus_ctx.active_layer);
        dbus_ctx.active_layer = -1;
        */
        dbus_ctx.api.load_url(spDefaultApp);
    }
}

void run_ssdp(int port, const char *pFriendlyName, const char * pModelName, const char *pUuid);

static void printUsage()
{
    int i, numberOfOptions = sizeof(gDialOptions) / sizeof(dial_options_t);
    printf("usage: dialserver <options>\n");
    printf("options:\n");
    for( i = 0; i < numberOfOptions; i++ )
    {
        printf("        %s|%s [value]: %s\n",
            gDialOptions[i].pOption,
            gDialOptions[i].pLongOption,
            gDialOptions[i].pOptionDescription );
    }
}

static void setValue( char * pSource, char dest[] )
{
    // Destination is always one of our static buffers with size BUFSIZE
    memset( dest, 0, BUFSIZE );
    memcpy( dest, pSource, strlen(pSource) );
}

static void setDataDir(char *pData)
{
    setValue( spNfDataDir, spDataDir );
    strcat(spDataDir, pData);
}

void runDial(void)
{
    DIALServer *ds;
    ds = DIAL_create();
    struct DIALAppCallbacks cb_yt = {youtube_start, youtube_hide, youtube_stop, youtube_status};

    DIAL_register_app(ds, "YouTube", &cb_yt, NULL, 1, ".youtube.com");
    DIAL_start(ds);

    gDialPort = DIAL_get_port(ds);
    printf("launcher listening on gDialPort %d\n", gDialPort);
    run_ssdp(gDialPort, spFriendlyName, spModelName, spUuid);

    DIAL_stop(ds);
    free(ds);
}

static void processOption( int index, char * pOption )
{
    switch(index)
    {
    case 0: // Data path
        memset( spDataDir, 0, sizeof(spDataDir) );
        setDataDir( pOption );
        break;
    case 1: // Netflix path
        setValue( pOption, spNetflix );
        break;
    case 2: // Friendly name
        setValue( pOption, spFriendlyName );
        break;
    case 3: // Model Name
        setValue( pOption, spModelName );
        break;
    case 4: // UUID
        setValue( pOption, spUuid );
        break;
    case 5:
        if (strcmp(pOption, "on")==0){
            wakeOnWifiLan=true;
        }else if (strcmp(pOption, "off")==0){
            wakeOnWifiLan=false;
        }else{
            fprintf(stderr, "Option %s is not valid for %s",
                    pOption, WAKE_OPTION_LONG);
            exit(1);
        }
        break;
    default:
        // Should not get here
        fprintf( stderr, "Option %d not valid\n", index);
        exit(1);
    }
}

int main(int argc, char* argv[])
{
    struct sigaction action;
    action.sa_handler = signalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    init_dbus();
    sigaction(SIGTERM, &action, NULL);

    srand(time(NULL));
    int i;
    i = isAppRunning( spAppYouTube, spAppYouTubeMatch );
    printf("YouTube is %s\n", i ? "Running":"Not Running");

    // set all defaults
    setValue(spDefaultFriendlyName, spFriendlyName );
    setValue(spDefaultModelName, spModelName );
    setValue(spDefaultUuid, spUuid );
    setValue(spDefaultNetflix, spNetflix );
    setDataDir(spDefaultData);

    // Process command line options
    // Loop through pairs of command line options.
    for( i = 1; i < argc; i+=2 )
    {
        int numberOfOptions = sizeof(gDialOptions) / sizeof(dial_options_t);
        while( --numberOfOptions >= 0 )
        {
            int shortLen, longLen;
            shortLen = strlen(gDialOptions[numberOfOptions].pOption);
            longLen = strlen(gDialOptions[numberOfOptions].pLongOption);
            if( ( ( strncmp( argv[i], gDialOptions[numberOfOptions].pOption, shortLen ) == 0 ) ||
                ( strncmp( argv[i], gDialOptions[numberOfOptions].pLongOption, longLen ) == 0 ) ) &&
                ( (i+1) < argc ) )
            {
                processOption( numberOfOptions, argv[i+1] );
                break;
            }
        }
        // if we don't find an option in our list, bail out.
        if( numberOfOptions < 0 )
        {
            printUsage();
            exit(1);
        }
    }
    runDial();

    free_dbus();
    return 0;
}

