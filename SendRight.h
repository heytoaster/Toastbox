#pragma once
#include <mach/mach.h>
#include <cassert>
#include "RefCounted.h"

namespace Toastbox {

inline void _SendRightRetain(mach_port_t x) {
    if (MACH_PORT_VALID(x)) {
        kern_return_t kr = mach_port_mod_refs(mach_task_self(), x, MACH_PORT_RIGHT_SEND, 1);
        // KERN_INVALID_RIGHT is returned when the send right is actually a dead name,
        // so we need to handle that case specially, since we still want to retain it.
        assert(kr==KERN_SUCCESS || kr==KERN_INVALID_RIGHT);
        if (kr == KERN_INVALID_RIGHT) {
            kr = mach_port_mod_refs(mach_task_self(), x, MACH_PORT_RIGHT_DEAD_NAME, 1);
            assert(kr == KERN_SUCCESS);
        }
    }
}

inline void _SendRightRelease(mach_port_t x) {
    if (MACH_PORT_VALID(x)) {
        kern_return_t kr = mach_port_deallocate(mach_task_self(), x);
        assert(kr == KERN_SUCCESS);
    }
}

class SendRight : public RefCounted<mach_port_t, _SendRightRetain, _SendRightRelease> {
public:
    using RefCounted::RefCounted;
    bool valid() const { return hasValue() && MACH_PORT_VALID(*this); }
};

} // namespace Toastbox
