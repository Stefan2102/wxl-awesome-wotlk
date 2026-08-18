#include "feature/NamePlates.hpp"

#include "Module.hpp"
#include "client/ClientOffsets.hpp"
#include "client/Console.hpp"
#include "client/Descriptors.hpp"
#include "client/Frame.hpp"
#include "client/FrameScript.hpp"
#include "client/Lua.hpp"
#include "client/ObjectMgr.hpp"
#include "hook/RawPatch.hpp"
#include "hook/Registry.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

namespace awow::feature::namePlates
{
    namespace console = awow::client::console;
    namespace fs      = awow::client::framescript;
    namespace frame   = awow::client::frame;
    namespace lua     = awow::client::lua;
    namespace obj     = awow::client::object;
    namespace off     = awow::client::offsets;

    namespace
    {
        using Guid = client::objectmgr::Guid;

        // --- what this feature publishes -----------------------------------------------------------

        constexpr const char* kEventCreated     = "NAME_PLATE_CREATED";
        constexpr const char* kEventUnitAdded   = "NAME_PLATE_UNIT_ADDED";
        constexpr const char* kEventUnitRemoved = "NAME_PLATE_UNIT_REMOVED";

        constexpr const char* kTableName = "C_NamePlate";

        /// Prefix of the unit tokens the plates answer to: nameplate1, nameplate2, ...
        constexpr const char* kToken = "nameplate";

        /// Enough for the prefix and any index the client could ever produce.
        constexpr size_t kTokenBytes = 32;

        constexpr const char* kDistanceVariable = "nameplateDistance";

        /// What the variable is registered with, and therefore the effective default in yards.
        constexpr const char* kDistanceDefault = "43";

        /// Used when the variable is set to something that is not a positive distance.
        constexpr double kDistanceFallbackYards = 41.0;

        /// Draw level the farthest plate is given; each nearer one gets the next level up. Well
        /// above zero so that a plate never lands under the frames the interface draws behind it.
        constexpr int kBaseFrameLevel = 10;

        /// How many plates to size the per-frame sort buffer for. The client draws from a pool of a
        /// few dozen, so this makes the steady state allocation-free without over-reserving.
        constexpr size_t kExpectedPlates = 64;

        /// Where the per-state plate list is kept. Namespaced, because the script registry is shared
        /// with the client and with every other module.
        constexpr const char* kRegistryKey = "wxl-awesome-wotlk.nameplates";

        // --- the plate list ------------------------------------------------------------------------

        enum PlateFlags : uint32_t
        {
            kFlagNone    = 0,
            kFlagCreated = 1u << 0, ///< NAME_PLATE_CREATED has been fired for this frame
            kFlagVisible = 1u << 1, ///< the plate is currently shown, and holds a unit token
        };

        /// Both flags: a plate is addressable as nameplate<N> only once it is created and shown.
        constexpr uint32_t kFlagsAddressable = kFlagCreated | kFlagVisible;

        struct Plate
        {
            frame::Frame* frame = nullptr;
            Guid          guid  = 0;
            uint32_t      flags = kFlagNone;
            uint32_t      sweep = 0; ///< the scan that last saw this plate
        };

        /**
         * @brief Every plate the client has shown, and the scan counter that ages them.
         *
         * A plate's index in this list is its unit token, so entries are never removed: an index
         * that shifted would silently rename every plate after it. Instead an entry that a scan did
         * not see loses its visible flag and keeps its place. The list is bounded in practice
         * because the client draws plates from a fixed pool and reuses the frames.
         *
         * It lives in the script registry rather than in a static here, so that a reload -- which
         * discards the script state -- discards the list with it. Carrying it across would leave
         * tokens pointing at plates whose events nothing has heard.
         */
        struct PlateList
        {
            std::vector<Plate> plates;
            uint32_t           sweep = 1;
        };

        int __cdecl DestroyPlateList(lua::State* state)
        {
            // The registry is releasing the userdata this list was placed into, so the vector inside
            // it has to be destroyed by hand: placement new has no matching automatic destruction.
            if (auto* list = static_cast<PlateList*>(lua::ToUserData(state, -1))) list->~PlateList();
            return 0;
        }

        /**
         * @brief Returns the plate list for a script state, creating it on first use.
         * @param state  Script state.
         * @return The list; the reference stays valid for as long as the state does.
         *
         * Stack-neutral in both paths, so it may be called with values already on the stack.
         */
        PlateList& ListFor(lua::State* state)
        {
            lua::GetField(state, lua::kRegistryIndex, kRegistryKey);
            PlateList* list = lua::IsUserData(state, -1)
                                  ? static_cast<PlateList*>(lua::ToUserData(state, -1))
                                  : nullptr;
            lua::Pop(state, 1);
            if (list) return *list;

            list = static_cast<PlateList*>(lua::NewUserData(state, sizeof(PlateList)));
            new (list) PlateList();

            lua::CreateTable(state, 0, 1);
            lua::PushFunction(state, &DestroyPlateList);
            lua::SetField(state, -2, "__gc");
            lua::SetMetatable(state, -2);
            lua::SetField(state, lua::kRegistryIndex, kRegistryKey);
            return *list;
        }

