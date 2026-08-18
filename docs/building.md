# Building and installing

## Requirements

- **CMake** 3.20 or later, on `PATH`
- **A Win32 MSVC toolchain** — Visual Studio 2022 with the C++ desktop workload is what this is
  developed against. The target client is a 32-bit process, so the build configures `-A Win32` and
  refuses anything else.
- **A [WarcraftXL](https://github.com/WarcraftXL/wxl-core) checkout** — build-time only, see below
- **PowerShell** 5.1 or later, for `build.bat` / `build.ps1`

## Finding WarcraftXL

The core is a build-time dependency only: its SDK (`wxl::game`, `wxl::events`) is header-only and
compiles in, and everything else arrives through the API table at load. Nothing of the core is
linked, and no core binary needs to exist to build this.

The build looks for it in this order:

1. `-DWXL_CORE_DIR=<path>` on the CMake command line, or `-CorePath <path>` to the build scripts
2. the `WXL_CORE_DIR` environment variable
3. `../wxl-core`, relative to this repository

The path has to contain `include/wxl/PluginApi.h`; configure fails with that message if it does not.

## Building

```
build.bat
```

or, with everything spelled out:

```
build.bat -CorePath ..\wxl-core -ClientPath D:\Path\To\Client -Config Release
```

`build.bat` finds PowerShell, hands its arguments to `build.ps1`, prints whether the build
succeeded, holds the window open, and exits with the build's own status. `build.ps1` accepts:

| Switch | Meaning |
|---|---|
| `-Config` | build configuration; `Release` by default |
| `-CorePath` | the WarcraftXL checkout to build against |
| `-ClientPath` | client directory to deploy into; omit to build only |
| `-Clean` | delete the build directory first, forcing a from-scratch build |

Both paths are remembered in the CMake cache after the first run, so later builds need no switches.

Straight CMake works too:

```
cmake -S . -B build -A Win32 -DWXL_CORE_DIR=../wxl-core
cmake --build build --config Release
```

Output: `build/<Config>/wxl-awesome-wotlk.dll`.

## Installing

WarcraftXL discovers extensions as `Extensions\<folder>\<folder>.dll` under the client directory, so
the folder name and the DLL name have to match:

```
<client>/Extensions/wxl-awesome-wotlk/wxl-awesome-wotlk.dll
```

Building with `-ClientPath` puts it there automatically after every build.

This module needs no patcher of its own. The client has to be set up for WarcraftXL — that is the
core's own install step — and this loads from there.

To confirm it came up, check WarcraftXL's log for the extension count and for a `wxl-awesome-wotlk:
loaded` line. A seam that could not be installed is logged individually, at error level, naming what
stops working.

## Releasing

A version lives in three places, and they have to move together. Nothing checks that they agree, so
this is the checklist:

| Where | Format | Example |
|---|---|---|
| `src/Module.hpp` → `kModuleVersion` | `major * 10000 + minor * 100 + patch` | `10000` |
| `wxl.json` → `extension.version` | semver | `"1.0.0"` |
| the git tag and its GitHub release | `vX.Y.Z` | `v1.0.0` |

Then attach the freshly built `wxl-awesome-wotlk.dll` to the release.

`kModuleVersion` is what the core prints in its load line, so it is what a player reads back off a
log when reporting something. `wxl.json` is what the hub compares against an installed copy to
decide whether an update is available, and it is read from the **default branch**, not from the tag
— so a manifest bumped only on a branch advertises nothing.

The hub picks the binary off the latest release using `deploy.match` (`*.dll` here). A bare DLL is
fine: the hub detects that the download is not an archive and installs it under the extension's own
id. Changing the asset to a zip means changing `match` to suit.

## Building inside the core's tree

The `src/` tree is also a valid WarcraftXL extension folder. Copying it to
`wxl-core/extensions/wxl-awesome-wotlk/` and running the core's own `build.ps1` builds the same DLL
through the core's per-extension CMake loop. Two differences are worth knowing:

- The core's loop also compiles `src/game/*.cpp`, the out-of-line half of its SDK. This module
  references none of it. The standalone build here leaves those out, along with the
  `d3d9`/`d3dcompiler` dependency they carry.
- The core's configure scans extension sources for a direct `#include "offsets/..."` and reports any
  it finds. This module never does that — the addresses it needs and the core does not curate are
  its own, in `src/client/ClientOffsets.hpp`.

## Notes on the build settings

- **Static CRT** (`MultiThreaded`), matching the core. The DLL loads inside `Wow.exe`, which has no
  reason to carry the toolset's redistributable; a missing one is a loader failure at process start
  with no log to explain it.
- **C++20**, matching the core.
- **`/W4 /permissive-`**, and the tree builds clean at that level.
- **`WXL_EXTENSION`** is defined on the target, not in a source file. It is what makes `PluginApi.h`
  declare `WXL_Query` and `WXL_Load` as exports; an extension that forgets it still compiles and
  fails only at load, so it belongs somewhere the whole target inherits it.
