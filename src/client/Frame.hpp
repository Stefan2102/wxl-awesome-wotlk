// Interface frames: putting one back on the script stack, and ordering it against its siblings.

#pragma once

#include "client/ClientOffsets.hpp"
#include "client/Lua.hpp"

#include "game/Binding.hpp"

namespace awow::client::frame
{
    namespace off = awow::client::offsets;

    /// An interface frame. Opaque: this module only hands one back to script code and reorders it.
    using Frame = void;

    /**
     * @brief Pushes the script object that stands for a frame.
     * @param state  Script state.
     * @param frame  Frame to push.
     *
     * Every frame the client creates is held by a reference in the script registry, so pushing the
     * frame means pushing that reference: script code then receives the very same object its own
     * CreateFrame handed back, with its methods and its stored fields intact.
     */
    inline void Push(lua::State* state, Frame* frame)
    {
        const int reference =
            wxl::game::Native<int(__thiscall*)(Frame*)>(off::kFrameGetRefTable)(frame);
        lua::RawGetI(state, lua::kRegistryIndex, reference);
    }

    /**
     * @brief Sets a frame's draw level within its strata.
     * @param frame  Frame to reorder.
     * @param level  New level; higher draws in front.
     */
    inline void SetLevel(Frame* frame, int level)
    {
        // The client's third argument selects whether children are relevelled with the parent; 1 is
        // what its own level assignments pass, and a nameplate carries the bars that must follow it.
        wxl::game::Native<void(__thiscall*)(Frame*, int, int)>(off::kFrameSetLevel)(frame, level, 1);
    }
}
