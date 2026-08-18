// The object manager: resolving GUIDs and unit tokens to world objects, and walking what is resident.

#pragma once

#include <cstdint>

#include "client/ClientOffsets.hpp"
#include "client/Descriptors.hpp"

#include "game/Binding.hpp"

namespace awow::client::objectmgr
{
    namespace off = awow::client::offsets;

    using Guid = uint64_t;

    /// One step of an object walk. Returning false stops the walk.
    using EnumStep = bool (*)(Guid guid, void* user);

    /**
     * @brief Walks every resident object.
     * @param step  Invoked once per object.
     * @param user  Opaque pointer handed back to @p step.
     *
     * Main thread only: the client reads the list head from thread-local storage, so off the main
     * thread this walks another thread's manager or none at all.
     */
    inline void EnumObjects(EnumStep step, void* user)
    {
        struct Bridge
        {
            EnumStep step;
            void*    user;

            // The client's callback convention is (guid, user) with an int result; the adapter exists
            // so callers can hand in a bool-returning function without knowing that.
            static int __cdecl Invoke(Guid guid, void* state)
            {
                const Bridge& bridge = *static_cast<const Bridge*>(state);
                return bridge.step(guid, bridge.user) ? 1 : 0;
            }
        };

        Bridge bridge{step, user};
        using EnumFn = int(__cdecl*)(int(__cdecl*)(Guid, void*), void*);
        wxl::game::Native<EnumFn>(off::kEnumObjects)(&Bridge::Invoke, &bridge);
    }

    /**
     * @brief Resolves a GUID to a resident object.
     * @param guid      Object GUID.
     * @param typeMask  Categories the caller accepts; anything else yields null.
     * @return The object, or null.
     */
    inline object::Object* Get(Guid guid, uint32_t typeMask)
    {
        // The client takes a diagnostic tag and a flag beyond the two arguments that matter. They are
        // passed explicitly rather than left to whatever the stack happened to hold: the tag reaches
        // the client's own assertion path, and a stale stack slot there is a pointer it would follow.
        using GetFn = object::Object*(__cdecl*)(Guid, uint32_t, const char*, int);
        return wxl::game::Native<GetFn>(off::kGetObjectByGuid)(guid, typeMask, "wxl-awesome-wotlk", 0);
    }

    /// @return The active player object, or null outside the world.
    inline object::Object* Player()
    { return wxl::game::Native<object::Object*(__cdecl*)()>(off::kGetPlayer)(); }

    /// @return The player's current target GUID, or 0.
    inline Guid TargetGuid() { return *reinterpret_cast<const Guid*>(off::kTargetGuid); }

    /**
     * @brief Resolves a unit token to a GUID through the client's token parser.
     * @param token  Unit token, such as "player" or "nameplate1".
     * @return The GUID, or 0 when the token names nothing.
     */
    inline Guid GuidByUnitToken(const char* token)
    { return wxl::game::Native<Guid(__cdecl*)(const char*)>(off::kGetGuidByUnitId)(token); }

    /**
     * @brief Resolves whatever a script passed as a unit: a token, or a GUID in text form.
     * @param text  Unit token or "0x..." GUID.
     * @return The GUID, or 0.
     */
    inline Guid GuidFromScriptArgument(const char* text)
    {
        if (!text) return 0;
        if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
            return wxl::game::Native<Guid(__cdecl*)(const char*)>(off::kHexStringToGuid)(text);
        return GuidByUnitToken(text);
    }

    /**
     * @brief Resolves whatever a script passed as a unit, straight to the object.
     * @param text      Unit token or "0x..." GUID.
     * @param typeMask  Categories the caller accepts.
     * @return The object, or null.
     */
    inline object::Object* GetByScriptArgument(const char* text, uint32_t typeMask)
    { return Get(GuidFromScriptArgument(text), typeMask); }
}
