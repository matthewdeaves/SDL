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

#ifdef SDL_JOYSTICK_IOKIT

/*
 * Joystick driver for Mac OS X 10.3 and 10.4, for the old-Mac PowerPC port.
 *
 * WHY THIS FILE EXISTS
 *
 * SDL2 upstream ships exactly one macOS joystick backend
 * (SDL_sysjoystick.c) and it is written against the IOHIDManager API.
 * IOHIDManager's headers (IOHIDBase.h, IOHIDDevice.h, IOHIDElement.h) first
 * appear in the 10.5 SDK. <IOKit/hid/IOHIDLib.h> itself DOES exist in the
 * 10.3.9 and 10.4u SDKs, so the include succeeds and the compile instead
 * fails with a wall of "syntax error before 'IOHIDElementRef'", which reads
 * nothing like a missing header. Measured 2026-08-21: identical source and
 * compiler, 10.3.9 SDK -> make exits 2, 10.5 SDK -> clean.
 *
 * That is the whole reason every PowerPC SDL in this project was configured
 * --disable-joystick, and therefore why the PowerPC slices had no gamepad
 * support: SDL_GameController still linked, but sat on the dummy joystick
 * driver and reported zero devices.
 *
 * SDL 1.2's darwin backend targeted the older IOCFPlugIn /
 * IOHIDDeviceInterface API, and IOCFPlugIn.h IS present in both old SDKs.
 * That is why the SDL 1.2 sister ports (Quake 1/2/3) have working joysticks
 * on Panther and Tiger while the SDL2 one did not.
 *
 * This file reimplements SDL2's joystick driver interface over that older
 * API. Device discovery, HID element walking, value reading and the
 * auto-calibration are ported from SDL 1.2's SDL_sysjoystick.c; the
 * instance-id, GUID and attach/detach parts are what SDL2 adds on top.
 *
 * NOT VERIFIED ON HARDWARE. No USB gamepad was available on a PowerPC Mac
 * when this was written, so it is build-correct and reasoned rather than
 * measured. The build is the only thing that has checked it. Treat a report
 * that it does not work as expected rather than surprising.
 *
 * Known limitation: no hotplug. SDL 1.2 enumerated once at init and so does
 * this, so a pad must be plugged in before the game starts. Unplugging IS
 * noticed, via the removal callback, and the device then reports detached
 * with its axes and buttons zeroed. Adding hotplug would mean an IOKit
 * notification port and a run loop source, which is a bigger change than the
 * one this file is for.
 */

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1050

#include <unistd.h>
#include <ctype.h>
#include <sysexits.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#ifdef MACOS_10_0_4
#include <IOKit/hidsystem/IOHIDUsageTables.h>
#else
/* The header moved here in Mac OS X 10.1 */
#include <Kernel/IOKit/hidsystem/IOHIDUsageTables.h>
#endif
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <CoreFoundation/CoreFoundation.h>

#include "SDL_joystick.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"
#include "SDL_sysjoystick_c.h"

/* Linked list of all discovered devices, in enumeration order. */
static recDevice *gpDeviceList = NULL;
static int s_joystick_instance_id = -1;

static void HIDGetCollectionElements(CFMutableDictionaryRef deviceProperties,
                                     recDevice *pDevice);

static int
GetIntValueFromDictionary(CFDictionaryRef dict, CFStringRef name, int defaultValue)
{
    int result;
    CFNumberRef number;

    if (CFDictionaryGetValueIfPresent(dict, name, (CFTypeRef *) &number)) {
        if (CFGetTypeID(number) == CFNumberGetTypeID()) {
            if (CFNumberGetValue(number, kCFNumberIntType, &result)) {
                return result;
            }
        }
    }
    return defaultValue;
}

/* Current value of an element, polling it. Returns 0 on any error, which the
 * caller cannot distinguish from a genuine zero; that is how SDL 1.2 behaved
 * and a joystick axis reading zero is harmless. */
static SInt32
HIDGetElementValue(recDevice *pDevice, recElement *pElement)
{
    IOHIDEventStruct hidEvent;
    hidEvent.value = 0;

    if (pDevice && pElement && pDevice->interface) {
        IOReturn result = (*(pDevice->interface))->getElementValue(pDevice->interface,
                                                                   pElement->cookie,
                                                                   &hidEvent);
        if (result == kIOReturnSuccess) {
            /* record min and max for auto calibration */
            if (hidEvent.value < pElement->minReport) {
                pElement->minReport = hidEvent.value;
            }
            if (hidEvent.value > pElement->maxReport) {
                pElement->maxReport = hidEvent.value;
            }
        }
    }

    return hidEvent.value;
}