        /// @return true once the player is in the world, where plates exist at all.
        bool InWorld() { return *reinterpret_cast<const char*>(off::kInWorldFlag) != 0; }

        // --- unit tokens ---------------------------------------------------------------------------

        Guid GuidForIndex(int index)
        {
            lua::State* state = lua::GetState();
            if (!state || index < 0) return 0;

            const PlateList& list = ListFor(state);
            if (size_t(index) >= list.plates.size()) return 0;
            return list.plates[size_t(index)].guid;
        }

        int IndexForGuid(Guid guid)
        {
            if (!guid) return -1;
            lua::State* state = lua::GetState();
            if (!state) return -1;

            const PlateList& list = ListFor(state);
            for (size_t i = 0; i < list.plates.size(); i++)
            {
                const Plate& plate = list.plates[i];
                if ((plate.flags & kFlagsAddressable) == kFlagsAddressable && plate.guid == guid)
                    return int(i);
            }
            return -1;
        }

        /// @return The shown plate a GUID currently has, or null.
        const Plate* ShownPlateFor(const PlateList& list, Guid guid)
        {
            if (!guid) return nullptr;
            for (const Plate& plate : list.plates)
                if ((plate.flags & kFlagVisible) && plate.guid == guid) return &plate;
            return nullptr;
        }

        // --- the script table ------------------------------------------------------------------------

        /// C_NamePlate.GetNamePlates() -> table of the plates currently shown.
        int __cdecl GetNamePlates(lua::State* state)
        {
            lua::CreateTable(state, 0, 0);

            const PlateList& list = ListFor(state);
            int index = 1;
            for (const Plate& plate : list.plates)
            {
                if (!(plate.flags & kFlagVisible) || !plate.guid) continue;
                frame::Push(state, plate.frame);
                lua::RawSetI(state, -2, index++);
            }
            return 1;
        }

        /// C_NamePlate.GetNamePlateForUnit(unit) -> that unit's plate, or nil.
        int __cdecl GetNamePlateForUnit(lua::State* state)
        {
            const char* token = lua::CheckString(state, 1);

            const Guid guid = client::objectmgr::GuidByUnitToken(token);
            if (!guid) return 0;

            const Plate* plate = ShownPlateFor(ListFor(state), guid);
            if (!plate) return 0;

            frame::Push(state, plate->frame);
            return 1;
        }

        void OpenLibrary(lua::State* state)
        {
            lua::CreateTable(state, 0, 2);
            lua::PushFunction(state, &GetNamePlates);
            lua::SetField(state, -2, "GetNamePlates");
            lua::PushFunction(state, &GetNamePlateForUnit);
            lua::SetField(state, -2, "GetNamePlateForUnit");
            lua::SetGlobal(state, kTableName);
        }

        // --- the visibility distance -------------------------------------------------------------------

        int __cdecl OnDistanceChanged(console::Variable*, const char*, const char* value, void*)
        {
            double yards = value ? std::atof(value) : 0.0;
            if (!(yards > 0.0)) yards = kDistanceFallbackYards;

            // The client's visibility test compares squared distances, so the square is what it
            // stores; the variable stays in yards because that is what a player can reason about.
            *reinterpret_cast<float*>(off::kNamePlateDistanceSquared) = float(yards * yards);
            return 1;
        }

        // --- the frame-level pass ------------------------------------------------------------------------

        // The engine rewrites every plate's draw level each frame in its own pass, which would undo
        // the ordering below before it was ever seen. This anchor skips that write and re-executes
        // the instruction it displaced. The literal is what the assembler needs; the assertion is
        // what stops it from drifting away from the curated constant beside it.
#define AWOW_NAMEPLATE_LEVEL_RESUME 0x0098EA27
        static_assert(off::kNamePlateLevelUpdateResume == AWOW_NAMEPLATE_LEVEL_RESUME,
                      "the naked stub's resume address must match the curated constant");

        __declspec(naked) void LevelUpdateStub()
        {
            __asm {
                push edi                              // the instruction the patch displaced
                push AWOW_NAMEPLATE_LEVEL_RESUME
                ret
            }
        }
#undef AWOW_NAMEPLATE_LEVEL_RESUME

        hook::JumpPatch g_levelPatch;

        /// One plate in the per-frame depth sort.
        struct Sorted
        {
            frame::Frame* frame;
            Guid          guid;
            float         distanceSquared;
        };

        /// Reused across frames so the steady state allocates nothing; cleared, not freed.
        std::vector<Sorted> g_sorted;

        float DistanceSquared(const float a[3], const float b[3])
        {
            const float dx = a[0] - b[0];
            const float dy = a[1] - b[1];
            const float dz = a[2] - b[2];
            return dx * dx + dy * dy + dz * dz;
        }

        void ApplyFrameLevels()
        {
            if (g_sorted.empty()) return;

            const Guid target = client::objectmgr::TargetGuid();
            std::sort(g_sorted.begin(), g_sorted.end(), [target](const Sorted& a, const Sorted& b) {
                // The target sorts last so that it always draws in front of everything else; the
                // rest sort far to near, so a nearer plate overlaps a farther one.
                if (a.guid == target) return false;
                if (b.guid == target) return true;
                return a.distanceSquared > b.distanceSquared;
            });

            int level = kBaseFrameLevel;
            for (const Sorted& entry : g_sorted) frame::SetLevel(entry.frame, level++);
        }

