// FrameScript events: resolving an event name to the id the client fires by, and firing one.

#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "client/ClientOffsets.hpp"
#include "client/Lua.hpp"

#include "game/Binding.hpp"

namespace awow::client::framescript
{
    namespace off = awow::client::offsets;

    /**
     * @brief One entry of the client's event table.
     *
     * Only the prefix this module reads is declared. Entries are reached through a pointer array, so
     * the record's full size never enters an address computation and declaring the tail would mean
     * naming fields nothing here uses.
     */
    struct Event
    {
        uint32_t    hash;      ///< hash of name, the table's lookup key
        uint32_t    gap04[4];
        const char* name;
    };
    static_assert(offsetof(Event, name) == 0x14, "framescript::Event layout does not match the client");

    /// The event table: an array of records, indexed by the id the client fires by.
    struct EventTable
    {
        size_t  reserve;
        size_t  count;
        Event** entries;
    };

    /// @return The event table. Empty until the client builds it during script initialisation.
    inline EventTable* Table()
    { return reinterpret_cast<EventTable*>(off::kFrameScriptEventList); }

    /**
     * @brief Resolves an event name to the id the client fires it by.
     * @param name  Event name.
     * @return The id, or -1 when the table does not carry that name.
     *
     * An event's id is its position in the table, so it is only stable within one run and must be
     * resolved rather than remembered across a table rebuild.
     */
    inline int EventId(const char* name)
    {
        const EventTable* table = Table();
        if (!name || table->count == 0) return -1;

        const uint32_t hash = wxl::game::Native<off::StringHashFn>(off::kStringHash)(name);
        for (size_t i = 0; i < table->count; i++)
        {
            const Event* event = table->entries[i];
            // Compare the hash first, then the text: the hash is not claimed to be collision-free,
            // and the pointer test short-circuits the common case of a literal the client interned.
            if (event && event->hash == hash &&
                (event->name == name || std::strcmp(event->name, name) == 0))
                return int(i);
        }
        return -1;
    }

    /**
     * @brief Fires an event whose arguments are already on the script stack.
     * @param eventId   Id from EventId().
     * @param state     Script state the arguments were pushed on.
     * @param argCount  How many arguments were pushed.
     */
    inline void FireWithStackArgs(int eventId, lua::State* state, int argCount)
    {
        wxl::game::Native<void(__cdecl*)(int, lua::State*, int)>(off::kFrameScriptFireEventInner)(
            eventId, state, argCount);
    }

    /**
     * @brief Fires an event by name, formatting its arguments.
     * @param name    Event name; ignored when the table does not carry it.
     * @param format  printf-style format describing the arguments that follow.
     */
    inline void Fire(const char* name, const char* format, ...)
    {
        const int eventId = EventId(name);
        if (eventId == -1) return;

        va_list args;
        va_start(args, format);
        wxl::game::Native<void(__cdecl*)(int, const char*, va_list)>(off::kFrameScriptFireEventV)(
            eventId, format, args);
        va_end(args);
    }
}
