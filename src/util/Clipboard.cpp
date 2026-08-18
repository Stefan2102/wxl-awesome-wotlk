#include "util/Clipboard.hpp"

#include <cstring>
#include <cwchar>

namespace awow::util::clipboard
{
    namespace
    {
        /**
         * @brief Opens the clipboard and closes it again, whatever happens in between.
         *
         * The clipboard is a process-wide lock held by whoever opened it last. Leaving it open on an
         * early return would deny it to every other application until this process exits, so the
         * close is tied to a scope rather than written at each exit.
         */
        class ClipboardScope
        {
        public:
            explicit ClipboardScope(HWND owner) : m_open(OpenClipboard(owner) != FALSE) {}
            ~ClipboardScope() { if (m_open) CloseClipboard(); }

            ClipboardScope(const ClipboardScope&)            = delete;
            ClipboardScope& operator=(const ClipboardScope&) = delete;

            bool IsOpen() const { return m_open; }

        private:
            bool m_open;
        };

        /**
         * @brief Locks a global memory block and unlocks it again.
         *
         * Same reasoning as above: a block that stays locked cannot be moved or discarded, and the
         * conversion paths here have several ways to fail partway through.
         */
        class GlobalLockScope
        {
        public:
            explicit GlobalLockScope(HGLOBAL block)
                : m_block(block), m_data(block ? GlobalLock(block) : nullptr)
            {}
            ~GlobalLockScope() { if (m_data) GlobalUnlock(m_block); }

            GlobalLockScope(const GlobalLockScope&)            = delete;
            GlobalLockScope& operator=(const GlobalLockScope&) = delete;

            void* Data() const { return m_data; }

        private:
            HGLOBAL m_block;
            void*   m_data;
        };
    }

    std::string ReadUtf8(HWND owner)
    {
        ClipboardScope clipboard(owner);
        if (!clipboard.IsOpen()) return {};

        HANDLE block = GetClipboardData(CF_UNICODETEXT);
        if (!block) return {};

        GlobalLockScope locked(block);
        const auto* wide = static_cast<const wchar_t*>(locked.Data());
        if (!wide) return {};

        // Lengths here count the terminator, so the conversion produces a terminated result and the
        // string is trimmed back to its text afterwards.
        const int wideLength = int(std::wcslen(wide)) + 1;
        const int utf8Length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, wideLength,
                                                   nullptr, 0, nullptr, nullptr);
        // WC_ERR_INVALID_CHARS makes an unpaired surrogate a failure rather than a silent U+FFFD.
        // The clipboard is arbitrary input from another process, and text the game cannot represent
        // is better dropped than pasted as replacement characters.
        if (utf8Length <= 0) return {};

        std::string text(size_t(utf8Length), '\0');
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, wideLength, text.data(), utf8Length,
                            nullptr, nullptr);
        text.resize(size_t(utf8Length) - 1);
        return text;
    }

    bool WriteUtf8(const char* text, HWND owner)
    {
        if (!text || !text[0])
        {
            ClipboardScope clipboard(owner);
            return clipboard.IsOpen() && EmptyClipboard() != FALSE;
        }

        const int utf8Length = int(std::strlen(text)) + 1;
        const int wideLength = MultiByteToWideChar(CP_UTF8, 0, text, utf8Length, nullptr, 0);
        if (wideLength <= 0) return false;

        HGLOBAL block = GlobalAlloc(GMEM_MOVEABLE, sizeof(wchar_t) * size_t(wideLength));
        if (!block) return false;

        {
            GlobalLockScope locked(block);
            auto* wide = static_cast<wchar_t*>(locked.Data());
            if (!wide)
            {
                GlobalFree(block);
                return false;
            }
            // wideLength counts the terminator the source carried, so the block comes out
            // terminated; writing one more past it would run off the end of the allocation.
            MultiByteToWideChar(CP_UTF8, 0, text, utf8Length, wide, wideLength);
        }

        ClipboardScope clipboard(owner);
        if (!clipboard.IsOpen() || !EmptyClipboard() || !SetClipboardData(CF_UNICODETEXT, block))
        {
            GlobalFree(block);
            return false;
        }

        // SetClipboardData succeeded, so the system owns the block now and freeing it here would
        // leave the clipboard holding a dangling handle.
        return true;
    }
}