static SInt32
HIDScaledCalibratedValue(recDevice *pDevice, recElement *pElement, long min, long max)
{
    float deviceScale = max - min;
    float readScale = pElement->maxReport - pElement->minReport;
    SInt32 value = HIDGetElementValue(pDevice, pElement);

    if (readScale == 0) {
        return value;           /* no scaling at all */
    }
    return ((value - pElement->minReport) * deviceScale / readScale) + min;
}

static void
HIDRemovalCallback(void *target, IOReturn result, void *refcon, void *sender)
{
    recDevice *device = (recDevice *) refcon;
    device->removed = 1;
    device->uncentered = 1;
}

/* Create and open an interface to the device. The application owns the device
 * from here and must close and release it before exiting. */
static IOReturn
HIDCreateOpenDeviceInterface(io_object_t hidDevice, recDevice *pDevice)
{
    IOReturn result = kIOReturnSuccess;
    HRESULT plugInResult = S_OK;
    SInt32 score = 0;
    IOCFPlugInInterface **ppPlugInInterface = NULL;

    if (pDevice->interface == NULL) {
        result = IOCreatePlugInInterfaceForService(hidDevice,
                                                   kIOHIDDeviceUserClientTypeID,
                                                   kIOCFPlugInInterfaceID,
                                                   &ppPlugInInterface, &score);
        if (result == kIOReturnSuccess) {
            plugInResult = (*ppPlugInInterface)->QueryInterface(ppPlugInInterface,
                                CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID),
                                (void *) &(pDevice->interface));
            if (plugInResult != S_OK) {
                SDL_SetError("Couldn't query HID class device interface from plugInInterface");
            }
            (*ppPlugInInterface)->Release(ppPlugInInterface);
        } else {
            SDL_SetError("Failed to create plugInInterface via IOCreatePlugInInterfaceForService");
        }
    }

    if (pDevice->interface != NULL) {
        result = (*(pDevice->interface))->open(pDevice->interface, 0);
        if (result != kIOReturnSuccess) {
            SDL_SetError("Failed to open pDevice->interface via open");
        } else {
            (*(pDevice->interface))->setRemovalCallback(pDevice->interface,
                                                        HIDRemovalCallback,
                                                        pDevice, pDevice);
        }
    }
    return result;
}

static IOReturn
HIDCloseReleaseInterface(recDevice *pDevice)
{
    IOReturn result = kIOReturnSuccess;

    if (pDevice && pDevice->interface) {
        result = (*(pDevice->interface))->close(pDevice->interface);
        if (result != kIOReturnNotOpen && result != kIOReturnSuccess) {
            SDL_SetError("Failed to close IOHIDDeviceInterface");
        }
        result = (*(pDevice->interface))->Release(pDevice->interface);
        if (result != kIOReturnSuccess) {
            SDL_SetError("Failed to release IOHIDDeviceInterface");
        }
        pDevice->interface = NULL;
    }
    return result;
}

/* Pull the specifics out of one element's CF dictionary entry. */
static void
HIDGetElementInfo(CFTypeRef refElement, recElement *pElement)
{
    long number;
    CFTypeRef refType;

    refType = CFDictionaryGetValue(refElement, CFSTR(kIOHIDElementCookieKey));
    if (refType && CFNumberGetValue(refType, kCFNumberLongType, &number)) {
        pElement->cookie = (IOHIDElementCookie) number;
    }
    refType = CFDictionaryGetValue(refElement, CFSTR(kIOHIDElementMinKey));
    if (refType && CFNumberGetValue(refType, kCFNumberLongType, &number)) {
        pElement->minReport = pElement->min = number;
    }
    refType = CFDictionaryGetValue(refElement, CFSTR(kIOHIDElementMaxKey));
    if (refType && CFNumberGetValue(refType, kCFNumberLongType, &number)) {
        pElement->maxReport = pElement->max = number;
    }
}

