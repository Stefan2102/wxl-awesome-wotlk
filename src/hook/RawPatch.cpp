#include "hook/RawPatch.hpp"

#include <windows.h>

#include <cstring>

namespace awow::hook
{
    namespace
    {
        /// Opcode of the near relative jump, whose operand is signed and measured from the end of
        /// the instruction.
        constexpr uint8_t kJumpNearRelative = 0xE9;

        /**
         * @brief Overwrites client code, taking write access for the duration and giving it back.
         * @param target  Address to write at.
         * @param bytes   What to write.
         * @param size    How much.
         * @return true when the write went through.
         */
        bool WriteCode(uintptr_t target, const void* bytes, size_t size)
        {
            void* address = reinterpret_cast<void*>(target);

            DWORD previousProtection = 0;
            if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &previousProtection))
                return false;

            std::memcpy(address, bytes, size);

            // Restoring the original protection matters beyond tidiness: leaving a writable page in
            // the client's code section is exactly the pattern anti-cheat and crash reporters flag.
            DWORD ignored = 0;
            VirtualProtect(address, size, previousProtection, &ignored);

            // The bytes were written as data; the instruction cache has to be told the code changed.
            FlushInstructionCache(GetCurrentProcess(), address, size);
            return true;
        }
    }

    bool JumpPatch::Install(uintptr_t target, const void* detour)
    {
        if (m_target || !target || !detour) return false;

        std::memcpy(m_displaced, reinterpret_cast<const void*>(target), kSize);

        uint8_t patch[kSize];
        patch[0] = kJumpNearRelative;
        const int32_t displacement =
            int32_t(reinterpret_cast<uintptr_t>(detour) - target - kSize);
        std::memcpy(patch + 1, &displacement, sizeof displacement);

        if (!WriteCode(target, patch, kSize)) return false;

        m_target = target;
        return true;
    }

    void JumpPatch::Remove()
    {
        if (!m_target) return;
        WriteCode(m_target, m_displaced, kSize);
        m_target = 0;
    }

    JumpPatch::~JumpPatch() { Remove(); }
}
