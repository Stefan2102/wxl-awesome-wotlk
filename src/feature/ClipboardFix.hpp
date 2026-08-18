// Clipboard text that survives the round trip: the client's own path is ANSI.

#pragma once

namespace awow::feature::clipboardFix
{
    /**
     * @brief Replaces the client's clipboard read and write with UTF-8 equivalents.
     *
     * The client converts through the active ANSI code page in both directions, so any character
     * the code page cannot represent -- which is most of them, for most players -- comes back as
     * '?'. Copying a link out of the game and pasting a name into it are both affected.
     * @return true when both detours were installed.
     */
    bool Initialize();
}
