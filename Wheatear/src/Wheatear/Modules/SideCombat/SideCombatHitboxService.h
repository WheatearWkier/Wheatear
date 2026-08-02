#pragma once

#include "SideCombatComponents.h"
#include "SideCombatTuningService.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"

#include <glm/glm.hpp>

#include <string>

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::SideCombatHitboxService {

    WHEATEAR_API Entity CreateHitbox(Scene* scene,
        const std::string& name,
        entt::entity ownerEntity,
        const glm::vec2& sourceGroundPosition,
        float sourceAirHeight,
        float facing,
        SideAttackKind kind,
        const std::string& actionRecipeId,
        int team,
        const SideCombatTuningService::SideAttackTuning& tuning,
        float damage,
        const SideCombatTuningService::SideCombatTuning& combatTuning);

    WHEATEAR_API void DestroyOwnedHitboxes(Scene* scene, entt::entity ownerEntity);
    WHEATEAR_API void ApplyFrameTexture(SpriteRendererComponent& sprite, const SideHitboxComponent& hitbox);
    WHEATEAR_API bool OverlapsHitbox(const SideHitboxComponent& hitbox, const SideCombatantComponent& target);
    WHEATEAR_API void UpdateHitboxes(Scene* scene,
        SideCombatLevelComponent& level,
        float dt);

} // namespace Wheatear::SideCombatHitboxService
