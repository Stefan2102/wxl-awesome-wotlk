# Client addresses

Every client address this module carries, what it is, and why it is carried here rather than taken
from WarcraftXL.

All of them are absolute virtual addresses in the client's fixed `0x00400000` image base, valid for
**build 12340 and nothing else**. `WXL_CLIENT_BUILD` in the extension ABI is what stops the module
loading against an image where they would mean something else.

The authoritative copy is [`src/client/ClientOffsets.hpp`](../src/client/ClientOffsets.hpp), where
each constant carries its own note. This page is the overview and the rationale.

## Why they live here

WarcraftXL curates client landmarks in `src/offsets` and deliberately hides them from extensions: a
module that reaches for a raw address is a module that breaks on a client rebase, and the SDK exists
to absorb that. Its configure step even reports an extension that includes an `offsets/` header
directly, which this module never does.

But the core's curated set is aimed at the renderer and the asset pipeline, and this module lives in
the script and interface half of the client — the console variable registry, the FrameScript event
table, the unit-token parser, frame reference tables, the replicated object descriptors. None of
that is curated upstream, and the agreed constraint for this port was to change nothing in the core.
So it is curated here instead, in one file, held to the same standard: every constant named, every
constant explained, and the struct layouts checked with static assertions so a mistyped offset is a
build failure rather than a corrupted read.

Where the core *does* cover something, the core is used: `wxl::game::script` for reading a script
call's arguments and pushing its results, `wxl::game::camera::GetActiveCamera`,
`wxl::game::Native<>` for the typed calls, `wxl::events` and the API table for everything else.

One constant is a knowing duplicate: `kCameraFovField` (`0x40`) is the same field the core reads
through `wxl::game::camera::GetFov`. The SDK exposes no writer, and the header that holds the offset
is one an extension must not include.

## Process state

| Constant | Address | What it is |
|---|---|---|
| `kGameWindowPtr` | `0x00D41620` | -> the game's top-level `HWND` |
| `kInWorldFlag` | `0x00BD0792` | byte: non-zero once past the loading screen |

## Client runtime helpers

| Constant | Address | What it is |
|---|---|---|
| `kAtoi` | `0x0076F190` | the client's string-to-int parser; advances the caller's cursor, which is what splits `"<token><n>"` |
| `kStringHash` | `0x0076F640` | the hash the console registry and event table key on |
| `kAlloc` | `0x00415074` | the client's allocator; a buffer handed *back* to the client must come from it |

## Console variables

| Constant | Address | What it is |
|---|---|---|
| `kCVarsInitialize` | `0x0051D9B0` | registers the stock variables, once, during startup |
| `kRegisterCVar` | `0x00767FC0` | creates one |

`console::Variable` mirrors the client's `0x70`-byte record. Only the fields this module reads are
named; the rest is explicit padding, and the size is asserted — a mis-sized layout would make the
registry's own array indexing land on the wrong variable.

## Script state and the Lua C API

| Constant | Address | |
|---|---|---|
| `kGetScriptState` | `0x00817DB0` | the active FrameScript state; changes on `/reload` |
| `kLuaSetTop` | `0x0084DBF0` | `lua_settop` |
| `kLuaType` | `0x0084DEB0` | `lua_type` |
| `kLuaToUserData` | `0x0084E1C0` | `lua_touserdata` |
| `kLuaPushCClosure` | `0x0084E400` | `lua_pushcclosure` |
| `kLuaGetField` | `0x0084E590` | `lua_getfield` |
| `kLuaRawGet` | `0x0084E600` | `lua_rawget` |
| `kLuaRawGetI` | `0x0084E670` | `lua_rawgeti` |
| `kLuaCreateTable` | `0x0084E6E0` | `lua_createtable` |
| `kLuaSetField` | `0x0084E900` | `lua_setfield` |
| `kLuaRawSetI` | `0x0084EA00` | `lua_rawseti` |
| `kLuaSetMetatable` | `0x0084EA90` | `lua_setmetatable` |
| `kLuaNewUserData` | `0x0084F0F0` | `lua_newuserdata` |
| `kLuaLCheckLString` | `0x0084F9F0` | `luaL_checklstring` |
| `kLuaLCheckNumber` | `0x0084FAB0` | `luaL_checknumber` |

Only the table, registry and userdata half. The argument readers and value pushers come from
`wxl::game::script` and are re-exported rather than re-bound, so each entry has exactly one
definition in the process.

## FrameScript: events

