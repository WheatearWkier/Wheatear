#pragma once

#include "ArcadeCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::ArcadeCombatPresentationService {

    WHEATEAR_API void UpdateStartFade(Scene* scene, ArcadeCombatLevelComponent& level, float dt);
    WHEATEAR_API void UpdateTriggerGlow(Scene* scene, ArcadeCombatLevelComponent& level);
    WHEATEAR_API void UpdateIntroTrigger(Scene* scene,
        ArcadeCombatLevelComponent& level,
        Entity player,
        Entity boss);
    WHEATEAR_API void UpdateBossIntro(Scene* scene,
        ArcadeCombatLevelComponent& level,
        Entity player,
        Entity boss,
        float dt);

} // namespace Wheatear::ArcadeCombatPresentationService
