#pragma once

#include "Wheatear/Core/Core.h"

#include <functional>
#include <string>
#include <vector>

namespace Wheatear::WAO {

    struct AttributeStore;
    struct EffectSpec;

    // Runtime context handed to a custom effect handler. Handlers operate on
    // the plain attribute dictionary only (no component types), so the
    // caller maps component state in and writes the mutated values back.
    struct CustomEffectContext
    {
        AttributeStore* Vars = nullptr;   // source.* / target.* / controller.*
        float InValue = 0.0f;             // the effect's authored value
        float OutValue = 0.0f;            // optional result the caller may use
    };

    // A custom effect semantic. Returning false means "not applied" (the
    // caller treats the effect as failed, e.g. respects cooldowns).
    using CustomEffectHandler = std::function<bool(CustomEffectContext& context)>;

    // Data-driven extension point for WAO effects: new effect *semantics*
    // (lifesteal, shield, poison ticks, ...) are one registration here plus
    // the effect authored in the WAO Action Editor (customType: <id>).
    // Effects keep working in every module that routes through the registry.
    class WHEATEAR_API EffectRegistry
    {
    public:
        static void Register(const std::string& id,
            const char* displayName,
            CustomEffectHandler handler);
        static void Clear();

        static bool Has(const std::string& id);
        static const char* DisplayName(const std::string& id);
        static std::vector<std::string> AllIds();

        // Runs the registered handler for `id`. Returns false when the id is
        // unknown (warns once per id) or the handler declined.
        static bool Run(const std::string& id, CustomEffectContext& context);
    };

} // namespace Wheatear::WAO
