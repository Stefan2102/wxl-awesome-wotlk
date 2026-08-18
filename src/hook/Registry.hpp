// The client-side extension points every feature of this module hangs off.
//
// The client does not offer a way to add a console variable, an event name, a script global or a
// unit token: each is built from a fixed table during startup. So each has one detour, installed
// once, that folds in whatever the features asked for. Features declare what they want here and
// never touch a seam themselves, which is what keeps a single detour per seam no matter how many
// features want a piece of it.
//
// Declaration order: every Add* below only appends to a list, so features register first and
// Initialize() wires the seams afterwards. Nothing here calls into the client before Initialize().

#pragma once

#include "client/Console.hpp"
#include "client/Lua.hpp"
#include "client/ObjectMgr.hpp"

namespace awow::hook::registry
{
    /// Adds this module's globals to a freshly initialised script state.
    using ScriptLibrary = void (*)(client::lua::State* state);

    /// Runs once per frame, from the client's own OnUpdate dispatch.
    using UpdateCallback = void (*)();

    /// Resolves the zero-based index of a numbered unit token to a GUID; 0 when it names nothing.
    using GuidForIndex = client::objectmgr::Guid (*)(int index);

    /// The reverse: the zero-based index a GUID currently occupies, or -1 when it occupies none.
    using IndexForGuid = int (*)(client::objectmgr::Guid guid);

    /**
     * @brief Declares a console variable, created once the client's own set exists.
     * @param slot          Receives the created variable, or null when the caller does not need it.
     * @param name          Name it is set by.
     * @param description   Help text, or null.
     * @param flags         Registration flags.
     * @param initialValue  Value the change handler is first called with.
     * @param handler       Invoked on every assignment, including that first one.
     *
     * @p name, @p description and @p initialValue are kept by pointer, and the client keeps them for
     * the process lifetime, so they must have static storage.
     */
    void AddVariable(client::console::Variable** slot, const char* name, const char* description,
                     client::console::Flags flags, const char* initialValue,
                     client::console::Variable::Handler handler);

    /**
     * @brief Declares an event name, appended to the client's event table when it is built.
     * @param name  Event name; must have static storage, because the table keeps the pointer.
     */
    void AddEvent(const char* name);

    /**
     * @brief Declares a script library, run against every script state the client brings up.
     * @param open  Invoked with the state, to install globals on it.
     *
     * Also run again after a reload: the client discards its script state and builds a new one, and
     * anything installed on the old one goes with it.
     */
    void AddScriptLibrary(ScriptLibrary open);

    /**
     * @brief Declares a numbered unit token, such as "nameplate" for "nameplate1".
     * @param token     Token prefix, without the number; must have static storage.
     * @param guidFor   Resolves an index to a GUID.
     * @param indexFor  Resolves a GUID back to an index, for the client's own reverse lookup.
     */
    void AddIndexedToken(const char* token, GuidForIndex guidFor, IndexForGuid indexFor);

    /**
     * @brief Declares a per-frame callback.
     * @param callback  Invoked once per frame, before the client's own OnUpdate scripts run.
     */
    void AddUpdateCallback(UpdateCallback callback);

    /**
     * @brief Installs the detours for whichever of the above were declared.
     * @return true when every seam a declaration needs was installed.
     *
     * A seam nothing declared against is left alone, so a module that wants none of this patches
     * nothing.
     */
    bool Initialize();
}
