#include "wtpch.h"
#include "ArcadeCombatOutcomeService.h"

#include "Wheatear/Modules/Common/GameplayFlowService.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>

namespace Wheatear::ArcadeCombatOutcomeService {

    void UpdateResultTransition(Scene* scene,
        ArcadeCombatLevelComponent& level,
        Entity player,
        Entity boss,
        float dt)
    {
        if (!level.RuntimeVictory && !level.RuntimeDefeat)
            return;

        level.RuntimeResultTimer += dt;

        if (player && player.HasComponent<ArcadeCombatantComponent>())
            player.GetComponent<ArcadeCombatantComponent>().ControlsLocked = true;

        if (level.RuntimeVictory && boss && boss.HasComponent<SpriteRendererComponent>())
        {
            const float bossFadeDuration = std::max(0.01f, level.BossDefeatFadeDuration);
            const float bossFade = std::clamp(level.RuntimeResultTimer / bossFadeDuration, 0.0f, 1.0f);
            boss.GetComponent<SpriteRendererComponent>().Color.a = 1.0f - bossFade;
        }

        const bool victory = level.RuntimeVictory;
        const std::string& command = victory ? level.VictorySceneCommand : level.DefeatSceneCommand;
        if (command.empty())
            return;

        const float delay = std::max(0.0f, victory ? level.VictoryReturnDelay : level.DefeatReturnDelay);
        const float sceneFadeDuration = std::max(0.01f, level.ResultSceneFadeDuration);
        const float sceneFadeStart = std::max(0.0f, delay - sceneFadeDuration);
        const float sceneFade = std::clamp(
            (level.RuntimeResultTimer - sceneFadeStart) / sceneFadeDuration,
            0.0f,
            1.0f);
        UIRuntimeTools::SetImageAlpha(scene, level.FadeEntityName, sceneFade);

        if (level.RuntimeRequestedCommand.empty())
        {
            GameplayFlowService::TryIssueDelayedCommand(level.RuntimeResultTimer,
                delay,
                level.RuntimeResultCommandIssued,
                level.RuntimeRequestedCommand,
                command);
        }
    }

} // namespace Wheatear::ArcadeCombatOutcomeService