/* Decide whether one element is interesting, and if so append it to the right
 * per-device list. Collections recurse so the device's element tree is
 * flattened into three flat lists. */
static void
HIDAddElement(CFTypeRef refElement, recDevice *pDevice)
{
    recElement *element = NULL;
    recElement **headElement = NULL;
    long elementType, usagePage, usage;
    CFTypeRef refElementType = CFDictionaryGetValue(refElement, CFSTR(kIOHIDElementTypeKey));
    CFTypeRef refUsagePage = CFDictionaryGetValue(refElement, CFSTR(kIOHIDElementUsagePageKey));
    CFTypeRef refUsage = CFDictionaryGetValue(refElement, CFSTR(kIOHIDElementUsageKey));

    if (refElementType &&
        CFNumberGetValue(refElementType, kCFNumberLongType, &elementType)) {
        if ((elementType == kIOHIDElementTypeInput_Misc) ||
            (elementType == kIOHIDElementTypeInput_Button) ||
            (elementType == kIOHIDElementTypeInput_Axis)) {

            if (refUsagePage && CFNumberGetValue(refUsagePage, kCFNumberLongType, &usagePage) &&
                refUsage && CFNumberGetValue(refUsage, kCFNumberLongType, &usage)) {

                switch (usagePage) {
                case kHIDPage_GenericDesktop:
                    switch (usage) {
                    case kHIDUsage_GD_X:
                    case kHIDUsage_GD_Y:
                    case kHIDUsage_GD_Z:
                    case kHIDUsage_GD_Rx:
                    case kHIDUsage_GD_Ry:
                    case kHIDUsage_GD_Rz:
                    case kHIDUsage_GD_Slider:
                    case kHIDUsage_GD_Dial:
                    case kHIDUsage_GD_Wheel:
                        element = (recElement *) SDL_calloc(1, sizeof(recElement));
                        if (element) {
                            pDevice->axes++;
                            headElement = &(pDevice->firstAxis);
                        }
                        break;
                    case kHIDUsage_GD_Hatswitch:
                        element = (recElement *) SDL_calloc(1, sizeof(recElement));
                        if (element) {
                            pDevice->hats++;
                            headElement = &(pDevice->firstHat);
                        }
                        break;
                    }
                    break;
                case kHIDPage_Button:
                    element = (recElement *) SDL_calloc(1, sizeof(recElement));
                    if (element) {
                        pDevice->buttons++;
                        headElement = &(pDevice->firstButton);
                    }
                    break;
                case kHIDPage_Simulation:
                    switch (usage) {
                    case kHIDUsage_Sim_Rudder:
                    case kHIDUsage_Sim_Throttle:
                        element = (recElement *) SDL_calloc(1, sizeof(recElement));
                        if (element) {
                            pDevice->axes++;
                            headElement = &(pDevice->firstAxis);
                        }
                        break;
                    }
                    break;
                default:
                    break;
                }
            }
        } else if (elementType == kIOHIDElementTypeCollection) {
            HIDGetCollectionElements((CFMutableDictionaryRef) refElement, pDevice);
        }
    }

    if (element && headElement) {
        recElement *elementPrevious = NULL;
        recElement *elementCurrent = *headElement;

        pDevice->elements++;
        while (elementCurrent) {
            elementPrevious = elementCurrent;
            elementCurrent = elementPrevious->pNext;
        }
        if (elementPrevious) {
            elementPrevious->pNext = element;
        } else {
            *headElement = element;
        }
        element->pNext = NULL;
        HIDGetElementInfo(refElement, element);
    }
}

static void
HIDGetElementsCFArrayHandler(const void *value, void *parameter)
{
    if (CFGetTypeID(value) == CFDictionaryGetTypeID()) {
        HIDAddElement((CFTypeRef) value, (recDevice *) parameter);
    }
}

static void
HIDGetElements(CFTypeRef refElementCurrent, recDevice *pDevice)
{
    if (CFGetTypeID(refElementCurrent) == CFArrayGetTypeID()) {
        CFRange range = { 0, CFArrayGetCount(refElementCurrent) };
        CFArrayApplyFunction(refElementCurrent, range,
                             HIDGetElementsCFArrayHandler, pDevice);
    }
}

