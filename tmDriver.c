/*********************************************************************
    Tiny Mouse Driver without X
    written by Koji Iigura 2019.
    please see the README.md.
*********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/inotify.h>
#include <linux/input.h>

#include "tmDriver.h"

static volatile int gMinX=0;
static volatile int gMinY=0;
static volatile int gMaxX=0;
static volatile int gMaxY=0;

static volatile int gMouseX=0;
static volatile int gMouseY=0;

static volatile int gMouseButtonStatus=0;

pthread_mutex_t gMutexForXY=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t gMutexForButtonStatus=PTHREAD_MUTEX_INITIALIZER;

#define kMaxMonitorThread (256*2)
typedef struct {
    int elementIndex;
    pthread_t thread;
    char *eventFileName;    // "eventN"
    FILE *fp;
} MonitorInfo;
static MonitorInfo gMonitorArray[kMaxMonitorThread];
pthread_mutex_t gMutexForMonitorArray=PTHREAD_MUTEX_INITIALIZER;

pthread_t gHotPlugMonitorThread;

volatile bool gInitializing=false;

static bool startMonitorForHotPlug();
static void *hotPlugMonitorFunc(void *inDummy);
static bool disposeMonitorThread(const char *inEventFile);
static bool isAlreadyMonitored(const char *inEventFile);
static MonitorInfo *getMonitorSlot(const char *inEventFile);
static struct inotify_event *getEvent(FILE *inINotifyFP);
static bool createMonitorForAlreadyPlugged();
static bool isEventFile(const char *inFileName);
static bool createMonitorThread(const char *inEventFileName);
static MonitorInfo *getEmptyMonitorArraySlot();
static void *mouseWatcherFunc(void *inMonitorInfo);
static void resetMonitorInfo(MonitorInfo *inMonitorInfo);
static bool initMonitorArray();
static int clamp(int inMin,int inMax,int inValue);

bool tmDrvrInit(int inMinX,int inMinY,int inMaxX,int inMaxY) {
    if(tmDrvrSetArea(inMinX,inMinY,inMaxX,inMaxY)==false) {
        return false;
    }
    if(tmDrvrSetPosition(0,0)==false) {
        return false;
    }
    gMouseButtonStatus=0;
    if(initMonitorArray()==false) {
        return false;
    }
    gInitializing=true;
        if(startMonitorForHotPlug()==false) {
            return false;
        }
        if(createMonitorForAlreadyPlugged()==false) {
            return false;
        }
    gInitializing=false;

    return true;
}
static bool initMonitorArray() {
    int result=pthread_mutex_lock(&gMutexForMonitorArray);
    if( result ) {
        return false;    // can not lock
    }
        int i;
        for(i=0; i<kMaxMonitorThread; i++) {
            gMonitorArray[i].elementIndex=i;
            gMonitorArray[i].eventFileName=NULL;
            gMonitorArray[i].fp=NULL;
        }
    pthread_mutex_unlock(&gMutexForMonitorArray);
    return true;
}
static bool startMonitorForHotPlug() {
    int inotify=inotify_init();
    if(inotify==-1) {
        fprintf(stderr,"libtmdrvr: can not init notification.\n");
        return false;
    }
    int wd=inotify_add_watch(inotify,"/dev/input/",IN_ALL_EVENTS);
    if(wd==-1) {
        fprintf(stderr,"libtmdrvr: can not watch /dev/input/\n");
        return false;
    }
    // fprintf(stderr,"startMonitorForHotPlug: wd=%d\n",wd);
    FILE *inotifyFP=fdopen(inotify,"rb");
    if(inotifyFP==NULL) {
        fprintf(stderr,"libtmdrvr: can not convert to file pointer.\n");
        return false;
    }   

    if( pthread_create(&gHotPlugMonitorThread,NULL,hotPlugMonitorFunc,inotifyFP) ) {
        fprintf(stderr,"libtmdrvr: can not start hotplug-monitor thread.\n");
        return false;
    }

    return true;
}
static void *hotPlugMonitorFunc(void *inInotifyFP) {
    FILE *inotifyFP=(FILE *)inInotifyFP;
    for(;;) {
        struct inotify_event *event=getEvent(inotifyFP);
        if(event==NULL) {
            fprintf(stderr,"libtmdrvr: can not get event.\n");
            return NULL;
        }
        bool isError=false;
        if(event->mask & IN_CREATE) {
            // device attached
            if(isEventFile(event->name)==false) {
                goto next;
            }
            // fprintf(stderr,"ATTACH %s\n",event->name);
            pthread_mutex_lock(&gMutexForMonitorArray);
                bool create=false;
                if( gInitializing ) {
                    if(isAlreadyMonitored(event->name)==false) {
                        create=true;
                    }
                } else {
                    create=true;
                }
                if( create ) {
                    if(createMonitorThread(event->name)==false) {
                        fprintf(stderr,"libtmdrvr: can not create monitor thread.\n");
                        isError=true;
                    }
                }
            pthread_mutex_unlock(&gMutexForMonitorArray);
        }
        if( isError ) {
            goto next;
        }
        if(event->mask & IN_DELETE) {
            // device dettached
            if(isEventFile(event->name)==false) {
                goto next;
            }
            //fprintf(stderr,"DETACH %s\n",event->name);
            pthread_mutex_lock(&gMutexForMonitorArray);
                if(disposeMonitorThread(event->name)==false) {
                    fprintf(stderr,"libtmdrvr: can not dispose monitor thread.\n");
                    isError=true;
                }
            pthread_mutex_unlock(&gMutexForMonitorArray);
        }
next:
        free(event);
        if( isError ) {
            break;
        }
    }
    fclose(inotifyFP);
    return NULL;
}
static bool disposeMonitorThread(const char *inEventFile) {
    MonitorInfo *monitorInfo=getMonitorSlot(inEventFile);
    if(monitorInfo==NULL) {
        // mouseWatcherFunc detect the mouse removal and terminate by its self.
        return true;
    }
    pthread_cancel(monitorInfo->thread);
    resetMonitorInfo(monitorInfo);
    return true;
}
static bool isAlreadyMonitored(const char *inEventFile) {
    return getMonitorSlot(inEventFile)!=NULL;
}
static MonitorInfo *getMonitorSlot(const char *inEventFile) {
    int i;
    MonitorInfo *ret=NULL;
    for(i=0; i<kMaxMonitorThread; i++) {
        if(gMonitorArray[i].fp==NULL || gMonitorArray[i].eventFileName==NULL) {
            continue;
        }
        if(strcmp(inEventFile,gMonitorArray[i].eventFileName)==0) {
            ret=gMonitorArray+i;
            break;
        }
    }
    return ret;
}

static struct inotify_event *getEvent(FILE *inINotifyFP) {
    const size_t fixedAreaSize=offsetof(struct inotify_event,name);
    char fixedArea[fixedAreaSize];
    int n=fread(fixedArea,sizeof(fixedArea),1,inINotifyFP);
    struct inotify_event *ret;

    if(n==0) {
        fprintf(stderr,"libtmdrvr: can not read inotify event (fixed area).\n");
        return NULL;
    }

    char nameBuffer[PATH_MAX+1];
    uint32_t nameSize=((struct inotify_event *)fixedArea)->len;
    char *nameAreaTop=nameBuffer;
    if(nameSize==0) {
        // a case of IN_OPEN.
        goto makeReturnValue;
    }
    if(nameSize>sizeof(nameBuffer)) {
        nameAreaTop=(char *)malloc(sizeof(char)*nameSize);
        if(nameAreaTop==NULL) {
            fprintf(stderr,"libtmdrvr: can not allocate file name memory.\n");
            return NULL;
        }
    } else {
        nameAreaTop=nameBuffer;
    }
    n=fread(nameAreaTop,nameSize,1,inINotifyFP);
    if(n==0) {
        fprintf(stderr,"libtmdrvr: can not read inotify event (name area).\n");
        fprintf(stderr,"errno=%s\n",strerror(errno));
        fprintf(stderr,"nameSize=%d\n",nameSize);
        fprintf(stderr,"wd=%d\n",((struct inotify_event *)fixedArea)->wd);
        fprintf(stderr,"mask=%x\n",((struct inotify_event *)fixedArea)->mask);
        return NULL;
    }

makeReturnValue:
    ret=(struct inotify_event *)malloc(sizeof(struct inotify_event)+nameSize);
    memcpy(ret,fixedArea,sizeof(fixedArea));
    if(nameSize>0) {
        memcpy(&ret->name,nameAreaTop,nameSize);
    }
    if(nameAreaTop!=nameBuffer) {
        free(nameAreaTop);
    }
    return ret;
}
static bool createMonitorForAlreadyPlugged() {
    DIR *dir=opendir("/dev/input/");
    if(dir==NULL) {
        return false;    // can not open directory.
    }

    bool ret=true;
    pthread_mutex_lock(&gMutexForMonitorArray);
        struct dirent *entry;
        do {
            entry=readdir(dir);
            if(entry!=NULL && isEventFile(entry->d_name) ) {
                if( isAlreadyMonitored(entry->d_name) ) {
                    continue;
                }
                if(createMonitorThread(entry->d_name)==false) {
                    ret=false;
                    break;
                }
            }
        } while(entry!=NULL);
    pthread_mutex_unlock(&gMutexForMonitorArray);
    closedir(dir);
    return ret;
}
static bool isEventFile(const char *inFileName) {
    if( strncmp(inFileName,"event",5) ) {
        return false;
    }
    int i=4;
    bool isDigit=true;
    do {
        i++;
        if(isdigit(inFileName[i])==false) {
            isDigit=false;
            // printf("i=%d isdigit(%c)==false   ",i,inFileName[i]);
            break;
        }
    } while(inFileName[i+1]!='\0');
    return isDigit;
}
static bool createMonitorThread(const char *inEventFileName) {
    //printf("start createMonitorThread %s\n",inEventFileName);
    MonitorInfo *monitorInfo=getEmptyMonitorArraySlot();
    FILE *fp=NULL;

    char *s=(char *)strdup(inEventFileName);
    if(s==NULL) {
        return false;
    }

    monitorInfo->eventFileName=s;

    char buf[PATH_MAX+1];
    sprintf(buf,"/dev/input/%s",inEventFileName);
tryAgain:
    fp=fopen(buf,"rb");
    if(fp==NULL) {
        if(errno==EACCES) {
            // Immediately after the USB device connection,
            // the permission denided may be occur.
            // So wait one second and retry. 
            sleep(1);
            goto tryAgain;
        } else {
            goto error;
        }
    }
    monitorInfo->fp=fp;
    if( pthread_create(&monitorInfo->thread,NULL,mouseWatcherFunc,monitorInfo) ) {
        goto error;
    }
    return true;

error:
    resetMonitorInfo(monitorInfo);
    return false;
}
static MonitorInfo *getEmptyMonitorArraySlot() {
    int i;
    for(i=0; i<kMaxMonitorThread; i++) {
        if(gMonitorArray[i].eventFileName==NULL
           && gMonitorArray[i].fp==NULL) {
            return gMonitorArray+i;
        }
    }
    return NULL;
}
static void *mouseWatcherFunc(void *inMonitorInfo) {
    MonitorInfo *monitorInfo=(MonitorInfo*)inMonitorInfo;
    FILE *fin=monitorInfo->fp;

    //printf("start mouse watcher [%s]\n",monitorInfo->eventFileName);

    struct input_event ev;
    for(;;) {
        int s;
        if((s=fread(&ev,sizeof(ev),1,fin))!=1) {
            // device detached?
            if( pthread_mutex_lock(&gMutexForMonitorArray) ) {
                fprintf(stderr,"libtmdrvr: can not lock for monitor info array.\n");
            }
                resetMonitorInfo(monitorInfo);
            pthread_mutex_unlock(&gMutexForMonitorArray);
            return NULL;
        }

        if(ev.type==EV_REL) {
            if( pthread_mutex_lock(&gMutexForXY) ) {
                fprintf(stderr,"libtmdrvr: can not lock for mouse position.\n");
                return NULL;
            }
                switch(ev.code) {
                    case REL_X:
                        gMouseX=clamp(gMinX,gMaxX-1,gMouseX+ev.value);
                        break;
                    case REL_Y:
                        gMouseY=clamp(gMinY,gMaxY-1,gMouseY+ev.value);
                        break;
                }
            pthread_mutex_unlock(&gMutexForXY);
        } else if(ev.type==EV_KEY) {
            if( pthread_mutex_lock(&gMutexForButtonStatus) ) {
                fprintf(stderr,"libtmdrvr: can not lock for mouse button.\n");
                return NULL;
            }
                int bitPattern=0;
                switch(ev.code) {
                    case BTN_LEFT:
                        bitPattern=TMDRVR_LEFT_BUTTON_PRESSED;
                        break;
                    case BTN_MIDDLE:
                        bitPattern=TMDRVR_MIDDLE_BUTTON_PRESSED;
                        break;
                    case BTN_RIGHT:
                        bitPattern=TMDRVR_RIGHT_BUTTON_PRESSED;
                }
                if( bitPattern ) {
                    if(ev.value==1) {
                        gMouseButtonStatus |= bitPattern;
                    } else {
                        gMouseButtonStatus &= ~bitPattern;
                    }
                }
            pthread_mutex_unlock(&gMutexForButtonStatus);
        }
    }
    return NULL;
}
static void resetMonitorInfo(MonitorInfo *inMonitorInfo) {
    //fprintf(stderr,"restMonitorInfo[%s]\n",inMonitorInfo->eventFileName);
    if(inMonitorInfo->eventFileName!=NULL) {
        free(inMonitorInfo->eventFileName);
        inMonitorInfo->eventFileName=NULL;
    }
    if(inMonitorInfo->fp!=NULL) {
        fclose(inMonitorInfo->fp);
        inMonitorInfo->fp=NULL;
    }
}
static int clamp(int inMin,int inMax,int inValue) {
    return inValue<inMin ? inMin
                         : inValue>inMax ? inMax : inValue;
}

bool tmDrvrSetArea(int inMinX,int inMinY,int inMaxX,int inMaxY) {
    if(inMinX>inMaxX || inMinY>inMaxY) {
        return false;
    }

    int result=pthread_mutex_lock(&gMutexForXY);
    if( result ) {
        return false;    // can not lock
    }
        gMinX=inMinX;
        gMinY=inMinY;
        gMaxX=inMaxX;
        gMaxY=inMaxY;
    pthread_mutex_unlock(&gMutexForXY);
    return true;
}

bool tmDrvrSetPosition(int inX,int inY) {
    int result=pthread_mutex_lock(&gMutexForXY);
    if( result ) {
        return false;    // can not lock
    }
        gMouseX=clamp(gMinX,gMaxX-1,inX);
        gMouseY=clamp(gMinY,gMaxY-1,inY);
    pthread_mutex_unlock(&gMutexForXY);
    return true;
}

int tmDrvrGetX() {
    return gMouseX;
}

int tmDrvrGetY() {
    return gMouseY;
}

void tmDrvrGetXY(int *outMouseX,int *outMouseY) {
    *outMouseX=gMouseX;
    *outMouseY=gMouseY;
}

int tmDrvrGetButton() {
    return gMouseButtonStatus;
}

bool tmDrvrIsLeftButtonPressed() {
    return (gMouseButtonStatus & TMDRVR_LEFT_BUTTON_PRESSED)!=0;
}

bool tmDrvrIsMiddleButtonPressed() {
    return (gMouseButtonStatus & TMDRVR_MIDDLE_BUTTON_PRESSED)!=0;
}

bool tmDrvrIsRightButtonPressed() {
    return (gMouseButtonStatus & TMDRVR_RIGHT_BUTTON_PRESSED)!=0;
}

bool tmDrvrDispose() {
    if( pthread_mutex_lock(&gMutexForMonitorArray) ) {
        return false;
    }
        int i;
        for(i=0; i<kMaxMonitorThread; i++) {
            if(gMonitorArray[i].eventFileName!=NULL || gMonitorArray[i].fp!=NULL) {
                pthread_cancel(gMonitorArray[i].thread);
                resetMonitorInfo(gMonitorArray+i);
            }
        }
    pthread_mutex_unlock(&gMutexForMonitorArray);
    return true;
}

