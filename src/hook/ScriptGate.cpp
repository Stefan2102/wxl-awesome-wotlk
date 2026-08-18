#include "hook/ScriptGate.hpp"

#include "Module.hpp"

#include "game/Script.hpp"

#include <windows.h>

/// The linker's own symbol for the start of the image this translation unit is linked into. Using it
/// rather than GetModuleHandle keeps the bounds correct with no name to get wrong and no lookup to
/// fail, and it is resolved at link time, so the check below costs two comparisons.
extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace awow::hook::scriptgate
{
    namespace
    {
        /// The core's name for the callback bounds check, from its hook-point table.
        constexpr const char* kHookPoint = "Lua.ValidateFunctionPointer";

        wxl::game::script::ValidateCallbackFn g_original = nullptr;

        uintptr_t g_imageBegin = 0;
        uintptr_t g_imageEnd   = 0;

        /// Records this module's address range, once.
        void MeasureImage()
        {
            const auto* dos = &__ImageBase;
            const auto* nt  = reinterpret_cast<const IMAGE_NT_HEADERS*>(
                reinterpret_cast<const uint8_t*>(dos) + dos->e_lfanew);

            g_imageBegin = reinterpret_cast<uintptr_t>(dos);
            g_imageEnd   = g_imageBegin + nt->OptionalHeader.SizeOfImage;
        }

        void __cdecl ValidateDetour(uintptr_t function)
        {
            // Ours: admitted by returning, which is what the engine does for a pointer it accepts.
            if (function >= g_imageBegin && function < g_imageEnd) return;
            g_original(function);
        }
    }

    bool Initialize()
    {
        const WXL_Api* api = Api();
        if (!api) return false;

        MeasureImage();

        if (!api->HookAttachByName(kHookPoint, reinterpret_cast<void*>(&ValidateDetour),
                                   reinterpret_cast<void**>(&g_original),
                                   WXL_HOOK_DEFAULT_PRIORITY))
        {
            Log(WXL_LOG_ERROR,
                "could not attach to '%s'; script functions would be refused at their first call",
                kHookPoint);
            return false;
        }
        return true;
    }
}
