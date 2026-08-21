/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#ifndef SDL_JOYSTICK_IOKIT_H

#include <IOKit/hid/IOHIDLib.h>

/* old-Mac port: SDL2 upstream has exactly one macOS joystick backend and it is
 * written against the IOHIDManager API, whose headers (IOHIDBase.h,
 * IOHIDDevice.h, IOHIDElement.h) first ship in the 10.5 SDK. IOHIDLib.h itself
 * DOES exist in 10.3.9 and 10.4u, so the #include above succeeds and the build
 * instead dies on a wall of "syntax error before 'IOHIDElementRef'".
 *
 * That is why every PowerPC SDL here was configured --disable-joystick, and why
 * the PowerPC slices had no gamepad support at all.
 *
 * SDL 1.2's darwin backend used the older IOCFPlugIn / IOHIDDeviceInterface API,
 * which IS present in both old SDKs. SDL_sysjoystick_legacy.c reimplements the
 * SDL2 driver interface on top of that, and is selected here by SDK version.
 * Below 10.5 the IOHIDManager types simply do not exist, so the two structure
 * sets cannot be shared.
 */
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1050

struct recElement
{
    IOHIDElementCookie cookie;  /* unique value identifying the element */
    long min;                   /* reported min value possible */
    long max;                   /* reported max value possible */

    /* runtime variables used for auto-calibration */
    long minReport;             /* min returned value */
    long maxReport;             /* max returned value */

    struct recElement *pNext;   /* next element in list */
};
typedef struct recElement recElement;

struct joystick_hwdata
{
    IOHIDDeviceInterface **interface;   /* NULL = no interface */

    char product[256];          /* name of product */
    long usage;                 /* usage page from IOUSBHID Parser.h */
    long usagePage;             /* usage within the above page */

    long axes;                  /* number of axes (calculated) */
    long buttons;               /* number of buttons (calculated) */
    long hats;                  /* number of hat switches (calculated) */
    long elements;              /* total elements (calculated) */

    recElement *firstAxis;
    recElement *firstButton;
    recElement *firstHat;

    int removed;
    int uncentered;

    int instance_id;
    SDL_JoystickGUID guid;

    struct joystick_hwdata *pNext;      /* next device */
};
typedef struct joystick_hwdata recDevice;

#else   /* 10.5 SDK or newer: upstream's IOHIDManager backend */

struct recElement
{
    IOHIDElementRef elementRef;
    IOHIDElementCookie cookie;
    uint32_t usagePage, usage;      /* HID usage */
    SInt32 min;                   /* reported min value possible */
    SInt32 max;                   /* reported max value possible */

    /* runtime variables used for auto-calibration */
    SInt32 minReport;             /* min returned value */
    SInt32 maxReport;             /* max returned value */

    struct recElement *pNext;   /* next element in list */
};
typedef struct recElement recElement;

struct joystick_hwdata
{
    IOHIDDeviceRef deviceRef;   /* HIDManager device handle */
    io_service_t ffservice;     /* Interface for force feedback, 0 = no ff */

    char product[256];          /* name of product */
    uint32_t usage;                 /* usage page from IOUSBHID Parser.h which defines general usage */
    uint32_t usagePage;             /* usage within above page from IOUSBHID Parser.h which defines specific usage */

    int axes;                  /* number of axis (calculated, not reported by device) */
    int buttons;               /* number of buttons (calculated, not reported by device) */
    int hats;                  /* number of hat switches (calculated, not reported by device) */
    int elements;              /* number of total elements (should be total of above) (calculated, not reported by device) */

    recElement *firstAxis;
    recElement *firstButton;
    recElement *firstHat;

    int removed;
    int uncentered;

    int instance_id;
    SDL_JoystickGUID guid;
    Uint8 send_open_event;      /* 1 if we need to send an Added event for this device */

    struct joystick_hwdata *pNext;      /* next device */
};
typedef struct joystick_hwdata recDevice;

#endif /* MAC_OS_X_VERSION_MAX_ALLOWED < 1050 */

#endif /* SDL_JOYSTICK_IOKIT_H */
