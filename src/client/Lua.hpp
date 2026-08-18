// The Lua 5.1 C API as the client exports it, for the half wxl::game::script does not bind.
//
// wxl-core's SDK covers reading a call's arguments and pushing its results, which is all most
// modules need. This one also builds tables (C_NamePlate), keeps per-state data in the registry
// (the nameplate list), and turns frame pointers back into script objects, so it needs the table,
// registry and userdata operations too. The overlap with the SDK is re-exported below rather than
// re-bound, so there is exactly one definition of each entry in the process.

#pragma once

#include <cstddef>

#include "client/ClientOffsets.hpp"

#include "game/Binding.hpp"
#include "game/Script.hpp"

namespace awow::client::lua
{
    namespace off = awow::client::offsets;

    /// A script state. Opaque: nothing in this module reaches into it.
    using State = void;

    /// A script function, as the globals table holds it.
    using Function = wxl::game::script::Function;

    // Pseudo-indices. Negative and far below any real stack slot, which is how the API tells them
    // apart from one.
    inline constexpr int kRegistryIndex = -10000;
    inline constexpr int kGlobalsIndex  = -10002;

    // Value type ids, as lua_type reports them. Only the ones this module tests against.
    inline constexpr int kTypeNil           = 0;
    inline constexpr int kTypeLightUserData = 2;
    inline constexpr int kTypeUserData      = 7;

    // --- from the core SDK, not re-bound ---------------------------------------------------------
    using wxl::game::script::PushNumber;
    using wxl::game::script::PushString;

    // --- state -------------------------------------------------------------------------------------

    /**
     * @brief Returns the active FrameScript state.
     * @return The state, or null before script initialisation. The pointer changes across a reload,
     *         so nothing may cache it beyond the call that read it.
     */
    inline State* GetState()
    { return wxl::game::Native<off::GetScriptStateFn>(off::kGetScriptState)(); }

    // --- stack ---------------------------------------------------------------------------------------

    /**
     * @brief Sets the stack top.
     * @param state  Script state.
     * @param index  New top; a negative index counts from the current top.
     */
    inline void SetTop(State* state, int index)
    { wxl::game::Native<void(__cdecl*)(State*, int)>(off::kLuaSetTop)(state, index); }

    /**
     * @brief Drops values from the top of the stack.
     * @param state  Script state.
     * @param count  How many to drop.
     */
    inline void Pop(State* state, int count) { SetTop(state, -count - 1); }

    /**
     * @brief Reports a value's type id.
     * @param state  Script state.
     * @param index  Stack index.
     */
    inline int Type(State* state, int index)
    { return wxl::game::Native<int(__cdecl*)(State*, int)>(off::kLuaType)(state, index); }

    /**
     * @brief Reports whether a value is userdata, full or light.
     * @param state  Script state.
     * @param index  Stack index.
     */
    inline bool IsUserData(State* state, int index)
    {
        const int type = Type(state, index);
        return type == kTypeUserData || type == kTypeLightUserData;
    }

    /**
     * @brief Reads a userdata value's payload pointer.
     * @param state  Script state.
     * @param index  Stack index.
     * @return The payload, or null when the value is not userdata.
     */
    inline void* ToUserData(State* state, int index)
    { return wxl::game::Native<void*(__cdecl*)(State*, int)>(off::kLuaToUserData)(state, index); }

    // --- arguments -----------------------------------------------------------------------------------

    /**
     * @brief Reads a string argument, raising a script error when it is not one.
     * @param state  Script state.
     * @param index  One-based argument index.
     * @return The text, owned by the state and valid only until the call returns.
     */
    inline const char* CheckString(State* state, int index)
    {
        return wxl::game::Native<const char*(__cdecl*)(State*, int, size_t*)>(
            off::kLuaLCheckLString)(state, index, nullptr);
    }

    /**
     * @brief Reads a number argument, raising a script error when it is not one.
     * @param state  Script state.
     * @param index  One-based argument index.
     */
    inline double CheckNumber(State* state, int index)
    {
        return wxl::game::Native<double(__cdecl*)(State*, int)>(off::kLuaLCheckNumber)(state, index);
    }

    // --- pushing ---------------------------------------------------------------------------------------

