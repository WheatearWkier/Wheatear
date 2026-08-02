#pragma once

#include "TurnCombatComponents.h"
#include "TurnCombatSkillService.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

#include <string>
#include <vector>

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::TurnCombatTargetService {

    WHEATEAR_API std::vector<Entity> CollectCombatants(Scene* scene);
    WHEATEAR_API bool HasAliveTeam(Scene* scene, int team);
    WHEATEAR_API std::string JoinTurnOrder(Scene* scene, const TurnCombatLevelComponent& level);
    WHEATEAR_API void BuildTurnQueue(Scene* scene, TurnCombatLevelComponent& level);
    WHEATEAR_API bool IsValidTarget(const TurnCombatSkillService::TurnSkillDefinition& skill,
        const TurnCombatantComponent& actor,
        const TurnCombatantComponent& target);
    WHEATEAR_API std::vector<Entity> ResolveTargets(Scene* scene,
        const TurnCombatSkillService::TurnSkillDefinition& skill,
        Entity actor,
        Entity explicitTarget);
    WHEATEAR_API Entity ChooseTargetForAI(Scene* scene,
        Entity actor,
        const TurnCombatSkillService::TurnSkillDefinition& skill);
    WHEATEAR_API Entity FindTurnTarget(Scene* scene, const std::string& token);

} // namespace Wheatear::TurnCombatTargetService
