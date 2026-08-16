#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

#include <functional>
#include <string>
#include <vector>

namespace Wheatear {

    struct SideCombatLevelComponent;
    struct SideCombatantComponent;
    struct SidePlayerControllerComponent;
    class Scene;

    // Extension point for side-combat skill behaviours. A custom skill kind
    // in the tuning skill-slot table dispatches here: registering a behaviour
    // (one function, no enum changes) makes it selectable in the editor's
    // "Skill Slots" tab and triggerable through a normal input action.
    namespace SideCombatSkillRegistry {

        struct SkillBehaviorContext
        {
            Scene* Scene = nullptr;
            SideCombatLevelComponent* Level = nullptr;
            Entity Player;
            SideCombatantComponent* Combatant = nullptr;
            SidePlayerControllerComponent* Controller = nullptr;
        };

        using SkillBehaviorFn = std::function<void(SkillBehaviorContext& context)>;

        WHEATEAR_API void Register(const std::string& id,
            const char* displayName,
            SkillBehaviorFn behavior);
        WHEATEAR_API void Clear();

        WHEATEAR_API bool Has(const std::string& id);
        WHEATEAR_API const char* DisplayName(const std::string& id);
        WHEATEAR_API std::vector<std::string> AllIds();

        // Runs the registered behaviour. Returns false when the id is unknown.
        WHEATEAR_API bool Run(const std::string& id, SkillBehaviorContext& context);

    } // namespace SideCombatSkillRegistry

} // namespace Wheatear
