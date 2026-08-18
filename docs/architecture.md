# Architecture

How the module is put together, which client seams it uses, and why each one is reached the way it
is.

## The shape of the problem

The client offers no way to add a console variable, an event name, a script global or a unit token.
Each of those is built once during startup from a table the client owns, and the only way in is to
be present while that table is being built. So the module is, structurally, a set of *declarations*
plus one detour per table.

WarcraftXL supplies the process bring-up, the hook engine, the event bus and an extension ABI. It
does not curate the script/interface half of the client, so the addresses for that half are carried
here — see [offsets.md](offsets.md).

## Layers

```
src/
├── Extension.cpp   WXL_Query / WXL_Load: the two entry points, and the bring-up order
├── Module.*        module identity, the core service table, logging
├── client/         the client, as this module sees it
│   ├── ClientOffsets.hpp  every curated address, one commented constant each
│   ├── Lua.hpp            the Lua 5.1 C API the core's SDK does not bind
│   ├── Console.hpp        console variables
│   ├── FrameScript.hpp    the event table, and firing an event
│   ├── Frame.hpp          pushing a frame to script, and its draw level
│   ├── ObjectMgr.hpp      GUIDs, unit tokens, and walking resident objects
│   └── Descriptors.hpp    object layout and the replicated descriptor fields
├── hook/           how this module gets into the client
│   ├── Attach.hpp         one detour through the core's chained engine
│   ├── RawPatch.*         direct jump patches, for the two non-function seams
│   ├── ScriptGate.*       admitting this module's script callbacks
│   └── Registry.*         the extension points every feature declares against
├── feature/        one file per user-visible feature
└── util/           UTF-8 clipboard access
```

The dependency direction is one-way: `feature` uses `hook` and `client`, `hook` uses `client`,
`client` uses nothing of ours but `ClientOffsets.hpp`.

## Bring-up

`WXL_Load` runs on the main thread at the client's engine initialisation, which WarcraftXL detours
for exactly this purpose. Everything the module patches happens later in startup, so there is no
race to lose. The order is:

1. **The script gate.** Every script function this module registers is refused at its first call
   without it, so it goes first.
2. **The features.** Each only appends to a list. Nothing here touches the client, so the order
   carries no meaning beyond matching the order awesome_wotlk brought the same features up in.
3. **The registry.** Installs one detour per seam, for whichever seams the declarations need. A seam
   nothing declared against is left alone.

Declaring first and wiring afterwards is what keeps a single detour per client function no matter
how many features want a piece of it.

## The seams

Every seam that is a real function entry goes through `WXL_Api::HookAttach`, so other modules may
detour the same address and the core arbitrates the chain.

| Client function | Address | What this module does with it |
|---|---|---|
| `CVars_Initialize` | `0x0051D9B0` | post-call: create `nameplateDistance` and `cameraFov`, once the stock set exists |
| `FrameScript_FillEvents` | `0x0081B5F0` | pre-call: append the three `NAME_PLATE_*` names to the event table |
| `FrameScript_RegisterFunction` | `0x00817F90` | post-call: install this module's script globals on the state |
| `FrameScript_FireOnUpdate` | `0x00495810` | pre-call: run the nameplate scan |
| `GetKeywordsByGuid` | `0x0060BB70` | post-call: report `nameplate<N>` for a GUID that has a plate |
| `Camera_Initialize` | `0x00607C20` | pre-call: substitute the configured field of view |
| `Clipboard_GetString` | `0x008726F0` | replace: read the clipboard as UTF-8 |
| `Clipboard_SetString` | `0x008727E0` | replace: write the clipboard as UTF-8 |
| `Lua.ValidateFunctionPointer` (by name) | — | admit this module's own script callbacks |

### The two that are not function entries

`hook/RawPatch` writes a five-byte relative jump directly, and owns the bytes it displaced.

| Anchor | Resumes at | Why |
|---|---|---|
| `0x0060AFAA` | `0x0060AD57` handled, `0x0060AD44` not | inside the unit-token parser |
| `0x0098E9F9` | `0x0098EA27` | inside the engine's nameplate frame-level pass |

Both are anchors in the middle of a function, reached with a particular register and stack state,
and the code that lands on them never continues to the original: it does its work and jumps to a
chosen instruction further along, re-executing the displaced bytes inline. A chaining engine builds
a trampoline from those displaced bytes and hands it out as `original` — there is nothing to hand
out here, and registering one would leave a trampoline nobody can legally call.

This has a consequence worth stating plainly: **a raw patch is module-private.** A second party
patching the same address wins or loses by install order, with no chain to arbitrate. That is why it
is confined to these two and nothing else. Both log explicitly when they fail to apply.

