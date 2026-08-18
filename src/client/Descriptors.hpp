// Object layouts: the runtime object header, and the server-replicated descriptor block behind it.
//
// Every world object carries a pointer to a flat array of 4-byte "update fields" that the server
// replicates. Their positions are a property of the 3.3.5a protocol, so each one below is expressed
// as its byte offset with the protocol field index it corresponds to written alongside, and checked
// with a static assertion. Only the fields this module reads are declared: a descriptor is large,
// and transcribing the parts nothing uses is how a layout silently drifts out of true.

#pragma once

#include <cstddef>
#include <cstdint>

namespace awow::client::object
{
    /// A world object as this module sees it: a header pointer and, for units, a nameplate slot.
    using Object = void;

    // Type mask bits a GUID lookup filters on: an object matches when any requested bit is set on
    // it. Only the two categories this module asks for are named. A player carries both, so asking
    // for a unit reaches players too, while asking for a player excludes creatures -- which matters
    // wherever a player-only descriptor field is about to be read.
    inline constexpr uint32_t kTypeMaskUnit   = 0x08;
    inline constexpr uint32_t kTypeMaskPlayer = 0x10;

    // --- runtime object layout ---------------------------------------------------------------------

    /// object -> its descriptor block.
    inline constexpr size_t kDescriptorField = 0x08;

    /// unit object -> the nameplate frame the client built for it, or null while it has none.
    inline constexpr size_t kNamePlateField = 0xC38;

    /// Virtual slot carrying GetPosition(self, float[3]). Shared by every object type; the same slot
    /// wxl-core documents as kVtPosition.
    inline constexpr size_t kPositionVirtualSlot = 11;
    using PositionFn = void(__thiscall*)(Object* self, float out[3]);

    // --- descriptor field offsets ------------------------------------------------------------------

    /// UNIT_FIELD_FLAGS (protocol field index 0x3B).
    inline constexpr size_t kUnitFlagsField = 0x3B * 4;
    static_assert(kUnitFlagsField == 0xEC, "UNIT_FIELD_FLAGS offset");

    /// PLAYER_VISIBLE_ITEM_1_ENTRYID (protocol field index 0x11B): the first of 19 two-field records
    /// describing what each equipment slot *looks* like, which is what a transmogrified item changes.
    inline constexpr size_t kPlayerVisibleItemsField = 0x11B * 4;
    static_assert(kPlayerVisibleItemsField == 0x46C, "PLAYER_VISIBLE_ITEM_1_ENTRYID offset");

    /// How many equipment slots those records cover.
    inline constexpr int kEquipmentSlotCount = 19;

    /// One such record.
    struct VisibleItem
    {
        int32_t entryId;   ///< item whose appearance is shown
        int32_t enchantId; ///< visual enchantment on it
    };
    static_assert(sizeof(VisibleItem) == 0x8, "VisibleItem layout does not match the client");

    // --- unit flags --------------------------------------------------------------------------------

    /// UNIT_FIELD_FLAGS bits. Only the states this module reports on are named.
    enum UnitFlags : uint32_t
    {
        kUnitFlagSilenced = 0x00002000,
        kUnitFlagPacified = 0x00020000,
        kUnitFlagStunned  = 0x00040000,
        kUnitFlagDisarmed = 0x00200000,
        kUnitFlagConfused = 0x00400000,
        kUnitFlagFleeing  = 0x00800000,
    };

    /// The states that take control of a unit away from whoever is driving it.
    inline constexpr uint32_t kUnitFlagsControlled =
        kUnitFlagFleeing | kUnitFlagConfused | kUnitFlagStunned | kUnitFlagPacified;

    // --- accessors ---------------------------------------------------------------------------------

    /**
     * @brief Reads an object's descriptor block.
     * @param object  World object.
     * @return The descriptor base, or null.
     */
    inline const uint8_t* Descriptor(const Object* object)
    {
        if (!object) return nullptr;
        return *reinterpret_cast<const uint8_t* const*>(static_cast<const uint8_t*>(object) +
                                                        kDescriptorField);
    }

    /**
     * @brief Reads a unit's UNIT_FIELD_FLAGS.
     * @param unit  Unit object.
     * @return The flags, or 0 when the unit or its descriptor is absent.
     */
    inline uint32_t UnitFlagsOf(const Object* unit)
    {
        const uint8_t* descriptor = Descriptor(unit);
        if (!descriptor) return 0;
        return *reinterpret_cast<const uint32_t*>(descriptor + kUnitFlagsField);
    }

    /**
     * @brief Reads one of a player's visible-item records.
     * @param player  Player object.
     * @param slot    Zero-based equipment slot, below kEquipmentSlotCount.
     * @return The record, or null when the player, its descriptor, or the slot is out of range.
     */
    inline const VisibleItem* VisibleItemOf(const Object* player, int slot)
    {
        if (slot < 0 || slot >= kEquipmentSlotCount) return nullptr;
        const uint8_t* descriptor = Descriptor(player);
        if (!descriptor) return nullptr;
        return reinterpret_cast<const VisibleItem*>(descriptor + kPlayerVisibleItemsField) + slot;
    }

    /**
     * @brief Reads the nameplate frame the client built for a unit.
     * @param unit  Unit object.
     * @return The frame, or null while the unit has no plate.
     */
    inline void* NamePlateOf(const Object* unit)
    {
        if (!unit) return nullptr;
        return *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(unit) + kNamePlateField);
    }

    /**
     * @brief Reads an object's world position through its virtual position accessor.
     * @param object  World object.
     * @param out     Receives x, y, z.
     */
    inline void PositionOf(Object* object, float out[3])
    {
        out[0] = out[1] = out[2] = 0.0f;
        if (!object) return;
        void* const* vtable = *reinterpret_cast<void* const* const*>(object);
        reinterpret_cast<PositionFn>(vtable[kPositionVirtualSlot])(object, out);
    }
}
