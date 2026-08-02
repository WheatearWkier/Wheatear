#include "wtpch.h"
#include "ArcadeCombatPresentationService.h"

#include "ArcadeCombatMath.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <cmath>

namespace Wheatear::ArcadeCombatPresentationService {

    namespace {

        constexpr float Pi = 3.1415926535f;

    } // namespace

    void UpdateStartFade(Scene* scene, ArcadeCombatLevelComponent& level, float dt)
    {
        if (level.StartFadeDuration <= 0.0f)
        {
            level.RuntimeFadeAlpha = 0.0f;
            UIRuntimeTools::SetImageAlpha(scene, level.FadeEntityName, 0.0f);
            return;
        }

        level.RuntimeFadeAlpha = std::max(0.0f,
            level.RuntimeFadeAlpha - dt / level.StartFadeDuration);
        UIRuntimeTools::SetImageAlpha(scene, level.FadeEntityName, level.RuntimeFadeAlpha);
    }

    void UpdateTriggerGlow(Scene* scene, ArcadeCombatLevelComponent& level)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<ArcadeTriggerComponent, CircleRendererComponent>())
        {
            auto& trigger = registry.get<ArcadeTriggerComponent>(e);
            auto& circle = registry.get<CircleRendererComponent>(e);
            const float pulse = 0.55f + 0.25f * std::sin(level.RuntimeElapsed * 5.0f);
            circle.Color.a = trigger.Triggered ? 0.15f : pulse;
        }
    }

    void UpdateIntroTrigger(Scene* scene,
        ArcadeCombatLevelComponent& level,
        Entity player,
        Entity boss)
    {
        if (!scene || !player || !player.HasComponent<TransformComponent>())
            return;

        auto& registry = scene->GetRegistry();
        auto& playerTransform = player.GetComponent<TransformComponent>();

        for (auto e : registry.view<TransformComponent, ArcadeTriggerComponent>())
        {
            auto& triggerTransform = registry.get<TransformComponent>(e);
            auto& trigger = registry.get<ArcadeTriggerComponent>(e);
            if (trigger.Triggered || trigger.Type != ArcadeTriggerType::BossIntro)
                continue;

            if (ArcadeCombatMath::Distance2D(playerTransform.Translation, triggerTransform.Translation) <= trigger.Radius)
            {
                trigger.Triggered = true;
                level.RuntimeBossIntroStarted = true;
                level.RuntimeBossIntroFinished = false;
                if (boss && boss.HasComponent<ArcadeBossComponent>())
                    boss.GetComponent<ArcadeBossComponent>().RuntimeIntroTimer = 0.0f;
                break;
            }
        }
    }

    void UpdateBossIntro(Scene*,
        ArcadeCombatLevelComponent& level,
        Entity player,
        Entity boss,
        float dt)
    {
        if (!boss || !boss.HasComponent<ArcadeBossComponent>())
        {
            level.RuntimeBossIntroFinished = true;
            return;
        }

        auto& bossComponent = boss.GetComponent<ArcadeBossComponent>();
        bossComponent.RuntimeIntroTimer += dt;

        const float duration = std::max(0.01f, bossComponent.IntroDuration);
        const float t = std::clamp(bossComponent.RuntimeIntroTimer / duration, 0.0f, 1.0f);
        const float ease = 1.0f - (1.0f - t) * (1.0f - t);

        if (boss.HasComponent<TransformComponent>())
        {
            auto& transform = boss.GetComponent<TransformComponent>();
            transform.Translation = glm::mix(
                bossComponent.IntroStartPosition,
                bossComponent.FightPosition,
                ease);
            transform.Translation.y += std::sin(t * Pi) * 0.7f;
        }

        if (boss.HasComponent<SpriteRendererComponent>())
            boss.GetComponent<SpriteRendererComponent>().Color.a = t;

        if (player && player.HasComponent<ArcadeCombatantComponent>())
            player.GetComponent<ArcadeCombatantComponent>().ControlsLocked = true;

        if (t >= 1.0f)
        {
            bossComponent.Active = true;
            level.RuntimeBossIntroFinished = true;
            if (player && player.HasComponent<ArcadeCombatantComponent>())
                player.GetComponent<ArcadeCombatantComponent>().ControlsLocked = false;
        }
    }

} // namespace Wheatear::ArcadeCombatPresentationService