static void
HIDGetCollectionElements(CFMutableDictionaryRef deviceProperties, recDevice *pDevice)
{
    CFTypeRef refElementTop = CFDictionaryGetValue(deviceProperties, CFSTR(kIOHIDElementKey));
    if (refElementTop) {
        HIDGetElements(refElementTop, pDevice);
    }
}

static void
HIDTopLevelElementHandler(const void *value, void *parameter)
{
    CFTypeRef refCF = 0;

    if (CFGetTypeID(value) != CFDictionaryGetTypeID()) {
        return;
    }
    refCF = CFDictionaryGetValue(value, CFSTR(kIOHIDElementUsagePageKey));
    if (!CFNumberGetValue(refCF, kCFNumberLongType, &((recDevice *) parameter)->usagePage)) {
        SDL_SetError("CFNumberGetValue error retrieving pDevice->usagePage");
    }
    refCF = CFDictionaryGetValue(value, CFSTR(kIOHIDElementUsageKey));
    if (!CFNumberGetValue(refCF, kCFNumberLongType, &((recDevice *) parameter)->usage)) {
        SDL_SetError("CFNumberGetValue error retrieving pDevice->usage");
    }
}

/* Build the 16-byte SDL2 joystick GUID. Same layout the IOHIDManager backend
 * uses: vendor id at data[0], product id at data[8], and a name-derived
 * Bluetooth-style GUID when the device reports neither. Keeping the layout
 * identical matters because gamecontrollerdb entries are keyed on it, so a pad
 * mapped on any other platform maps here too. */
static void
HIDBuildGUID(recDevice *pDevice, CFMutableDictionaryRef hidProperties,
             CFMutableDictionaryRef usbProperties)
{
    Uint32 *guid32;
    Sint32 vendor = 0;
    Sint32 product = 0;

    SDL_zero(pDevice->guid);

    vendor = GetIntValueFromDictionary(hidProperties, CFSTR(kIOHIDVendorIDKey), 0);
    product = GetIntValueFromDictionary(hidProperties, CFSTR(kIOHIDProductIDKey), 0);

    /* 10.3/10.4 do not mirror every USB property onto the HID page, so fall
     * back to the USB dictionary two levels up the registry. */
    if (usbProperties) {
        if (!vendor) {
            vendor = GetIntValueFromDictionary(usbProperties, CFSTR("idVendor"), 0);
        }
        if (!product) {
            product = GetIntValueFromDictionary(usbProperties, CFSTR("idProduct"), 0);
        }
    }

    SDL_memcpy(&pDevice->guid.data[0], &vendor, sizeof(vendor));
    SDL_memcpy(&pDevice->guid.data[8], &product, sizeof(product));

    guid32 = (Uint32 *) pDevice->guid.data;
    if (!guid32[0] && !guid32[1]) {
        /* No vendor or product id: derive from the name, as the other
         * backends do for Bluetooth devices. */
        const Uint16 BUS_BLUETOOTH = 0x05;
        Uint16 *guid16 = (Uint16 *) guid32;
        *guid16++ = BUS_BLUETOOTH;
        *guid16++ = 0;
        SDL_strlcpy((char *) guid16, pDevice->product,
                    sizeof(pDevice->guid.data) - 4);
    }
}

