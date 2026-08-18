# wxl-awesome-wotlk

A **WarcraftXL** extension for the World of Warcraft 3.3.5a (build 12340) client. It brings the
feature set of [awesome_wotlk](https://github.com/FrostAtom/awesome_wotlk) into WarcraftXL's module
system, so it composes with the rest of the framework instead of being a second injector.

**The auto-login functionality is deliberately not part of this module.** See
[What is not here](#what-is-not-here).

## What it adds

| | |
|---|---|
| **Nameplates** | `C_NamePlate.GetNamePlates`, `C_NamePlate.GetNamePlateForUnit`, the `NAME_PLATE_CREATED` / `NAME_PLATE_UNIT_ADDED` / `NAME_PLATE_UNIT_REMOVED` events, `nameplate1`..`nameplateN` unit tokens, depth-sorted plate ordering, and the `nameplateDistance` console variable |
| **Camera** | the `cameraFov` console variable |
| **Units** | `UnitIsControlled`, `UnitIsDisarmed`, `UnitIsSilenced` |
| **Inventory** | `GetInventoryItemTransmog` |
| **Window** | `FlashWindow`, `IsWindowFocused`, `FocusWindow`, `CopyToClipboard` |
| **Fix** | clipboard text is read and written as UTF-8, so non-ASCII no longer round-trips as `?` |

Every function and event is described in [docs/api_reference.md](docs/api_reference.md).

The global `AwesomeWotlk` is set to `1` while the module is loaded, which is what addons written
against the original library test for.

## What is not here

The original library also logs you in from the command line
(`-login` / `-password` / `-realmlist` / `-realmname` / `-character`) and auto-accepts the terms of
service and EULA prompts. None of that is ported: no command line is read, no credentials are
handled, and the legal prompts behave as the client intends.

## Building

```
build.bat -CorePath ..\wxl-core -ClientPath D:\Path\To\Client
```

Requires CMake 3.20+, a Win32 MSVC toolchain, and a
[WarcraftXL](https://github.com/WarcraftXL/wxl-core) checkout. The core is a build-time dependency
only — its SDK is header-only, and everything else arrives through the API table at load. Full
details in [docs/building.md](docs/building.md).

## Installing

Build with `-ClientPath` to deploy automatically, or copy the DLL yourself:

```
<client>/Extensions/wxl-awesome-wotlk/wxl-awesome-wotlk.dll
```

WarcraftXL discovers it from there at startup. It needs no separate patcher of its own — the client
must be set up for WarcraftXL, and that is all.

## Documentation

- [docs/api_reference.md](docs/api_reference.md) — the script API, events and console variables
- [docs/architecture.md](docs/architecture.md) — how the module is put together, and every client
  seam it uses
- [docs/building.md](docs/building.md) — build, deploy, and building inside the core's tree
- [docs/offsets.md](docs/offsets.md) — every client address the module carries, and why

`wxl.json` is the [wxl-hub](https://github.com/WarcraftXL/wxl-hub) catalogue manifest: it is what
lists this module in the hub and tells it where to fetch the binary from.

## Credits

The features, and the reverse engineering behind them, are the work of the
[awesome_wotlk](https://github.com/FrostAtom/awesome_wotlk) authors. This module is a port of that
work onto [WarcraftXL](https://github.com/WarcraftXL/wxl-core).

## Legal

Modding a game client is on you: work on a copy, keep an untouched backup, and only point this at a
client and a server you are permitted to modify and connect to.

World of Warcraft and Wrath of the Lich King are trademarks of Blizzard Entertainment. This project
is not affiliated with or endorsed by Blizzard.

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE). The same license the original library and
WarcraftXL are released under.
