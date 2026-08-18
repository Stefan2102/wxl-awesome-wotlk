// Direct jump patches, for the two client seams that are not function entries.
//
// Everything this module detours goes through WarcraftXL's hook engine, which chains parties on one
// address and hands each a trampoline to the next. That model needs the target to be a function
// entry: the trampoline is built from the instructions the patch displaced, and calling it is how a
// detour reaches what it replaced.
//
// Two of the seams here are not that. They are anchors in the middle of a function, reached with a
// particular register and stack state, and the code that lands on them never continues to the
// original -- it does its work and jumps to a chosen instruction further along, so the displaced
// bytes are re-executed inline rather than through a trampoline. There is nothing for a chaining
// engine to hand out, and registering one would leave a trampoline nobody can legally call.
//
// So those two are patched here instead, which has a consequence worth stating plainly: a raw patch
// is module-private. A second party patching the same address wins or loses by install order, with
// no chain to arbitrate. That is why it is confined to these two anchors and nothing else.

#pragma once

#include <cstddef>
#include <cstdint>

namespace awow::hook
{
    /**
     * @brief A five-byte relative jump written over a client instruction boundary.
     *
     * Owns the bytes it displaced and puts them back when it goes out of scope, so a patch cannot
     * outlive the code it points at.
     */
    class JumpPatch
    {
    public:
        /// Size of the jump written, and therefore of the instruction bytes displaced.
        static constexpr size_t kSize = 5;

        JumpPatch() = default;
        ~JumpPatch();

        JumpPatch(const JumpPatch&)            = delete;
        JumpPatch& operator=(const JumpPatch&) = delete;

        /**
         * @brief Writes the jump.
         * @param target  Client address to patch. Must be an instruction boundary with at least
         *                kSize bytes of instructions that nothing else branches into.
         * @param detour  Where control goes instead.
         * @return true when the patch was written.
         */
        bool Install(uintptr_t target, const void* detour);

        /// Restores the displaced bytes. Idempotent; also run by the destructor.
        void Remove();

        /// @return true while the patch is in place.
        bool Installed() const { return m_target != 0; }

    private:
        uintptr_t m_target = 0;
        uint8_t   m_displaced[kSize] = {};
    };
}
