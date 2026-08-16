#include "wtpch.h"
#include "SideCombatSkillRegistry.h"

#include "Wheatear/Core/Log.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace Wheatear::SideCombatSkillRegistry {

    namespace {

        struct RegisteredBehavior
        {
            std::string DisplayName;
            SkillBehaviorFn Behavior;
        };

        std::unordered_map<std::string, RegisteredBehavior>& Registry()
        {
            static std::unordered_map<std::string, RegisteredBehavior> registry;
            return registry;
        }

        std::unordered_set<std::string>& WarnedIds()
        {
            static std::unordered_set<std::string> warned;
            return warned;
        }

    } // namespace

    void Register(const std::string& id, const char* displayName, SkillBehaviorFn behavior)
    {
        if (id.empty() || !behavior)
            return;
        Registry()[id] = RegisteredBehavior{
            displayName ? displayName : id,
            std::move(behavior)
        };
    }

    void Clear()
    {
        Registry().clear();
        WarnedIds().clear();
    }

    bool Has(const std::string& id)
    {
        return !id.empty() && Registry().find(id) != Registry().end();
    }

    const char* DisplayName(const std::string& id)
    {
        auto it = Registry().find(id);
        return it != Registry().end() ? it->second.DisplayName.c_str() : id.c_str();
    }

    std::vector<std::string> AllIds()
    {
        std::vector<std::string> ids;
        ids.reserve(Registry().size());
        for (const auto& [id, behavior] : Registry())
            ids.push_back(id);
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    bool Run(const std::string& id, SkillBehaviorContext& context)
    {
        if (id.empty())
            return false;

        auto it = Registry().find(id);
        if (it == Registry().end())
        {
            if (WarnedIds().insert(id).second)
                WT_CORE_WARN("SideCombat: unknown custom skill behaviour '{}' "
                    "(registered behaviours: {}).", id, AllIds().size());
            return false;
        }
        it->second.Behavior(context);
        return true;
    }

} // namespace Wheatear::SideCombatSkillRegistry
