// Curated addresses of the 3.3.5a (build 12340) client, for the parts of it WarcraftXL's own
// offsets do not cover.
//
// WarcraftXL keeps its client landmarks in wxl-core/src/offsets and deliberately hides them from
// extensions: a module that reaches for a raw address is a module that breaks on a client rebase.
// The core's curated set is aimed at the renderer and the asset pipeline, though, and this module
// lives in the script/interface half of the client -- the console variable registry, the FrameScript
// event table, the unit-token parser, frame reference tables, the unit and player descriptor blocks.
// None of that is curated upstream, so it is curated here instead, in one place, with the same
// discipline: every constant named, every constant explained.
//
// All addresses are absolute virtual addresses in the client's fixed 0x00400000 image base. They are
// valid for build 12340 and nothing else, which is why WXL_CLIENT_BUILD gates the load.

#pragma once

#include <cstddef>
#include <cstdint>

namespace awow::client::offsets
{
    // --- process-wide state ----------------------------------------------------------------------

    /// -> the game's top-level HWND. Null until the window is created.
    inline constexpr uintptr_t kGameWindowPtr = 0x00D41620;

    /// Byte flag: non-zero once the player is in the world (past the loading screen).
    inline constexpr uintptr_t kInWorldFlag = 0x00BD0792;

    // --- client runtime helpers ------------------------------------------------------------------

    /// The client's own string-to-int parser. It advances the caller's pointer past the digits it
    /// consumed, which is what makes it usable for splitting a "<token><n>" unit id.
    inline constexpr uintptr_t kAtoi = 0x0076F190;
    using AtoiFn = int(__stdcall*)(const char** cursor);

    /// The string hash the console registry and the event table both key on.
    inline constexpr uintptr_t kStringHash = 0x0076F640;
    using StringHashFn = uint32_t(__stdcall*)(const char* text);

    /// The client's allocator. A buffer handed back to the client (see the clipboard read detour)
    /// has to come from here: the client releases it through its own matching free.
    inline constexpr uintptr_t kAlloc = 0x00415074;
    using AllocFn = void*(__cdecl*)(size_t size);

    // --- console variables -----------------------------------------------------------------------

    /// Registers the client's own console variables, once, during startup. Detoured post-call: the
    /// stock set exists by then, so an added variable cannot be shadowed by one registered later.
    inline constexpr uintptr_t kCVarsInitialize = 0x0051D9B0;
    using CVarsInitializeFn = void(__cdecl*)();

    /// Creates a console variable and returns it. The four trailing arguments are unused by the
    /// registration path this module needs and are passed as zero.
    inline constexpr uintptr_t kRegisterCVar = 0x00767FC0;


    // --- script state and the Lua C API ------------------------------------------------------------

    // The client embeds Lua 5.1. wxl::game::script (wxl-core/src/game/Script.hpp) already binds the
    // argument readers and the value pushers, and this module uses those rather than repeating them;
    // what follows is only the table/registry/userdata half it does not cover, which the nameplate
    // registry and the C_NamePlate table both need.

    /// The active FrameScript Lua state, or null before script initialisation. Changes on /reload.
    inline constexpr uintptr_t kGetScriptState = 0x00817DB0;
    using GetScriptStateFn = void*(__cdecl*)();

    inline constexpr uintptr_t kLuaSetTop       = 0x0084DBF0;
    inline constexpr uintptr_t kLuaType         = 0x0084DEB0;
    inline constexpr uintptr_t kLuaToUserData   = 0x0084E1C0;
    inline constexpr uintptr_t kLuaPushCClosure = 0x0084E400;
    inline constexpr uintptr_t kLuaGetField     = 0x0084E590;
    inline constexpr uintptr_t kLuaRawGet       = 0x0084E600;
    inline constexpr uintptr_t kLuaRawGetI      = 0x0084E670;
    inline constexpr uintptr_t kLuaCreateTable  = 0x0084E6E0;
    inline constexpr uintptr_t kLuaSetField     = 0x0084E900;
    inline constexpr uintptr_t kLuaRawSetI      = 0x0084EA00;
    inline constexpr uintptr_t kLuaSetMetatable = 0x0084EA90;
    inline constexpr uintptr_t kLuaNewUserData  = 0x0084F0F0;
    inline constexpr uintptr_t kLuaLCheckLString = 0x0084F9F0;
    inline constexpr uintptr_t kLuaLCheckNumber  = 0x0084FAB0;

    // --- FrameScript: events ---------------------------------------------------------------------

    /// Builds the client's event-name table from an array of names. Detoured pre-call so the array
    /// can be extended: that table is what FrameScript resolves an event name against, and a name
    /// absent from it can neither be registered for nor fired.
    inline constexpr uintptr_t kFrameScriptFillEvents = 0x0081B5F0;
    using FrameScriptFillEventsFn = void(__cdecl*)(const char** names, size_t count);

    /// -> the built event table (reserve, size, entries).
    inline constexpr uintptr_t kFrameScriptEventList = 0x00D3F7D0;

    /// Fires an event whose arguments the caller already pushed onto the script stack.
    inline constexpr uintptr_t kFrameScriptFireEventInner = 0x0081AA00;

    /// Fires an event, formatting its arguments from a printf-style list.
    inline constexpr uintptr_t kFrameScriptFireEventV = 0x0081AC90;

