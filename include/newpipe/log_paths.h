#pragma once
#include <sys/stat.h>
#include <stdio.h>

#define KAITUBE_LOG_DIR "sdmc:/switch/KaiTube-Native/logs"
#define KAITUBE_APP_LOG KAITUBE_LOG_DIR "/app.log"
#define KAITUBE_UI_LOG KAITUBE_LOG_DIR "/ui.log"
#define KAITUBE_BOOT_LOG KAITUBE_LOG_DIR "/boot.log"
#define KAITUBE_STDERR_LOG KAITUBE_LOG_DIR "/stderr.log"
#define KAITUBE_CRASH_LOG KAITUBE_LOG_DIR "/abort.log"

static inline void kaitubeEnsureLogDirectory(void)
{
    mkdir("sdmc:/switch", 0777);
    mkdir("sdmc:/switch/KaiTube-Native", 0777);
    mkdir(KAITUBE_LOG_DIR, 0777);
}

static inline void kaitubeRotateLogs(void)
{
    static const char* names[]={"app.log","ui.log","boot.log","stderr.log","abort.log"};
    mkdir(KAITUBE_LOG_DIR "/previous",0777);
    for(unsigned i=0;i<5;++i){
        char source[160],target[180];
        snprintf(source,sizeof(source),"%s/%s",KAITUBE_LOG_DIR,names[i]);
        snprintf(target,sizeof(target),"%s/previous/%s",KAITUBE_LOG_DIR,names[i]);
        remove(target); rename(source,target);
    }
}
#ifdef __cplusplus
extern "C" {
#endif
void kaitubeInitAbortLog(void);
#ifdef __cplusplus
}
#endif
