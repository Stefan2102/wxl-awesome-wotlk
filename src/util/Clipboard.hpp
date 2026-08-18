// Reading and writing the Windows clipboard as UTF-8.

#pragma once

#include <string>

#include <windows.h>

namespace awow::util::clipboard
{
    /**
     * @brief Reads the clipboard's text.
     * @param owner  Window to open the clipboard on behalf of; null for the calling task.
     * @return The text as UTF-8, or empty when the clipboard holds no text, cannot be opened, or
     *         holds a sequence that is not valid Unicode.
     */
    std::string ReadUtf8(HWND owner);

    /**
     * @brief Replaces the clipboard's contents with text.
     * @param text   UTF-8 text; null or empty empties the clipboard instead.
     * @param owner  Window to open the clipboard on behalf of; null for the calling task.
     * @return true when the clipboard was updated.
     */
    bool WriteUtf8(const char* text, HWND owner);
}
