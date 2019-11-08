/*********************************************************************
    Tiny Mouse Driver without X
    written by Koji Iigura 2019.
    please see the README.md.
*********************************************************************/

#ifndef libtmdrvr__TINY_MOUSE_DRIVER_H__
#define libtmdrvr__TINY_MOUSE_DRIVER_H__

#include <stdbool.h>

// mouseX in [inMinX,inMaxX)
// mouseY in [inMinY,inMaxY)
bool tmDrvrInit(int inMinX,int inMinY,int inMaxX,int inMaxY);
bool tmDrvrDispose();

int tmDrvrGetX();
int tmDrvrGetY();
void tmDrvrGetXY(int *outMouseX,int *outMouseY);

bool tmDrvrIsLeftButtonPressed();
bool tmDrvrIsMiddleButtonPressed();
bool tmDrvrIsRightButtonPressed();

enum {
    TMDRVR_LEFT_BUTTON_PRESSED    =1,
    TMDRVR_MIDDLE_BUTTON_PRESSED=2,
    TMDRVR_RIGHT_BUTTON_PRESSED    =4,
};
int tmDrvrGetButton();

bool tmDrvrSetPosition(int inMouseX,int inMouseY);
bool tmDrvrSetArea(int inMinX,int inMinY,int inMaxX,int inMaxY);

#endif    // libtmdrvr__TINY_MOUSE_DRIVER_H__

