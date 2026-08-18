// Installing one detour through the core's chained hook engine, with the failure reported.

#pragma once

#include <cstdint>

#include "Module.hpp"

namespace awow::hook
{
    /**
     * @brief Adds a detour to a client function, alongside any party already detouring it.
     * @param name      Label the core logs the chain under.
     * @param target    Client function to detour.
     * @param detour    Replacement function.
     * @param original  Receives the next link in the chain, which ends at the client function.
     * @return true when the detour was registered.
     *
     * Typed rather than void*-based, so wiring a detour to a trampoline of another signature does
     * not compile. A refusal is logged here: every caller would otherwise repeat the same message,
     * and a silently missing detour is a feature that does nothing for no stated reason.
     */
    template <class Fn>
    bool Attach(const char* name, uintptr_t target, Fn* detour, Fn** original)
    {
        const WXL_Api* api = Api();
        if (api && api->HookAttach(name, target, reinterpret_cast<void*>(detour),
                                   reinterpret_cast<void**>(original), WXL_HOOK_DEFAULT_PRIORITY))
            return true;

        Log(WXL_LOG_ERROR, "could not detour %s at 0x%08X", name, unsigned(target));
        return false;
    }
}
