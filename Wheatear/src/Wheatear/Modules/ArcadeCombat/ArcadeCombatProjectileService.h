#pragma once

#include "Wheatear/Core/Core.h"

#include <glm/glm.hpp>

#include <string>

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::ArcadeCombatProjectileService {

    WHEATEAR_API void CreateProjectile(Scene* scene,
        const std::string& name,
        const glm::vec3& position,
        const glm::vec2& velocity,
        float damage,
        float lifetime,
        float radius,
        int team,
        const glm::vec4& color,
        bool heavy = false,
        bool melee = false);

    WHEATEAR_API void DestroyProjectiles(Scene* scene);
    WHEATEAR_API void UpdateProjectiles(Scene* scene, float dt);

} // namespace Wheatear::ArcadeCombatProjectileService
