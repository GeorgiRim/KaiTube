/*
    Copyright 2019 natinusala
    Copyright 2019 WerWolv
    Copyright 2019 p-sam

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <stdio.h>
#include <switch.h>
#include <unistd.h>
#include "newpipe/log_paths.h"

static int nxlink_sock = -1;

static void boot_record(const char* stage, Result rc)
{
    FILE* log = fopen(KAITUBE_BOOT_LOG, "a");
    if (log) {
        fprintf(log, "%s: 0x%08lx\n", stage, (unsigned long)rc);
        fclose(log);
    }
}
#define INIT_LOGGED(call) do { \
    boot_record(#call " begin", 0xffffffffU); \
    Result boot_rc = (call); \
    boot_record(#call " end", boot_rc); \
} while (0)


void userAppInit()
{
    printf("userAppInit\n");
    kaitubeEnsureLogDirectory();
    kaitubeRotateLogs();
    FILE* boot = fopen(KAITUBE_BOOT_LOG, "w");
    if (boot) fclose(boot);
    INIT_LOGGED(appletLockExit());

    // Init network
    SocketInitConfig cfg = *(socketGetDefaultInitConfig());
    AppletType at        = appletGetAppletType();
    if (at == AppletType_Application || at == AppletType_SystemApplication)
    {
        cfg.num_bsd_sessions = 12; // default is 3
        cfg.sb_efficiency    = 8; // default is 4
        INIT_LOGGED(socketInitialize(&cfg));
    }
    else
    {
        cfg.num_bsd_sessions = 2;
        cfg.sb_efficiency    = 1;
        INIT_LOGGED(socketInitialize(&cfg));
    }

#ifdef DEBUG
    nxlink_sock = nxlinkStdio();
#endif

    INIT_LOGGED(romfsInit());
    INIT_LOGGED(plInitialize(PlServiceType_User));
    INIT_LOGGED(setsysInitialize());
    INIT_LOGGED(setInitialize());
    INIT_LOGGED(psmInitialize());
    INIT_LOGGED(nifmInitialize(NifmServiceType_User));
    INIT_LOGGED(lblInitialize());
}

void userAppExit()
{
    printf("userAppExit\n");

    // backlight
    lblExit();
    // network state
    nifmExit();
    // power state
    psmExit();
    // system language...
    setExit();
    // system theme, system version...
    setsysExit();
    // system font
    plExit();

    romfsExit();

    if (nxlink_sock != -1)
        close(nxlink_sock);

    socketExit();

    appletUnlockExit();
}