static void
HIDGetDeviceInfo(io_object_t hidDevice, CFMutableDictionaryRef hidProperties,
                 recDevice *pDevice)
{
    CFMutableDictionaryRef usbProperties = 0;
    io_registry_entry_t parent1, parent2;
    CFTypeRef refCF = 0;
    int haveUSB = 0;

    /* Step up two levels in the registry for the USB properties. */
    if ((IORegistryEntryGetParentEntry(hidDevice, kIOServicePlane, &parent1) == KERN_SUCCESS) &&
        (IORegistryEntryGetParentEntry(parent1, kIOServicePlane, &parent2) == KERN_SUCCESS) &&
        (IORegistryEntryCreateCFProperties(parent2, &usbProperties, kCFAllocatorDefault, kNilOptions) == KERN_SUCCESS)) {
        haveUSB = (usbProperties != NULL);
    }

    /* Product name: HID dictionary first, then USB. */
    refCF = CFDictionaryGetValue(hidProperties, CFSTR(kIOHIDProductKey));
    if (!refCF && haveUSB) {
        refCF = CFDictionaryGetValue(usbProperties, CFSTR("USB Product Name"));
    }
    if (!refCF || !CFStringGetCString(refCF, pDevice->product,
                                      sizeof(pDevice->product),
                                      CFStringGetSystemEncoding())) {
        SDL_strlcpy(pDevice->product, "Unidentified joystick",
                    sizeof(pDevice->product));
    }

    /* Usage page and usage. */
    refCF = CFDictionaryGetValue(hidProperties, CFSTR(kIOHIDPrimaryUsagePageKey));
    if (refCF) {
        if (!CFNumberGetValue(refCF, kCFNumberLongType, &pDevice->usagePage)) {
            SDL_SetError("CFNumberGetValue error retrieving pDevice->usagePage");
        }
        refCF = CFDictionaryGetValue(hidProperties, CFSTR(kIOHIDPrimaryUsageKey));
        if (refCF) {
            if (!CFNumberGetValue(refCF, kCFNumberLongType, &pDevice->usage)) {
                SDL_SetError("CFNumberGetValue error retrieving pDevice->usage");
            }
        }
    }

    if (refCF == NULL) {
        /* Fall back to the top level element. */
        CFTypeRef refCFTopElement = CFDictionaryGetValue(hidProperties, CFSTR(kIOHIDElementKey));
        if (refCFTopElement) {
            CFRange range = { 0, CFArrayGetCount(refCFTopElement) };
            CFArrayApplyFunction(refCFTopElement, range,
                                 HIDTopLevelElementHandler, pDevice);
        }
    }

    HIDBuildGUID(pDevice, hidProperties, haveUSB ? usbProperties : NULL);

    if (haveUSB) {
        CFRelease(usbProperties);
    }
    if (usbProperties || haveUSB) {
        IOObjectRelease(parent2);
        IOObjectRelease(parent1);
    }
}

static recDevice *
HIDBuildDevice(io_object_t hidDevice, CFMutableDictionaryRef hidProperties)
{
    recDevice *pDevice = (recDevice *) SDL_calloc(1, sizeof(recDevice));

    if (pDevice) {
        if (HIDCreateOpenDeviceInterface(hidDevice, pDevice) == kIOReturnSuccess) {
            HIDGetDeviceInfo(hidDevice, hidProperties, pDevice);
            HIDGetCollectionElements(hidProperties, pDevice);
            pDevice->instance_id = ++s_joystick_instance_id;
        } else {
            SDL_free(pDevice);
            pDevice = NULL;
        }
    }
    return pDevice;
}

static void
HIDDisposeElementList(recElement **elementList)
{
    recElement *pElement = *elementList;

    while (pElement) {
        recElement *pElementNext = pElement->pNext;
        SDL_free(pElement);
        pElement = pElementNext;
    }
    *elementList = NULL;
}

static recDevice *
HIDDisposeDevice(recDevice **ppDevice)
{
    recDevice *pDeviceNext = NULL;

    if (*ppDevice) {
        pDeviceNext = (*ppDevice)->pNext;

        HIDDisposeElementList(&(*ppDevice)->firstAxis);
        HIDDisposeElementList(&(*ppDevice)->firstButton);
        HIDDisposeElementList(&(*ppDevice)->firstHat);

        HIDCloseReleaseInterface(*ppDevice);
        SDL_free(*ppDevice);
        *ppDevice = NULL;
    }
    return pDeviceNext;
}

static recDevice *
GetDeviceForIndex(int device_index)
{
    recDevice *device = gpDeviceList;

    while (device && device_index > 0) {
        device = device->pNext;
        device_index--;
    }
    return device;
}

/* ---- SDL2 joystick driver interface ------------------------------------- */

