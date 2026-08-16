#include "wtpch.h"
#include "EffectRegistry.h"

#include "ActionTypes.h"
#include "Wheatear/Core/Log.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace Wheatear::WAO {

    namespace {

        struct RegisteredEffect
        {
            std::string DisplayName;
            CustomEffectHandler Handler;
        };

        std::unordered_map<std::string, RegisteredEffect>& Registry()
        {
            static std::unordered_map<std::string, RegisteredEffect> registry;
            return registry;
        }

        // Warn once per unknown id so a typo in data surfaces without
        // spamming the log every frame.
        std::unordered_set<std::string>& WarnedIds()
        {
            static std::unordered_set<std::string> warned;
            return warned;
        }

    } // namespace

    void EffectRegistry::Register(const std::string& id,
        const char* displayName,
        CustomEffectHandler handler)
    {
        if (id.empty() || !handler)
            return;
        Registry()[id] = RegisteredEffect{
            displayName ? displayName : id,
            std::move(handler)
        };
    }

    void EffectRegistry::Clear()
    {
        Registry().clear();
        WarnedIds().clear();
    }

    bool EffectRegistry::Has(const std::string& id)
    {
        return !id.empty() && Registry().find(id) != Registry().end();
    }

    const char* EffectRegistry::DisplayName(const std::string& id)
    {
        auto it = Registry().find(id);
        return it != Registry().end() ? it->second.DisplayName.c_str() : id.c_str();
    }

    std::vector<std::string> EffectRegistry::AllIds()
    {
        std::vector<std::string> ids;
        ids.reserve(Registry().size());
        for (const auto& [id, effect] : Registry())
            ids.push_back(id);
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    bool EffectRegistry::Run(const std::string& id, CustomEffectContext& context)
    {
        if (id.empty())
            return false;

        auto it = Registry().find(id);
        if (it == Registry().end())
        {
            if (WarnedIds().insert(id).second)
                WT_CORE_WARN("WAO: unknown custom effect type '{}' (registered ids: {}).",
                    id, AllIds().size());
            return false;
        }
        return it->second.Handler(context);
    }

} // namespace Wheatear::WAO
