#include "hook/Registry.hpp"

#include "Module.hpp"
#include "client/ClientOffsets.hpp"
#include "hook/Attach.hpp"
#include "hook/RawPatch.hpp"

#include "game/Binding.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

#if !defined(_M_IX86)
#error "This module targets the 32-bit client and contains x86 inline assembly."
#endif

// --- the unit-token seam, which is reached from inline assembly --------------------------------
//
// Apart from the two ABI entry points, these are the only symbols in this module with C linkage.
// They are named from the naked stub below, and the inline assembler resolves an unmangled name far
// more predictably than a mangled one -- a stub that assembles against the wrong symbol is a stack
// corruption rather than a compile error, so the names are made unambiguous rather than merely
// likely to work. Both carry the module prefix, since C linkage puts them in a shared namespace.

/// Where the client's token parser resumes: written by the dispatcher, read by the stub.
///
/// A variable rather than a return value, because the stub restores every register before it jumps
/// and nothing the dispatcher returned would survive to be jumped through. It is safe because the
/// parser is main-thread-only, and the write and the read are the same call.
extern "C" uintptr_t awow_tokenResume = 0;

/**
 * @brief Matches a unit token this module added, and resolves it.
 * @param cursor  Points at the parser's own token pointer; advanced past a token that matched.
 * @param out     Receives the GUID, or 0 when the token matched but names nothing.
 */
extern "C" void __cdecl awow_TokenDispatch(const char** cursor, awow::client::objectmgr::Guid* out);

namespace awow::hook::registry
{
    namespace off = awow::client::offsets;
    namespace lua = awow::client::lua;

    namespace
    {
        // --- console variables ----------------------------------------------------------------

        struct PendingVariable
        {
            client::console::Variable**        slot;
            const char*                        name;
            const char*                        description;
            client::console::Flags             flags;
            const char*                        initialValue;
            client::console::Variable::Handler handler;
        };

        std::vector<PendingVariable> g_variables;
        off::CVarsInitializeFn       g_originalCVarsInitialize = nullptr;

        void __cdecl CVarsInitializeDetour()
        {
            g_originalCVarsInitialize();

            for (const PendingVariable& pending : g_variables)
            {
                client::console::Variable* variable =
                    client::console::Register(pending.name, pending.description, pending.flags,
                                              pending.initialValue, pending.handler);
                if (pending.slot) *pending.slot = variable;
                if (!variable)
                    Log(WXL_LOG_WARN, "console variable '%s' was refused; one of that name exists",
                        pending.name);
            }
        }

        // --- event names ------------------------------------------------------------------------

        std::vector<const char*>     g_events;
        off::FrameScriptFillEventsFn g_originalFillEvents = nullptr;

        void __cdecl FillEventsDetour(const char** names, size_t count)
        {
            // The client keeps the name pointers, not the array, so the joined array is free to be
            // a local: what has to outlive the call is the strings, and both sides supply literals.
            std::vector<const char*> joined;
            joined.reserve(count + g_events.size());
            joined.insert(joined.end(), names, names + count);
            joined.insert(joined.end(), g_events.begin(), g_events.end());

            g_originalFillEvents(joined.data(), joined.size());
        }

        // --- script libraries ---------------------------------------------------------------------

        /// Set on every script state this module has served, and the witness that says so -- which
        /// is why it is assigned after the libraries run and never by one of them.
        ///
        /// The name is the one awesome_wotlk published, kept verbatim: addons written against that
        /// library test for it to decide whether these APIs are present.
        constexpr const char* kMarkerGlobal = "AwesomeWotlk";
        constexpr double      kMarkerValue  = 1.0;

        std::vector<ScriptLibrary> g_libraries;
        bool                       g_installing = false;

        using RegisterFunctionFn = void(__cdecl*)(const char* name, lua::Function function);
        RegisterFunctionFn g_originalRegisterFunction = nullptr;

        /// @return true when this module's globals are already on @p state.
        bool AlreadyServed(lua::State* state)
        {
            // Raw, so no metamethod on the globals table can run and no lookup can raise: this runs
            // from inside a client detour, where a script error has nowhere to go.
            lua::PushString(state, kMarkerGlobal);
            lua::RawGet(state, lua::kGlobalsIndex);
            const bool present = lua::Type(state, -1) != lua::kTypeNil;
            lua::Pop(state, 1);
            return present;
        }

        /**
         * @brief Installs this module's globals on the current script state, unless they are there.
         *
         * Idempotent, and called from two places on purpose. The registration seam is the earlier of
         * the two, and is what puts the globals in place before FrameXML and the addons run, which
         * is when they have to exist. The per-frame seam is the guarantee: it costs one raw table
         * lookup, and it means a state this module somehow did not see registered still gets served.
         */
        void EnsureLibraries()
        {
            if (g_installing) return;

            lua::State* state = lua::GetState();
            if (!state || AlreadyServed(state)) return;

            g_installing = true;
            for (ScriptLibrary open : g_libraries) open(state);
            lua::PushNumber(state, kMarkerValue);
            lua::SetGlobal(state, kMarkerGlobal);
            g_installing = false;
        }

        void __cdecl RegisterFunctionDetour(const char* name, lua::Function function)
        {
            g_originalRegisterFunction(name, function);
            EnsureLibraries();
        }

        // --- per-frame callbacks -----------------------------------------------------------------

