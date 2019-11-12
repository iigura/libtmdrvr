# libtmdrvr - Tiny Mouse Driver without X window
This is a tiny mouse driver library for Raspberry Pi (Raspbian) without X window system.
This library is developed for easy use of the mouse in CUI application programs.
With this mouse driver, the X window is no longer required, making a program simple and compact.

Information about the license is given at the end of the text (we adopt the MIT license).

This driver program does not undertake to display the mouse cursor. The mouse cursor position is managed and maintained, but the display of the mouse cursor itself should be implemented on the user program side.

## Description
Once the library is initialized, you can get the mouse pointer position and mouse button status at any time.
It also supports hot-plugging and supports library re-initialization.
For details, refer to the sample program test.c included in the package.

## Usage
Include the tmDriver.h in your C program and specify libtmdrvr.a and the pthread library when linking.
For example:

    gcc -std=gnu99 -Wextra -Wall -pthread test.c -L. -ltmdrvr -o test

See Makefile for details.

## Install
There is no installer in particular.
It is only necessary to expand it to an appropriate location so that the header file and the library file can be referenced by the compiler and the linker.

Run 'make' to create the library libtmdrvr.a.

## Files
tmDriver.h  --- the header file for the C programming language.
libtmdrvr.a --- the library

## APIs
Note: All functions provided by the library have the prefix tmDrvr*.

### Initialize and Dispose the driver
Initialize the mouse driver.

    bool tmDrvrInit(int inMinX,int inMinY,int inMaxX,int inMaxY);
    The mouse pointer movement range (X, Y) is inMinX <= X <inMaxX and inMinY <= Y <inMaxY.    
    Returns true if initialization was successful, otherwise returns false.

Dispose the mouse driver.

    bool tmDrvrDispose();
    Returns true if the driver is successfully destroyed, false otherwise.

### Get the mouse pointer position
The X and Y coordinates of the mouse cursor is going to be get with the following functions, respectively.

Note: The coordinate system used by the mouse cursor is the origin (X, Y) = (0,0) at the top left of the screen, + X for the right direction on the screen, and the + Y direction for the bottom direction.

    int tmDrvrGetX();
    Returns the X coordinate of the current mouse cursor.

    int tmDrvrGetY();
    Returns the X coordinate of the current mouse cursor.

The next function returns the X and Y coordinates of the mouse cursor at the same time.

    void tmDrvrGetXY(int *outMouseX,int *outMouseY);
    The area pointed to by outMouseX stores the X coordinate of the current mouse cursor. The Y coordinate is stored in the area pointed to by outMouseY.
    
### Get the mouse button status
The following function provides information on whether the mouse button has been pressed.

    bool tmDrvrIsLeftButtonPressed();
    This function returns the value of true when the left button is pressed. if the button is not pressed return the value of false.

    bool tmDrvrIsMiddleButtonPressed();
    bool tmDrvrIsRightButtonPressed();
    Programmers are able to use these above functions to check if the middle and right buttons are pressed.

The following function is used to check the status of multiple buttons, such as pressing the right and left buttons simultaneously.

    int tmDrvrGetButton();
    This function returns the button state.
    The status of each buttons are known by taking the bitwise AND of the return value of this function and the following constants.

        TMDRVR_LEFT_BUTTON_PRESSED,
        TMDRVR_MIDDLE_BUTTON_PRESSED,
        TMDRVR_RIGHT_BUTTON_PRESSED,
    
    For example, the following program verifies that the right and left buttons are pressed at the same time, and the middle button is not pressed:
    
        int status=tmDrvrGetButton();
        if(  (status & TMDRVR_LEFT_BUTTON_PRESSED) !=0
          && (status & TMDRVR_RIGHT_BUTTON_PRESSED)!=0
          && (status & TMDRVR_MIDDLE_BUTTON_PRESSED)==0 ) {
            // do something
        } 

### Auxiliary functions
    bool tmDrvrSetPosition(int inMouseX,int inMouseY);
    Set the current mouse cursor position to (X,Y)=(inMouseX,inMouseY).    

    bool tmDrvrSetArea(int inMinX,int inMinY,int inMaxX,int inMaxY);

## System Configuration Diagram

    +-------------------------------------------+
    |    Tiny Mouse Driver <--- this library    |
    +-----------------------+---------+---------+
    | Linux Input Subsystem | inotify | pthread |
    +-----------------------+---------+---------+
    |                Linux Kernel               |
    +-------------------------------------------+
    |                Raspberry Pi               |
    +-------------------------------------------+

## License
The MIT License.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Author
This program is written by Koji Iigura, 2019.
https://github.com/iigura

