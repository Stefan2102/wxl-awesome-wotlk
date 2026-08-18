// Console variables: the client's own registry, as this module adds to and reads from it.

#pragma once

#include <cstdint>

#include "client/ClientOffsets.hpp"

#include "game/Binding.hpp"

namespace awow::client::console
{
    namespace off = awow::client::offsets;

    /// Registration flags. The client defines a wider set; only what this module registers with is
    /// named here, because naming the rest would mean guessing at semantics it never exercises.
    enum Flags : uint32_t
    {
        /// What the client passes for an ordinary user-settable variable, and what both of this
        /// module's variables register with. Named for that use: its exact meaning inside the
        /// client's own flag set is unconfirmed.
        kFlagUserVariable = 0x001,
    };

    /**
     * @brief One console variable, as the client lays it out.
     *
     * Named fields are the ones this module reads or writes; the gaps are the client's own state and
     * are deliberately left unnamed rather than guessed at. The size assertion is the guard: a
     * mis-sized layout would make the registry's array indexing read the wrong variable entirely.
     */
    struct Variable
    {
        /// Change handler. Returning zero rejects the assignment and keeps the previous value.
        using Handler = int(__cdecl*)(Variable* self, const char* previous, const char* next,
                                      void* userData);

        uint32_t    hash;
        uint32_t    gap04[4];
        const char* name;
        uint32_t    gap18;
        Flags       flags;
        uint32_t    gap20[2];
        const char* value;        ///< current value, always in text form
        uint32_t    gap2C[5];
        uint32_t    valueAsBool;
        uint32_t    gap44[9];
        Handler     handler;
        void*       userData;
    };
    static_assert(sizeof(Variable) == 0x70, "console::Variable layout does not match the client");

    /**
     * @brief Creates a console variable.
     * @param name          Name it is set by.
     * @param description   Help text, or null.
     * @param flags         Registration flags.
     * @param initialValue  Value before anything assigns one.
     * @param handler       Invoked on every assignment, including the initial one.
     * @return The variable, or null when one of that name already exists.
     *
     * The four trailing client arguments select a category and a default-tracking mode this module
     * has no use for; zero is what the client itself passes for a plain variable.
     */
    inline Variable* Register(const char* name, const char* description, Flags flags,
                              const char* initialValue, Variable::Handler handler)
    {
        using RegisterFn = Variable*(__cdecl*)(const char*, const char*, uint32_t, const char*,
                                               Variable::Handler, int, int, int, int);
        return wxl::game::Native<RegisterFn>(off::kRegisterCVar)(name, description, uint32_t(flags),
                                                                 initialValue, handler, 0, 0, 0, 0);
    }
}