        std::vector<UpdateCallback>    g_updateCallbacks;
        off::FrameScriptFireOnUpdateFn g_originalFireOnUpdate = nullptr;

        int __cdecl FireOnUpdateDetour(int a1, int a2, int a3, int a4)
        {
            EnsureLibraries();
            for (UpdateCallback callback : g_updateCallbacks) callback();
            return g_originalFireOnUpdate(a1, a2, a3, a4);
        }

        // --- unit tokens --------------------------------------------------------------------------

        struct IndexedToken
        {
            const char*  text;
            size_t       length;
            GuidForIndex guidFor;
            IndexForGuid indexFor;
        };

        std::vector<IndexedToken> g_tokens;
        JumpPatch                 g_tokenPatch;
        off::GetKeywordsByGuidFn  g_originalGetKeywords = nullptr;

        /**
         * @brief The stub the client's token parser jumps to.
         *
         * Naked because it runs at an anchor inside the parser rather than at a call: every register
         * and flag has to reach the resume point exactly as it was, so the dispatcher is bracketed
         * by a full save and restore. The two stack reads are the parser's own frame -- the token
         * pointer at [ebp+8], the GUID output at [ebp+0xC] -- and the address control resumes at was
         * chosen by the dispatcher.
         */
        __declspec(naked) void TokenAnchorStub()
        {
            __asm {
                pushad
                pushfd

                push [ebp + 0x0C]      // guid out
                lea eax, [ebp + 0x08]  // &token cursor
                push eax
                call awow_TokenDispatch
                add esp, 8

                popfd
                popad

                push awow_tokenResume
                ret
            }
        }

        char** __cdecl GetKeywordsDetour(uint64_t* guid, size_t* count)
        {
            char** buffer = g_originalGetKeywords(guid, count);
            if (!buffer) return buffer;

            for (const IndexedToken& token : g_tokens)
            {
                if (*count >= off::kKeywordBufferSlots) break;
                const int index = token.indexFor(*guid);
                if (index < 0) continue;
                std::snprintf(buffer[(*count)++], off::kKeywordSlotBytes, "%s%d", token.text,
                              index + 1);
            }
            return buffer;
        }
    }

    // Defined here rather than beside the other detours because its C linkage puts its name outside
    // every namespace: the naked stub above calls it, and a stub can only name what the assembler
    // sees unmangled.
    extern "C" void __cdecl awow_TokenDispatch(const char** cursor,
                                               awow::client::objectmgr::Guid* out)
    {
        for (const IndexedToken& token : g_tokens)
        {
            if (std::strncmp(*cursor, token.text, token.length) != 0) continue;

            // Past the prefix, the client's own number parser reads the index and leaves the cursor
            // after it, which is what tells "nameplate1" apart from "nameplate11".
            *cursor += token.length;
            const int index = wxl::game::Native<off::AtoiFn>(off::kAtoi)(cursor);

            *out = index > 0 ? token.guidFor(index - 1) : 0;
            awow_tokenResume = off::kGetGuidByKeywordHandled;
            return;
        }

        awow_tokenResume = off::kGetGuidByKeywordUnhandled;
    }

    void AddVariable(client::console::Variable** slot, const char* name, const char* description,
                     client::console::Flags flags, const char* initialValue,
                     client::console::Variable::Handler handler)
    {
        g_variables.push_back(PendingVariable{slot, name, description, flags, initialValue, handler});
    }

    void AddEvent(const char* name) { g_events.push_back(name); }

    void AddScriptLibrary(ScriptLibrary open) { g_libraries.push_back(open); }

    void AddIndexedToken(const char* token, GuidForIndex guidFor, IndexForGuid indexFor)
    {
        g_tokens.push_back(IndexedToken{token, std::strlen(token), guidFor, indexFor});
    }

    void AddUpdateCallback(UpdateCallback callback) { g_updateCallbacks.push_back(callback); }

    bool Initialize()
    {
        bool complete = true;

        if (!g_variables.empty())
            complete &= Attach("CVars_Initialize", off::kCVarsInitialize, &CVarsInitializeDetour,
                               &g_originalCVarsInitialize);

        if (!g_events.empty())
            complete &= Attach("FrameScript_FillEvents", off::kFrameScriptFillEvents,
                               &FillEventsDetour, &g_originalFillEvents);

        if (!g_libraries.empty())
            complete &= Attach("FrameScript_RegisterFunction", off::kFrameScriptRegisterFunction,
                               &RegisterFunctionDetour, &g_originalRegisterFunction);

        // The per-frame seam also carries the script-library guarantee, so it goes in when either
        // of the two has something to do.
        if (!g_updateCallbacks.empty() || !g_libraries.empty())
            complete &= Attach("FrameScript_FireOnUpdate", off::kFrameScriptFireOnUpdate,
                               &FireOnUpdateDetour, &g_originalFireOnUpdate);

        if (!g_tokens.empty())
        {
            complete &= Attach("GetKeywordsByGuid", off::kGetKeywordsByGuid, &GetKeywordsDetour,
                               &g_originalGetKeywords);

            if (!g_tokenPatch.Install(off::kGetGuidByKeywordAnchor,
                                      reinterpret_cast<const void*>(&TokenAnchorStub)))
            {
                Log(WXL_LOG_ERROR,
                    "could not patch the unit-token parser at 0x%08X; added tokens will not resolve",
                    unsigned(off::kGetGuidByKeywordAnchor));
                complete = false;
            }
        }

        return complete;
    }
}
