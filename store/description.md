# Awesome WotLK

World of Warcraft 3.3.5a (12340) improvements, as a WarcraftXL module.
Ported from awesome_wotlk, without the auto-login.

---

## Details

- BugFix: clipboard issue when non-english text becomes "???"
- Changing camera's FOV
- Improved nameplates sorting
- Nameplate unit tokens `nameplate1`..`nameplateN`, usable in macros and secure frames

### New API

- `C_NamePlate.GetNamePlates`
- `C_NamePlate.GetNamePlateForUnit`
- `UnitIsControlled`
- `UnitIsDisarmed`
- `UnitIsSilenced`
- `GetInventoryItemTransmog`
- `FlashWindow`
- `IsWindowFocused`
- `FocusWindow`
- `CopyToClipboard`

### New events

- `NAME_PLATE_CREATED`
- `NAME_PLATE_UNIT_ADDED`
- `NAME_PLATE_UNIT_REMOVED`

### New CVars

- `nameplateDistance`
- `cameraFov`

## Not included

Auto login, and the TOS/EULA auto-accept.

## Notes

Sets `AwesomeWotlk = 1`, so addons written against the original detect it.
Client build **12340** only.

Docs and source:
[github.com/Stefan2102/wxl-awesome-wotlk](https://github.com/Stefan2102/wxl-awesome-wotlk)