int
SDL_SYS_JoystickInit(void)
{
    IOReturn result = kIOReturnSuccess;
    mach_port_t masterPort = 0;
    io_iterator_t hidObjectIterator = 0;
    CFMutableDictionaryRef hidMatchDictionary = NULL;
    recDevice *device, *lastDevice;
    io_object_t ioHIDDeviceObject = 0;
    CFMutableDictionaryRef hidProperties;
    int numjoysticks = 0;

    if (gpDeviceList) {
        return SDL_SetError("Joystick: Device list already inited.");
    }

    result = IOMasterPort(bootstrap_port, &masterPort);
    if (result != kIOReturnSuccess) {
        return SDL_SetError("Joystick: IOMasterPort error with bootstrap_port.");
    }

    hidMatchDictionary = IOServiceMatching(kIOHIDDeviceKey);
    if (!hidMatchDictionary) {
        return SDL_SetError("Joystick: Failed to get HID CFMutableDictionaryRef via IOServiceMatching.");
    }

    /* Consumes a reference to the dictionary, so it must not be released. */
    result = IOServiceGetMatchingServices(masterPort, hidMatchDictionary, &hidObjectIterator);
    if (result != kIOReturnSuccess) {
        return SDL_SetError("Joystick: Couldn't create a HID object iterator.");
    }
    if (!hidObjectIterator) {    /* no joysticks attached */
        return 0;
    }

    gpDeviceList = lastDevice = NULL;

    for (ioHIDDeviceObject = IOIteratorNext(hidObjectIterator);
         ioHIDDeviceObject != 0;
         IOObjectRelease(ioHIDDeviceObject),
         ioHIDDeviceObject = IOIteratorNext(hidObjectIterator)) {

        int page, usage;
        SDL_bool skip = SDL_TRUE;

        result = IORegistryEntryCreateCFProperties(ioHIDDeviceObject, &hidProperties,
                                                   kCFAllocatorDefault, kNilOptions);
        if (result != KERN_SUCCESS || hidProperties == NULL) {
            continue;
        }

        /* Filter out keyboards, mice and everything else that is not a pad. */
        page = GetIntValueFromDictionary(hidProperties, CFSTR(kIOHIDPrimaryUsagePageKey),
                                         kHIDPage_Undefined);
        usage = GetIntValueFromDictionary(hidProperties, CFSTR(kIOHIDPrimaryUsageKey),
                                          kHIDUsage_Undefined);
        if (page == kHIDPage_GenericDesktop) {
            switch (usage) {
            case kHIDUsage_GD_Joystick:
            case kHIDUsage_GD_GamePad:
            case kHIDUsage_GD_MultiAxisController:
                skip = SDL_FALSE;
                break;
            }
        }

        device = skip ? NULL : HIDBuildDevice(ioHIDDeviceObject, hidProperties);
        CFRelease(hidProperties);
        if (!device) {
            continue;
        }

        if (lastDevice) {
            lastDevice->pNext = device;
        } else {
            gpDeviceList = device;
        }
        lastDevice = device;
    }
    IOObjectRelease(hidObjectIterator);

    for (device = gpDeviceList; device; device = device->pNext) {
        numjoysticks++;
    }
    return numjoysticks;
}

int
SDL_SYS_NumJoysticks()
{
    recDevice *device = gpDeviceList;
    int nJoySticks = 0;

    while (device) {
        if (!device->removed) {
            nJoySticks++;
        }
        device = device->pNext;
    }
    return nJoySticks;
}

void
SDL_SYS_JoystickDetect()
{
    /* No hotplug on this path: SDL 1.2 enumerated once at init and so do we.
     * See the file header. Removal is still noticed via HIDRemovalCallback,
     * which SDL_SYS_JoystickAttached reports. */
}

SDL_bool
SDL_SYS_JoystickNeedsPolling()
{
    /* Nothing for SDL to poll for, since there is no hotplug here. */
    return SDL_FALSE;
}

const char *
SDL_SYS_JoystickNameForDeviceIndex(int device_index)
{
    recDevice *device = GetDeviceForIndex(device_index);
    return device ? device->product : NULL;
}

SDL_JoystickID
SDL_SYS_GetInstanceIdOfDeviceIndex(int device_index)
{
    recDevice *device = GetDeviceForIndex(device_index);
    return device ? device->instance_id : -1;
}

int
SDL_SYS_JoystickOpen(SDL_Joystick * joystick, int device_index)
{
    recDevice *device = GetDeviceForIndex(device_index);

    if (!device) {
        return SDL_SetError("Joystick: no device at index %d", device_index);
    }

    joystick->instance_id = device->instance_id;
    joystick->hwdata = device;
    joystick->name = device->product;

    joystick->naxes = device->axes;
    joystick->nhats = device->hats;
    joystick->nballs = 0;
    joystick->nbuttons = device->buttons;
    return 0;
}

SDL_bool
SDL_SYS_JoystickAttached(SDL_Joystick * joystick)
{
    return joystick->hwdata && !joystick->hwdata->removed ? SDL_TRUE : SDL_FALSE;
}