    /**
     * @brief Pushes a function with no upvalues.
     * @param state     Script state.
     * @param function  Function to push. It must have been admitted through the callback-validation
     *                  seam first, or the engine refuses to invoke it (see hook/ScriptGate.hpp).
     */
    inline void PushFunction(State* state, Function function)
    {
        wxl::game::Native<void(__cdecl*)(State*, Function, int)>(off::kLuaPushCClosure)(state, function, 0);
    }

    /**
     * @brief Pushes a new table.
     * @param state        Script state.
     * @param arrayHint    Expected array-part size, for pre-sizing only.
     * @param recordHint   Expected hash-part size, for pre-sizing only.
     */
    inline void CreateTable(State* state, int arrayHint, int recordHint)
    {
        wxl::game::Native<void(__cdecl*)(State*, int, int)>(off::kLuaCreateTable)(state, arrayHint, recordHint);
    }

    /**
     * @brief Pushes userdata of the given size and returns its payload.
     * @param state  Script state.
     * @param size   Payload bytes.
     * @return The payload, uninitialised.
     */
    inline void* NewUserData(State* state, size_t size)
    { return wxl::game::Native<void*(__cdecl*)(State*, size_t)>(off::kLuaNewUserData)(state, size); }

    // --- tables ------------------------------------------------------------------------------------------

    /**
     * @brief Pushes t[key] for the table at @p index.
     * @param state  Script state.
     * @param index  Stack index of the table, or a pseudo-index.
     * @param key    Field name.
     */
    inline void GetField(State* state, int index, const char* key)
    { wxl::game::Native<void(__cdecl*)(State*, int, const char*)>(off::kLuaGetField)(state, index, key); }

    /**
     * @brief Assigns t[key] from the value on top of the stack, popping it.
     * @param state  Script state.
     * @param index  Stack index of the table, or a pseudo-index.
     * @param key    Field name.
     */
    inline void SetField(State* state, int index, const char* key)
    { wxl::game::Native<void(__cdecl*)(State*, int, const char*)>(off::kLuaSetField)(state, index, key); }

    /**
     * @brief Pushes t[k] for the table at @p index, with the key on top of the stack, popping it.
     *        Raw: no metamethod runs, so the lookup cannot raise.
     * @param state  Script state.
     * @param index  Stack index of the table, or a pseudo-index.
     */
    inline void RawGet(State* state, int index)
    { wxl::game::Native<void(__cdecl*)(State*, int)>(off::kLuaRawGet)(state, index); }

    /**
     * @brief Pushes t[n] for the table at @p index, without consulting metamethods.
     * @param state     Script state.
     * @param index     Stack index of the table, or a pseudo-index.
     * @param position  Integer key.
     */
    inline void RawGetI(State* state, int index, int position)
    { wxl::game::Native<void(__cdecl*)(State*, int, int)>(off::kLuaRawGetI)(state, index, position); }

    /**
     * @brief Assigns t[n] from the value on top of the stack, popping it, without metamethods.
     * @param state     Script state.
     * @param index     Stack index of the table.
     * @param position  Integer key.
     */
    inline void RawSetI(State* state, int index, int position)
    { wxl::game::Native<void(__cdecl*)(State*, int, int)>(off::kLuaRawSetI)(state, index, position); }

    /**
     * @brief Assigns the metatable on top of the stack to the value at @p index, popping it.
     * @param state  Script state.
     * @param index  Stack index of the value.
     */
    inline void SetMetatable(State* state, int index)
    { wxl::game::Native<int(__cdecl*)(State*, int)>(off::kLuaSetMetatable)(state, index); }

    // --- globals ---------------------------------------------------------------------------------------

    /**
     * @brief Assigns a global from the value on top of the stack, popping it.
     * @param state  Script state.
     * @param name   Global name.
     */
    inline void SetGlobal(State* state, const char* name) { SetField(state, kGlobalsIndex, name); }

    /**
     * @brief Registers a global script function.
     * @param state     Script state.
     * @param name      Name it is called by.
     * @param function  Function invoked.
     */
    inline void SetGlobalFunction(State* state, const char* name, Function function)
    {
        PushFunction(state, function);
        SetGlobal(state, name);
    }
}
