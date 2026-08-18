#include "feature/ClipboardFix.hpp"

#include "client/ClientOffsets.hpp"
#include "hook/Attach.hpp"
#include "util/Clipboard.hpp"

#include "game/Binding.hpp"

#include <windows.h>

#include <cstring>
#include <string>

namespace awow::feature::clipboardFix
{
    namespace off = awow::client::offsets;

    namespace
    {
        off::ClipboardGetStringFn g_originalGetString = nullptr;
        off::ClipboardSetStringFn g_originalSetString = nullptr;

        /**
         * @brief Reads the clipboard as UTF-8 into a buffer the client will free.
         *
         * The client frees what this returns through its own allocator, so the buffer has to come
         * from the same one -- a CRT allocation handed back here would be freed against the wrong
         * heap. On an allocation failure the original is called instead, which leaves the player
         * with the client's ANSI behaviour rather than with nothing.
         */
        const char* __cdecl GetStringDetour(void* window)
        {
            const std::string text = util::clipboard::ReadUtf8(static_cast<HWND>(window));

            const size_t bytes  = text.size() + 1;
            char*        buffer = static_cast<char*>(
                wxl::game::Native<off::AllocFn>(off::kAlloc)(bytes));
            if (!buffer) return g_originalGetString(window);

            std::memcpy(buffer, text.c_str(), bytes);
            return buffer;
        }

        /// Writes the clipboard as UTF-8. The original is not called: this replaces it outright.
        int __cdecl SetStringDetour(const char* text, void* window)
        {
            return util::clipboard::WriteUtf8(text, static_cast<HWND>(window)) ? TRUE : FALSE;
        }
    }

    bool Initialize()
    {
        bool complete = true;
        complete &= hook::Attach("Clipboard_GetString", off::kClipboardGetString, &GetStringDetour,
                                 &g_originalGetString);
        complete &= hook::Attach("Clipboard_SetString", off::kClipboardSetString, &SetStringDetour,
                                 &g_originalSetString);
        return complete;
    }
}
