#pragma once

#include "Wheatear/Core/Core.h"

#include <optional>
#include <string>
#include <vector>

namespace Wheatear::Logic {

    struct CommandDescriptor
    {
        std::string Name;          // "scene", "vn", "event" (no trailing colon)
        std::string Category;      // "Flow", "Progression", "Combat", ...
        std::string Description;
        std::string ArgumentHint;  // e.g. "@UUID event_name"
        bool Gameplay = false;     // queued for a gameplay module
    };

    // Central catalog for CommandBus prefixes. The catalog does not replace
    // command handlers; it gives every DSL, editor validator and diagnostic
    // panel one place to ask "is this command known and who owns it?".
    class WHEATEAR_API CommandRegistry
    {
    public:
        static void Register(const CommandDescriptor& descriptor);
        static bool Validate(const std::string& command, std::string* error = nullptr);
        // Returns a copy: registrations may grow the catalog while the caller
        // still holds the result.
        static std::optional<CommandDescriptor> Find(const std::string& command);
        static std::vector<CommandDescriptor> Snapshot();

    private:
        static void EnsureBuiltins();
    };

} // namespace Wheatear::Logic
