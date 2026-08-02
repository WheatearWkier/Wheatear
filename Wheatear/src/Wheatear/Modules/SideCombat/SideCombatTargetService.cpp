#include "wtpch.h"
#include "SideCombatTargetService.h"

#include "SideCombatMath.h"
#include "Wheatear/Modules/Common/GameplayTargetingService.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

#include <glm/gtx/norm.hpp>

namespace Wheatear::SideCombatTargetService {

    Entity FindNearestAliveEnemy(Scene* scene, const glm::vec3& origin)
    {
        return GameplayTargetingService::FindBest<SideCombatantComponent>(scene,
            [](Entity candidate, const SideCombatantComponent& combatant)
            {
                return candidate.HasComponent<TransformComponent>()
                    && combatant.Team == (int)SideCombatTeam::Enemy
                    && combatant.Alive;
            },
            [&](Entity, const SideCombatantComponent& combatant)
            {
                return glm::length2(combatant.RuntimeGroundPosition - SideCombatMath::ToVec2(origin));
            });
    }

} // namespace Wheatear::SideCombatTargetService
