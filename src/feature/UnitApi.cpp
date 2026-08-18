#include "feature/UnitApi.hpp"

#include "client/Descriptors.hpp"
#include "client/Lua.hpp"
#include "client/ObjectMgr.hpp"
#include "hook/Registry.hpp"

namespace awow::feature::unitApi
{
    namespace lua = awow::client::lua;
    namespace obj = awow::client::object;

    namespace
    {
        /**
         * @brief Answers a unit-state question from UNIT_FIELD_FLAGS.
         * @param state  Script state; argument 1 is the unit.
         * @param mask   Flags any of which makes the answer true.
         * @return How many values were pushed: 1 when true, 0 when false.
         *
         * Nothing is pushed for false, so the call reads as nil rather than as false. That is what
         * the stock UnitIs* functions do, and what addons written against them test for.
         */
        int Report(lua::State* state, uint32_t mask)
        {
            const char* token = lua::CheckString(state, 1);
            obj::Object* unit = client::objectmgr::GetByScriptArgument(token, obj::kTypeMaskUnit);
            if (!unit || !(obj::UnitFlagsOf(unit) & mask)) return 0;

            lua::PushNumber(state, 1.0);
            return 1;
        }

        int __cdecl UnitIsControlled(lua::State* state)
        { return Report(state, obj::kUnitFlagsControlled); }

        int __cdecl UnitIsDisarmed(lua::State* state)
        { return Report(state, obj::kUnitFlagDisarmed); }

        int __cdecl UnitIsSilenced(lua::State* state)
        { return Report(state, obj::kUnitFlagSilenced); }

        void OpenLibrary(lua::State* state)
        {
            lua::SetGlobalFunction(state, "UnitIsControlled", &UnitIsControlled);
            lua::SetGlobalFunction(state, "UnitIsDisarmed",   &UnitIsDisarmed);
            lua::SetGlobalFunction(state, "UnitIsSilenced",   &UnitIsSilenced);
        }
    }

    void Initialize() { hook::registry::AddScriptLibrary(&OpenLibrary); }
}