        // --- the per-frame scan --------------------------------------------------------------------------

        struct ScanContext
        {
            PlateList*           list;
            std::vector<Sorted>* sorted;
            obj::Object*         player;
            float                playerPosition[3];
        };

        bool VisitObject(Guid guid, void* user)
        {
            ScanContext& scan = *static_cast<ScanContext*>(user);

            obj::Object* unit = client::objectmgr::Get(guid, obj::kTypeMaskUnit);
            if (!unit) return true;

            frame::Frame* plateFrame = obj::NamePlateOf(unit);
            if (!plateFrame) return true;

            std::vector<Plate>& plates = scan.list->plates;
            auto existing = std::find_if(plates.begin(), plates.end(),
                                         [plateFrame](const Plate& plate)
                                         { return plate.frame == plateFrame; });

            if (existing == plates.end())
            {
                Plate& plate = plates.emplace_back();
                plate.frame  = plateFrame;
                plate.guid   = guid;
                plate.sweep  = scan.list->sweep;
            }
            else
            {
                // The client hands a pooled plate to a different unit without announcing it, so the
                // GUID is re-read every scan rather than trusted from when the plate was first seen.
                existing->guid  = guid;
                existing->sweep = scan.list->sweep;
            }

            if (scan.player)
            {
                float position[3];
                obj::PositionOf(unit, position);
                scan.sorted->push_back(
                    Sorted{plateFrame, guid, DistanceSquared(scan.playerPosition, position)});
            }
            return true;
        }

        /**
         * @brief Fires one of the two token-carrying events.
         * @param event  Event name.
         * @param index  Zero-based plate index; the token is one-based.
         */
        void FireUnitEvent(const char* event, size_t index)
        {
            char token[kTokenBytes];
            std::snprintf(token, sizeof token, "%s%d", kToken, int(index) + 1);
            fs::Fire(event, "%s", token);
        }

        void PublishEvents(lua::State* state, PlateList& list)
        {
            const int createdEvent = fs::EventId(kEventCreated);

            for (size_t i = 0; i < list.plates.size(); i++)
            {
                Plate& plate = list.plates[i];

                if (plate.sweep != list.sweep)
                {
                    if (plate.flags & kFlagVisible)
                    {
                        FireUnitEvent(kEventUnitRemoved, i);
                        plate.flags &= ~uint32_t(kFlagVisible);
                    }
                    continue;
                }

                if (!(plate.flags & kFlagCreated))
                {
                    // Recorded first, so an event the client never registered costs one failed
                    // lookup rather than one per frame for the rest of the session.
                    plate.flags |= kFlagCreated;
                    if (createdEvent != -1)
                    {
                        // This event carries the frame itself, so its arguments are pushed rather
                        // than described by a format string as the other two are.
                        lua::PushString(state, kEventCreated);
                        frame::Push(state, plate.frame);
                        fs::FireWithStackArgs(createdEvent, state, 2);
                        lua::Pop(state, 2);
                    }
                }

                if (!(plate.flags & kFlagVisible))
                {
                    plate.flags |= kFlagVisible;
                    FireUnitEvent(kEventUnitAdded, i);
                }
            }
        }

        void Scan()
        {
            if (!InWorld()) return;

            lua::State* state = lua::GetState();
            if (!state) return;

            PlateList& list = ListFor(state);

            ScanContext scan{&list, &g_sorted, client::objectmgr::Player(), {0.0f, 0.0f, 0.0f}};
            if (scan.player) obj::PositionOf(scan.player, scan.playerPosition);

            g_sorted.clear();
            client::objectmgr::EnumObjects(&VisitObject, &scan);

            ApplyFrameLevels();
            PublishEvents(state, list);

            list.sweep++;
        }
    }

    bool Initialize()
    {
        g_sorted.reserve(kExpectedPlates);

        hook::registry::AddScriptLibrary(&OpenLibrary);
        hook::registry::AddEvent(kEventCreated);
        hook::registry::AddEvent(kEventUnitAdded);
        hook::registry::AddEvent(kEventUnitRemoved);
        // The variable is never read back, only acted on from its handler, so no slot is kept.
        hook::registry::AddVariable(nullptr, kDistanceVariable, nullptr, console::kFlagUserVariable,
                                    kDistanceDefault, &OnDistanceChanged);
        hook::registry::AddIndexedToken(kToken, &GuidForIndex, &IndexForGuid);
        hook::registry::AddUpdateCallback(&Scan);

        if (!g_levelPatch.Install(off::kNamePlateLevelUpdateAnchor,
                                  reinterpret_cast<const void*>(&LevelUpdateStub)))
        {
            Log(WXL_LOG_ERROR,
                "could not patch the nameplate level pass at 0x%08X; plate depth ordering is off",
                unsigned(off::kNamePlateLevelUpdateAnchor));
            return false;
        }
        return true;
    }
}
