#include "feature/Window.hpp"

#include "client/ClientOffsets.hpp"
#include "client/Lua.hpp"
#include "hook/Registry.hpp"
#include "util/Clipboard.hpp"

#include <windows.h>

namespace awow::feature::window
{
    namespace lua = awow::client::lua;
    namespace off = awow::client::offsets;

    namespace
    {
        /// @return The game's window, or null before it is created.
        HWND GameWindow() { return *reinterpret_cast<HWND*>(off::kGameWindowPtr); }

        /// FlashWindow(): draws attention to the taskbar button until the window is next activated.
        int __cdecl FlashGameWindow(lua::State*)
        {
            if (HWND window = GameWindow()) ::FlashWindow(window, FALSE);
            return 0;
        }

        /// IsWindowFocused() -> 1 or nil. Nothing is pushed for false, matching the stock API style.
        int __cdecl IsWindowFocused(lua::State* state)
        {
            HWND window = GameWindow();
            if (!window || GetForegroundWindow() != window) return 0;

            lua::PushNumber(state, 1.0);
            return 1;
        }

        /// FocusWindow(): raises the game window, subject to the system's own foreground rules.
        int __cdecl FocusWindow(lua::State*)
        {
            if (HWND window = GameWindow()) SetForegroundWindow(window);
            return 0;
        }

        /// CopyToClipboard(text): puts text on the clipboard as UTF-8.
        int __cdecl CopyToClipboard(lua::State* state)
        {
            const char* text = lua::CheckString(state, 1);
            if (text && text[0]) util::clipboard::WriteUtf8(text, nullptr);
            return 0;
        }

        void OpenLibrary(lua::State* state)
        {
            lua::SetGlobalFunction(state, "FlashWindow",      &FlashGameWindow);
            lua::SetGlobalFunction(state, "IsWindowFocused",  &IsWindowFocused);
            lua::SetGlobalFunction(state, "FocusWindow",      &FocusWindow);
            lua::SetGlobalFunction(state, "CopyToClipboard",  &CopyToClipboard);
        }
    }

    void Initialize() { hook::registry::AddScriptLibrary(&OpenLibrary); }
}
