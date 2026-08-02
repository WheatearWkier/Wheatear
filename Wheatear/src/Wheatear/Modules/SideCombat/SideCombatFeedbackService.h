#pragma once

#include "SideCombatComponents.h"
#include "Wheatear/Core/Core.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::SideCombatFeedbackService {

    WHEATEAR_API void PlaySfx(const std::string& path, float volume = 1.0f);
    WHEATEAR_API void TriggerHitFeedback(Scene* scene,
        SideCombatLevelComponent& level,
        const SideHitboxComponent& hitbox);
    WHEATEAR_API void UpdateCameraFeedback(Scene* scene,
        SideCombatLevelComponent& level,
        float dt);
    WHEATEAR_API void UpdateStartFade(Scene* scene,
        SideCombatLevelComponent& level,
        float dt);

} // namespace Wheatear::SideCombatFeedbackService