The token anchor is where it is on purpose. Every consumer of a unit token — script functions, macro
conditionals, the secure frames' unit attributes — goes through that one parser, so `nameplate1`
resolves everywhere rather than only in Lua. Hooking the outer `GetGuidByUnitID` (`0x0060C1C0`),
which *is* a clean function entry, would cover the script path alone.

## Two deliberate divergences from awesome_wotlk

Both preserve the behaviour and change how it is reached.

### Script callbacks are admitted by range, not by opening the gate

Before it invokes a script callback, the engine checks the pointer against the bounds of its own
image. A function compiled into an extension lies outside them by construction. awesome_wotlk widens
those bounds globally (`*(DWORD*)0x00D415B8 = 1; *(DWORD*)0x00D415BC = 0x7FFFFFFF`), which admits
any out-of-image pointer for the rest of the process.

This module detours the check instead — WarcraftXL already registers it as the hook point
`Lua.ValidateFunctionPointer` — and returns without calling the original only for pointers inside
its own image, whose bounds come from the linker's `__ImageBase`. Everything else still reaches the
engine's test, which is doing its job for the calls it was written for.

### Script globals are installed from the registration sweep

awesome_wotlk hooks `0x00530F85`, an anchor on the tail of `Lua_OpenFrameXMLApi`, where the five
displaced bytes are a one-byte `ret` plus four bytes belonging to the next function.

This module detours `FrameScript_RegisterFunction` — a real function entry — and installs its
globals on the first stock registration it sees against a state that does not carry them yet. That
is the same `Lua_OpenFrameXMLApi` pass, slightly earlier within it, and it re-arms after `/reload`
because the client builds a fresh state and the marker goes with the old one.

"Does not carry them yet" is tested by a raw lookup of the `AwesomeWotlk` global rather than by
comparing state pointers: a freed state's allocation can come back for the next one, and a pointer
comparison would then decide, wrongly, that the new state had already been served.

The same idempotent check also runs once per frame from the OnUpdate seam. It costs one raw table
lookup, and it means a state the registration seam somehow did not see still gets served — a frame
late rather than never.

## Nameplates

The per-frame scan walks every resident object, and for each one that has a nameplate frame records
the pairing of frame to unit GUID.

A plate's **index in that list is its unit token**, so entries are never removed — an index that
shifted would silently rename every plate after it. An entry a scan does not see loses its visible
flag and keeps its place. The list is bounded in practice because the client draws plates from a
fixed pool and reuses the frames.

The list lives in the **script registry**, keyed under `wxl-awesome-wotlk.nameplates`, not in a
static. A reload discards the script state, and the list goes with it; carrying it across would
leave tokens pointing at plates whose creation nothing has heard about.

The client hands a pooled plate to a different unit without announcing it, so the GUID is re-read
every scan rather than trusted from when the plate was first seen.

Ordering: plates sort far to near, with the current target forced last so it always draws in front,
and are then assigned consecutive frame levels from 10 upward. Sorting is by *squared* distance —
same order, no square roots. The engine's own level pass would undo all of this each frame, which is
what the `0x0098E9F9` anchor suppresses.

## Defects fixed while porting

These are real defects in the code being ported. Carrying them over verbatim would have meant
shipping known bugs, so each was fixed:

1. **Clipboard write overran its allocation.** `CopyToClipboardU8` wrote a terminator at
   `cbBuf[wCharsLen]`, one element past a `sizeof(wchar_t) * wCharsLen` block. `MultiByteToWideChar`
   already terminates, because the source length counts the terminator; the extra write is gone.
2. **Clipboard write leaked a lock.** The same function's failure path after `OpenClipboard` failed
   freed the block without unlocking it. Both the clipboard and the memory lock are now scope-owned,
   so no exit path can skip either. A `SetClipboardData` failure is also handled rather than assumed
   away.
3. **`FireEvent` never called `va_end`.**
4. **`GetInventoryItemTransmog` read player fields off any unit.** It resolved its argument with the
   *unit* type mask and then read `PLAYER_VISIBLE_ITEM_*`, which only exists on a player's
   descriptor — on a creature that reads past the end of it. It now resolves with the player mask,
   so a non-player argument returns nil.
5. **The player descriptor layout was off by 0x250.** `PlayerEntry` inherited `UnitEntry` *and*
   carried a second `UnitEntry` member, added by an unrelated commit after the transmog API was
   written, which put `visibleItems` at `0x6BC` instead of `0x46C`. The layout here is expressed as
   protocol field indices with static assertions on the resulting byte offsets, which makes that
   class of drift a build failure.
6. **`Camera_Initialize` dereferenced its console variable unconditionally.** A registration that
   was refused would have crashed the first camera build; the pointer is now checked.
