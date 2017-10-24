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
#include "custom_callbacks.h"

#define BUFSIZE 256

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
    struct DIALAppCallbacks cb_yt = {&youtube_start, &youtube_hide, &youtube_stop, &youtube_status};

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