| Constant | Address | What it is |
|---|---|---|
| `kFrameScriptFillEvents` | `0x0081B5F0` | builds the event-name table from an array |
| `kFrameScriptEventList` | `0x00D3F7D0` | -> the built table (reserve, size, entries) |
| `kFrameScriptFireEventInner` | `0x0081AA00` | fires an event whose arguments are already on the stack |
| `kFrameScriptFireEventV` | `0x0081AC90` | fires an event, formatting its arguments |
| `kFrameScriptFireOnUpdate` | `0x00495810` | runs the frame's OnUpdate scripts |
| `kFrameScriptRegisterFunction` | `0x00817F90` | adds one global script function |

An event's id is its position in the table, so it is resolved by name each time rather than
remembered across a rebuild.

## FrameScript: unit tokens

| Constant | Address | What it is |
|---|---|---|
| `kGetGuidByKeywordAnchor` | `0x0060AFAA` | anchor inside the token parser (token cursor at `[ebp+8]`, GUID out at `[ebp+0xC]`) |
| `kGetGuidByKeywordHandled` | `0x0060AD57` | where it resumes when a token resolved |
| `kGetGuidByKeywordUnhandled` | `0x0060AD44` | where it resumes to try the client's own matching |
| `kGetKeywordsByGuid` | `0x0060BB70` | the reverse: tokens naming a GUID; buffer of 8 slots, 32 bytes each |
| `kGetGuidByUnitId` | `0x0060C1C0` | resolves a token to a GUID through that parser |

The anchor is patched directly rather than detoured — see
[architecture.md](architecture.md#the-two-that-are-not-function-entries).

## Frames

| Constant | Address | What it is |
|---|---|---|
| `kFrameGetRefTable` | `0x00488380` | the script-registry reference a frame is held by (`__thiscall`) |
| `kFrameSetLevel` | `0x004910A0` | sets a frame's draw level within its strata (`__thiscall`) |

## Object manager

| Constant | Address | What it is |
|---|---|---|
| `kEnumObjects` | `0x004D4B30` | walks every resident object; main thread only |
| `kGetObjectByGuid` | `0x004D4DB0` | resolves a GUID, filtered by type mask |
| `kGetPlayer` | `0x004038F0` | the active player object |
| `kHexStringToGuid` | `0x0074D120` | parses the `"0x..."` form of a GUID |
| `kTargetGuid` | `0x00BD07B0` | -> the player's current target GUID |

## Object layout and descriptors

Field positions, not addresses. Each is expressed as the protocol field index it corresponds to,
with the byte offset asserted — see
[`src/client/Descriptors.hpp`](../src/client/Descriptors.hpp).

| Field | Offset | What it is |
|---|---|---|
| object -> descriptor | `+0x08` | the replicated update-field block |
| unit -> nameplate | `+0xC38` | the plate frame the client built, or null |
| virtual slot 11 | — | `GetPosition(self, float[3])`; the slot WarcraftXL documents as `kVtPosition` |
| `UNIT_FIELD_FLAGS` | index `0x3B` = `0xEC` | the flags behind `UnitIsControlled` / `Disarmed` / `Silenced` |
| `PLAYER_VISIBLE_ITEM_1_ENTRYID` | index `0x11B` = `0x46C` | 19 two-field records: what each slot *looks* like |

The last one is where the original library drifted; see
[architecture.md](architecture.md#defects-fixed-while-porting), item 5.

## Nameplates

| Constant | Address | What it is |
|---|---|---|
| `kNamePlateDistanceSquared` | `0x00ADAA7C` | float: the squared visibility distance |
| `kNamePlateLevelUpdateAnchor` | `0x0098E9F9` | anchor in the engine's own frame-level pass |
| `kNamePlateLevelUpdateResume` | `0x0098EA27` | where that pass continues once the write is skipped |

## Camera

| Constant | Address | What it is |
|---|---|---|
| `kCameraInitialize` | `0x00607C20` | initialises a camera's projection (`__fastcall`), field of view last |
| `kCameraFovField` | `+0x40` | camera -> field of view, a full angle in radians |

## Clipboard

| Constant | Address | What it is |
|---|---|---|
| `kClipboardGetString` | `0x008726F0` | reads clipboard text; returns a client-allocated buffer |
| `kClipboardSetString` | `0x008727E0` | writes clipboard text |

Both convert through the active ANSI code page, which is the bug this module replaces them to fix.

## Provenance

These come from [awesome_wotlk](https://github.com/FrostAtom/awesome_wotlk), where the reverse
engineering behind them was done; they were transcribed with the auto-login entries left out. The
player descriptor layout was rebuilt from the 3.3.5a update-field indices rather than copied, because
the copy had drifted — see the architecture notes.
