# Script API reference

Everything this module adds to the client's script environment. All of it is available once the
module is loaded; the global `AwesomeWotlk` is set to `1` and is what an addon should test for.

- [C_NamePlate](#c_nameplate)
- [Unit](#unit)
- [Inventory](#inventory)
- [Window](#window)
- [Console variables](#console-variables)

---

## C_NamePlate

Backported from the retail client's C API.

### `C_NamePlate.GetNamePlates()`

**Returns** `table` — an array of the nameplate frames currently shown.

```lua
for _, plate in ipairs(C_NamePlate.GetNamePlates()) do
    -- plate is the nameplate's base frame
end
```

A fresh table each call; the frames in it are the client's own, the same objects the events below
hand over.

### `C_NamePlate.GetNamePlateForUnit(unit)`

**Arguments** `unit` `string` — a unit token.

**Returns** `frame` — that unit's nameplate, or `nil` when it has none shown.

```lua
local plate = C_NamePlate.GetNamePlateForUnit("target")
```

### Event: `NAME_PLATE_CREATED`

**Payload** `namePlateBase` `frame`

Fires the first time a nameplate frame is seen, before it is shown. Once per frame object, not once
per unit — the client draws plates from a pool and reuses them.

### Event: `NAME_PLATE_UNIT_ADDED`

**Payload** `unitToken` `string`

Fires when a plate becomes visible. The token is `nameplate1`, `nameplate2`, and so on, and is valid
for as long as that plate stays shown.

### Event: `NAME_PLATE_UNIT_REMOVED`

**Payload** `unitToken` `string`

Fires when a plate stops being shown. The token stops resolving after this.

### Unit tokens: `nameplate1` … `nameplateN`

Every shown nameplate answers to a numbered token, and it works wherever a unit token works — script
functions, macro conditionals, and secure frame unit attributes:

```lua
print(UnitName("nameplate1"))
```

```
/target nameplate1
```

The number is a stable slot, not a ranking: it does not reorder as units move.

---

## Unit

Three states the stock API does not expose. Each returns `1` when true and `nil` otherwise, matching
the stock `UnitIs*` functions.

### `UnitIsControlled(unit)`

True while the unit is stunned, feared, confused or pacified — that is, while something other than
its owner is deciding what it does.

### `UnitIsDisarmed(unit)`

True while the unit is disarmed.

### `UnitIsSilenced(unit)`

True while the unit is silenced.

---

## Inventory

### `GetInventoryItemTransmog(unit, slot)`

**Arguments** `unit` `string` — a unit token, which must resolve to a **player**;
`slot` `number` — a one-based equipment slot, as `GetInventoryItemLink` takes.

**Returns** `itemId` `number`, `enchantId` `number` — what the slot *looks* like, which is what a
transmogrification changes, as opposed to what is equipped there. `nil` when the unit is not a
player, is not in range, or the slot is out of range.

```lua
local itemId, enchantId = GetInventoryItemTransmog("player", 16) -- main hand
```

---

## Window

### `FlashWindow()`

Draws attention to the game's taskbar button until the window is next activated.

### `IsWindowFocused()`

**Returns** `1` when the game window is in the foreground, `nil` otherwise.

### `FocusWindow()`

Raises the game window. Subject to the system's own rules about which process may take the
foreground, so it is a request rather than a guarantee.

### `CopyToClipboard(text)`

**Arguments** `text` `string`

Puts text on the clipboard as UTF-8, so non-ASCII survives.

---

## Console variables

### `cameraFov`

**Default** `100`. **Range** `1`–`200`, clamped.

The camera's field of view as a percentage, where `100` is the client's own 90-degree view. Higher
values widen it towards a fisheye.

```
/console cameraFov 130
```

Applied immediately, and re-applied to every camera the client builds afterwards, so it survives
zoning.

### `nameplateDistance`

**Default** `43`. In yards.

How far away nameplates stay visible. A value of zero or less falls back to `41`.

```
/console nameplateDistance 60
```

---

## Not in this module

The original awesome_wotlk library also read `-login`, `-password`, `-realmlist`, `-realmname` and
`-character` from the command line to log in automatically, and auto-accepted the terms of service
and EULA prompts. None of that is part of this module.
