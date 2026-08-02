#include "wtpch.h"
#include "ArcadeCombatLifecycleService.h"

#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>

namespace Wheatear::ArcadeCombatLifecycleService {

    void ResetLevelRuntime(Scene* scene, ArcadeCombatLevelComponent& level)
    {
        level.RuntimeElapsed = 0.0f;
        level.RuntimeFadeAlpha = 1.0f;
        level.RuntimePaused = false;
        level.RuntimeBossIntroStarted = false;
        level.RuntimeBossIntroFinished = false;
        level.RuntimeVictory = false;
        level.RuntimeDefeat = false;
        level.RuntimeResultTimer = 0.0f;
        level.RuntimeResultCommandIssued = false;
        level.RuntimeRequestedCommand.clear();

        UIRuntimeTools::SetWidgetVisible(scene, level.PausePanelEntityName, false);
        UIRuntimeTools::SetImageAlpha(scene, level.FadeEntityName, 1.0f);
    }

    void ResetCombatants(Scene* scene)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<ArcadeCombatantComponent>())
        {
            auto& combatant = registry.get<ArcadeCombatantComponent>(e);
            combatant.Health = std::max(0.0f, combatant.MaxHealth);
            combatant.Alive = combatant.Health > 0.0f;
            combatant.ControlsLocked = false;
        }

        for (auto e : registry.view<ArcadeCoverComponent>())
        {
            auto& cover = registry.get<ArcadeCoverComponent>(e);
            cover.Health = cover.MaxHealth;
        }

        for (auto e : registry.view<ArcadeTriggerComponent>())
            registry.get<ArcadeTriggerComponent>(e).Triggered = false;
    }

    void ResetBossPresentation(Entity boss)
    {
        if (!boss || !boss.HasComponent<ArcadeBossComponent>())
            return;

        auto& bossComponent = boss.GetComponent<ArcadeBossComponent>();
        bossComponent.RuntimeIntroTimer = 0.0f;
        bossComponent.RuntimeShootTimer = 0.0f;
        bossComponent.RuntimeJumpTimer = 0.0f;
        bossComponent.RuntimeJumpProgress = 0.0f;
        bossComponent.RuntimeJumping = false;

        if (boss.HasComponent<TransformComponent>())
        {
            boss.GetComponent<TransformComponent>().Translation =
                bossComponent.Active ? bossComponent.FightPosition : bossComponent.IntroStartPosition;
        }
        if (boss.HasComponent<SpriteRendererComponent>())
            boss.GetComponent<SpriteRendererComponent>().Color.a = bossComponent.Active ? 1.0f : 0.0f;
    }

} // namespace Wheatear::ArcadeCombatLifecycleService