void
SDL_SYS_JoystickUpdate(SDL_Joystick * joystick)
{
    recDevice *device = joystick->hwdata;
    recElement *element;
    SInt32 value, range;
    int i;

    if (!device) {
        return;
    }

    if (device->removed) {      /* device was unplugged; ignore it. */
        if (device->uncentered) {
            device->uncentered = 0;

            /* Tell the app that everything is centered and unpressed. */
            for (i = 0; i < device->axes; i++) {
                SDL_PrivateJoystickAxis(joystick, i, 0);
            }
            for (i = 0; i < device->buttons; i++) {
                SDL_PrivateJoystickButton(joystick, i, 0);
            }
            for (i = 0; i < device->hats; i++) {
                SDL_PrivateJoystickHat(joystick, i, SDL_HAT_CENTERED);
            }
        }
        return;
    }

    element = device->firstAxis;
    i = 0;
    while (element) {
        value = HIDScaledCalibratedValue(device, element, -32768, 32767);
        if (value != joystick->axes[i]) {
            SDL_PrivateJoystickAxis(joystick, i, value);
        }
        element = element->pNext;
        ++i;
    }

    element = device->firstButton;
    i = 0;
    while (element) {
        value = HIDGetElementValue(device, element);
        if (value > 1) {        /* handle pressure-sensitive buttons */
            value = 1;
        }
        if (value != joystick->buttons[i]) {
            SDL_PrivateJoystickButton(joystick, i, value);
        }
        element = element->pNext;
        ++i;
    }

    element = device->firstHat;
    i = 0;
    while (element) {
        Uint8 pos = 0;

        range = (element->max - element->min + 1);
        value = HIDGetElementValue(device, element) - element->min;
        if (range == 4) {       /* 4 position hatswitch, scale up value */
            value *= 2;
        } else if (range != 8) {
            /* Neither 4 nor 8 positions, fall back to centered */
            value = -1;
        }
        switch (value) {
        case 0:
            pos = SDL_HAT_UP;
            break;
        case 1:
            pos = SDL_HAT_RIGHTUP;
            break;
        case 2:
            pos = SDL_HAT_RIGHT;
            break;
        case 3:
            pos = SDL_HAT_RIGHTDOWN;
            break;
        case 4:
            pos = SDL_HAT_DOWN;
            break;
        case 5:
            pos = SDL_HAT_LEFTDOWN;
            break;
        case 6:
            pos = SDL_HAT_LEFT;
            break;
        case 7:
            pos = SDL_HAT_LEFTUP;
            break;
        default:
            /* Every other value maps to centered. Some pads use 8 and some 15
             * for this, and there are more variants out there, so be
             * generous. */
            pos = SDL_HAT_CENTERED;
            break;
        }
        if (pos != joystick->hats[i]) {
            SDL_PrivateJoystickHat(joystick, i, pos);
        }
        element = element->pNext;
        ++i;
    }
}

void
SDL_SYS_JoystickClose(SDL_Joystick * joystick)
{
    /* The device stays open for the lifetime of the device list, exactly as in
     * SDL 1.2. SDL_SYS_JoystickQuit does the closing and releasing. */
}

void
SDL_SYS_JoystickQuit(void)
{
    while (gpDeviceList) {
        gpDeviceList = HIDDisposeDevice(&gpDeviceList);
    }
    s_joystick_instance_id = -1;
}

SDL_JoystickGUID
SDL_SYS_JoystickGetDeviceGUID(int device_index)
{
    recDevice *device = GetDeviceForIndex(device_index);
    SDL_JoystickGUID guid;

    if (device) {
        guid = device->guid;
    } else {
        SDL_zero(guid);
    }
    return guid;
}

SDL_JoystickGUID
SDL_SYS_JoystickGetGUID(SDL_Joystick * joystick)
{
    SDL_JoystickGUID guid;

    if (joystick->hwdata) {
        guid = joystick->hwdata->guid;
    } else {
        SDL_zero(guid);
    }
    return guid;
}

#endif /* MAC_OS_X_VERSION_MAX_ALLOWED < 1050 */

#endif /* SDL_JOYSTICK_IOKIT */

/* vi: set ts=4 sw=4 expandtab: */