    /// Runs the frame's OnUpdate scripts. Detoured pre-call: it is the per-frame point at which the
    /// script state is current and firing an event is legal, which the raw frame pump is not.
    inline constexpr uintptr_t kFrameScriptFireOnUpdate = 0x00495810;
    using FrameScriptFireOnUpdateFn = int(__cdecl*)(int a1, int a2, int a3, int a4);

    /// Adds one global script function. Detoured only to observe: the client's own registration
    /// sweep is the window in which this module's globals have to be installed (see hook/Registry.cpp).
    inline constexpr uintptr_t kFrameScriptRegisterFunction = 0x00817F90;

    // --- FrameScript: unit tokens ----------------------------------------------------------------

    // Resolving "player" / "target" / "party1" to a GUID happens in one parser, and every consumer
    // goes through it -- script functions, macro conditionals and the secure frames' unit attributes
    // alike. Adding a token means extending that parser, which is why the seam below is an anchor
    // inside it rather than a function entry.

    /// Anchor inside the unit-token parser, reached with the token cursor at [ebp+8] and the GUID
    /// output at [ebp+0xC]. See hook/RawPatch.hpp for why this one is patched directly.
    inline constexpr uintptr_t kGetGuidByKeywordAnchor = 0x0060AFAA;

    /// Where the parser continues when a token was resolved.
    inline constexpr uintptr_t kGetGuidByKeywordHandled = 0x0060AD57;

    /// Where the parser continues when the token is none of ours and its own matching must run.
    inline constexpr uintptr_t kGetGuidByKeywordUnhandled = 0x0060AD44;

    /// The reverse direction: the tokens that currently name a GUID, as the client reports them.
    /// Detoured post-call to append ours, into the client's own fixed-size buffer.
    inline constexpr uintptr_t kGetKeywordsByGuid = 0x0060BB70;
    using GetKeywordsByGuidFn = char**(__cdecl*)(uint64_t* guid, size_t* count);

    /// Capacity of that buffer, and of each of its entries.
    inline constexpr size_t kKeywordBufferSlots = 8;
    inline constexpr size_t kKeywordSlotBytes   = 32;

    /// Resolves a unit token to a GUID through the parser above.
    inline constexpr uintptr_t kGetGuidByUnitId = 0x0060C1C0;

    // --- frames ----------------------------------------------------------------------------------

    /// The script-registry reference a frame is held by: the key that turns a frame pointer back
    /// into the script object addons already hold. this-in-ECX.
    inline constexpr uintptr_t kFrameGetRefTable = 0x00488380;

    /// Sets a frame's draw level within its strata. this-in-ECX.
    inline constexpr uintptr_t kFrameSetLevel = 0x004910A0;

    // --- object manager ----------------------------------------------------------------------------

    /// Walks every resident object, one callback per object. Main thread only: the list head comes
    /// from thread-local storage.
    inline constexpr uintptr_t kEnumObjects = 0x004D4B30;

    /// Resolves a GUID to an object, filtered by type mask.
    inline constexpr uintptr_t kGetObjectByGuid = 0x004D4DB0;

    /// The active player object, or null outside the world.
    inline constexpr uintptr_t kGetPlayer = 0x004038F0;

    /// Parses the "0x0000000000000000" text form of a GUID that the script API also accepts
    /// wherever it takes a unit.
    inline constexpr uintptr_t kHexStringToGuid = 0x0074D120;

    /// -> the player's current target GUID.
    inline constexpr uintptr_t kTargetGuid = 0x00BD07B0;

    // --- nameplates --------------------------------------------------------------------------------

    /// The squared distance at which nameplates stop being shown. Squared because the visibility
    /// test compares squared distances; the console variable that drives it is in yards.
    inline constexpr uintptr_t kNamePlateDistanceSquared = 0x00ADAA7C;

    /// Anchor in the engine's own nameplate frame-level pass, which rewrites every plate's draw
    /// level each frame. Skipping it is what lets a depth-sorted order survive to the next frame.
    inline constexpr uintptr_t kNamePlateLevelUpdateAnchor = 0x0098E9F9;

    /// Where that pass continues once the level write is skipped.
    inline constexpr uintptr_t kNamePlateLevelUpdateResume = 0x0098EA27;

    // --- camera ------------------------------------------------------------------------------------

    /// Initialises a camera's projection, taking the field of view as its last argument. Detoured
    /// pre-call to substitute the configured value, because the client recreates the camera on every
    /// world entry and would otherwise put its own default back.
    inline constexpr uintptr_t kCameraInitialize = 0x00607C20;
    using CameraInitializeFn = void(__fastcall*)(void* self, void* edx, float a2, float a3, float fov);

    /// camera -> field of view, a full angle in radians. wxl-core curates this same offset for its
    /// own reader (wxl::game::camera::GetFov); it is repeated here because the SDK exposes no
    /// writer, and reaching into the offsets header that holds it is what the SDK boundary forbids.
    inline constexpr size_t kCameraFovField = 0x40;

    // --- clipboard ---------------------------------------------------------------------------------

    // The client reads and writes the clipboard as ANSI, so anything outside the active code page
    // makes the round trip as '?'. Both entries are replaced with UTF-8 equivalents.

    /// Reads clipboard text. Returns a client-allocated buffer the caller takes ownership of.
    inline constexpr uintptr_t kClipboardGetString = 0x008726F0;
    using ClipboardGetStringFn = const char*(__cdecl*)(void* hwnd);

    /// Writes clipboard text.
    inline constexpr uintptr_t kClipboardSetString = 0x008727E0;
    using ClipboardSetStringFn = int(__cdecl*)(const char* text, void* hwnd);
}
