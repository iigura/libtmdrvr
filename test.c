/*********************************************************************
    sample program of Tiny Mouse Driver without X
    written by Koji Iigura 2019.
    please see the README.md.
*********************************************************************/

#include <stdio.h>

#include "tmDriver.h"

int main(void) {
    bool isRestart;
    int mouseX=0,mouseY=0;
    int button=0;
restart:
    isRestart=false;
    printf("Initialize Tiny Mouse Driver.\n");
    if(tmDrvrInit(0,0,640,480)==false) {
        fprintf(stderr,"can not initialize TinyMouseDriver.\n");
        return -1;
    }
    printf("press Left and Middle button to terminate.\n");
    printf("press Left and Right button to restart TinyMouseDriver.\n");
    for(;;) {
        int x=tmDrvrGetX();
        int y=tmDrvrGetY();
        int btn=tmDrvrGetButton();
        if(mouseX!=x || mouseY!=y || button!=btn) {
            mouseX=x;
            mouseY=y;
            printf("mouse pos=(%d,%d) ",mouseX,mouseY);
            putchar(tmDrvrIsLeftButtonPressed()   ? 'L' : '_');
            putchar(tmDrvrIsMiddleButtonPressed() ? 'M' : '_');
            putchar(tmDrvrIsRightButtonPressed()  ? 'R' : '_');
            printf(" PressLandM=QUIT PressLandR=Restart\n");
            button=btn;
        }
        if((button&TMDRVR_MIDDLE_BUTTON_PRESSED)
        && (button&TMDRVR_LEFT_BUTTON_PRESSED)) {
            break;
        }
        if((button&TMDRVR_RIGHT_BUTTON_PRESSED)
        && (button&TMDRVR_LEFT_BUTTON_PRESSED)) {
            isRestart=true;
            break;
        }
    }
    
    printf("Dipose Tiny Mouse Driver.\n");
    if(tmDrvrDispose()==false) {
        fprintf(stderr,"ERROR: can not dispose Tiny Mouse Driver.\n");
        return -1;
    }

    if(isRestart) {
        goto restart;
    }

    return 0;
}

