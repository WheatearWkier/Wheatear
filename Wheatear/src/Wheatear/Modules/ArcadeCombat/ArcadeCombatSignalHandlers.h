#pragma once

#include "Wheatear/Core/Core.h"

#include <glm/glm.hpp>

#include <string>

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::ArcadeCombatSignalHandlers {

    inline constexpr const char* SpawnProjectileSignal = "arcade.projectile.spawn";

    struct ProjectileSpawnPayload
    {
        Scene* SceneContext = nullptr;
        std::string EntityName;
        glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
        glm::vec2 Velocity = { 0.0f, 0.0f };
        float Damage = 0.0f;
        float Lifetime = 0.0f;
        float Radius = 0.0f;
        int Team = 0;
        glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
        bool Heavy = false;
        bool Melee = false;
    };

    WHEATEAR_API void RegisterHandlers();
    WHEATEAR_API void EmitProjectileSpawn(const std::string& actionId,
        const std::string& source,
        const std::string& detail,
        const ProjectileSpawnPayload& payload);

} // namespace Wheatear::ArcadeCombatSignalHandlers
