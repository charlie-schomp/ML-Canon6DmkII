#ifndef _property_whitelist_h_
#define _property_whitelist_h_

#include "property.h"

// By default, we allow "reads" (property_handler, ML hooking) for any property.
//
// Initial support for Digic 6, 7, 8 cams found some prop handlers triggered
// bad behaviour (crashes, hangs).  These are blacklisted for both read and write
// until investigated and fixed.
//
// For Digic 4 and 5 cams, "writes" (prop_request_change()) are controlled
// by a global flag, CONFIG_PROP_REQUEST_CHANGE.  This is all or nothing.
//
// For D678, we disallow all "writes" via the same flag, so,
// CONFIG_PROP_REQUEST_CHANGE should be #undef for new ports in features.h.
// Writing to props can brick cams if mistakes are made.
//
// Additionally, even when defined, we now have a per property check (on D678),
// so if prop_write_allow[] is empty, no writes will be attempted even
// with CONFIG_PROP_REQUEST_CHANGE.  This allows devs to enable properties
// one at a time after checking correctness via reversing, tests, etc.

// DO NOT put the same property in both lists;
// that would allow writes but not reads, which is untested
// (and unnecessary as far as I know).

// deny reads / do not register property handlers for these
const uint32_t prop_handler_deny[] =
{
    /* PROP_MVR_REC_START was listed here from the first port ("probably related
     * to MVR stubs being all wrong"). Denying it left ML unable to see Canon
     * recording at all: RECORDING_H264 stayed 0, so the H.264 proxy never got
     * its buffers back and never stopped Canon's recording on giving up. Its
     * handler that touches hardware - audio-common.c's enable_recording(),
     * which wakes the AK audio-chip task - is disabled for this body instead. */
    0   /* no property is 0; keeps the array non-empty */
};

// allow writes / allow prop_request_change() for these:
const uint32_t prop_write_allow[] =
{
    PROP_ICU_AUTO_POWEROFF, // used in multiple places, incl. installer / uninstaller
    PROP_BUTTON_ASSIGNMENT,
    PROP_REMOTE_SW1,
    PROP_REMOTE_SW2,
    PROP_LV_DISPSIZE,  // mlv_lite crop mode: request Canon's 5x zoom (1:1 readout)
    PROP_SHUTTER       // shutter angle lock: the same request turning the dial makes; ML reads
                       // this property for its shutter display, so the ID and length are known good
};

// anything not listed above will allow reads but not writes

#endif
