// Admitting this module's script functions past the engine's callback bounds check.

#pragma once

namespace awow::hook::scriptgate
{
    /**
     * @brief Installs the detour that lets this module's script functions be invoked.
     *
     * Before it calls a script callback, the engine checks the pointer against the bounds of its own
     * image. A function compiled into an extension lies outside them by construction, so without
     * this the first call to any of the functions registered here is refused with a fatal error.
     *
     * The check is only bypassed for pointers inside this module's image; every other pointer still
     * reaches the engine's own test, which is doing its job for the calls it was written for.
     * @return true when the detour was installed.
     */
    bool Initialize();
}
