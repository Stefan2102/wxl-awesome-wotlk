#include "feature/Inventory.hpp"

#include "client/Descriptors.hpp"
#include "client/Lua.hpp"
#include "client/ObjectMgr.hpp"
#include "hook/Registry.hpp"

namespace awow::feature::inventory
{
    namespace lua = awow::client::lua;
    namespace obj = awow::client::object;

    namespace
    {
        /**
         * @brief GetInventoryItemTransmog(unit, slot) -> itemId, enchantId
         *
         * Reports what an equipment slot looks like, which is what a transmogrification changes, as
         * opposed to what is actually equipped there. Slots are one-based, matching the stock
         * GetInventoryItem* functions.
         *
         * The unit is resolved as a player rather than as a unit: the record read here only exists
         * on a player's descriptor, and reading it off a creature's would run past the end of it.
         */
        int __cdecl GetInventoryItemTransmog(lua::State* state)
        {
            const char* token = lua::CheckString(state, 1);
            const int   slot  = int(lua::CheckNumber(state, 2)) - 1;

            obj::Object* player = client::objectmgr::GetByScriptArgument(token, obj::kTypeMaskPlayer);
            const obj::VisibleItem* item = obj::VisibleItemOf(player, slot);
            if (!item) return 0;

            lua::PushNumber(state, item->entryId);
            lua::PushNumber(state, item->enchantId);
            return 2;
        }

        void OpenLibrary(lua::State* state)
        {
            lua::SetGlobalFunction(state, "GetInventoryItemTransmog", &GetInventoryItemTransmog);
        }
    }

    void Initialize() { hook::registry::AddScriptLibrary(&OpenLibrary); }
}
