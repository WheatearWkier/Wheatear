#include "wtpch.h"
#include "SideCombatSystem.h"

#include "Wheatear/Input/Input.h"
#include "Wheatear/Input/InputBindingService.h"
#include "Wheatear/Input/KeyCodes.h"
#include "Wheatear/Input/MouseButtonCodes.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Modules/Progression/ProgressionContent.h"
#include "Wheatear/Modules/SideCombat/SideCombatComboService.h"
#include "Wheatear/Modules/SideCombat/SideCombatEnemyAIService.h"
#include "Wheatear/Modules/SideCombat/SideCombatEntityReferenceService.h"
#include "Wheatear/Modules/SideCombat/SideCombatFeedbackService.h"
#include "Wheatear/Modules/SideCombat/SideCombatHitboxService.h"
#include "Wheatear/Modules/SideCombat/SideCombatHudService.h"
#include "Wheatear/Modules/SideCombat/SideCombatLifecycleService.h"
#include "Wheatear/Modules/SideCombat/SideCombatOutcomeService.h"
#include "Wheatear/Modules/SideCombat/SideCombatPhysicsService.h"
#include "Wheatear/Modules/SideCombat/SideCombatPickupService.h"
#include "Wheatear/Modules/SideCombat/SideCombatPlayerService.h"
#include "Wheatear/Modules/SideCombat/SideCombatTuningService.h"
#include "Wheatear/Runtime/CommandBus.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"

#include <algorithm>
#include <string>

namespace Wheatear {

    namespace {

        static void ApplyActiveDungeonProfile(SideCombatLevelComponent& level)
        {
            const std::string& dungeonId = GameProgress::GetActiveSideCombatDungeonId();
            if (dungeonId.empty())
                return;

            level.LevelId = dungeonId;
            level.VictorySceneCommand = "event:side_combat_victory";
            level.DefeatSceneCommand = "event:side_combat_retry";

            if (const auto* dungeon = ProgressionContent::FindDungeon(dungeonId);
                dungeon && !dungeon->FirstClearRewardText.empty())
            {
                level.FirstClearRewardText = dungeon->FirstClearRewardText;
            }
        }

    } // namespace


    void SideCombatSystem::OnRuntimeStart(Scene* scene)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<SideCombatLevelComponent>())
        {
            auto& level = registry.get<SideCombatLevelComponent>(e);
            if (!level.PlayOnStart)
                continue;

            ApplyActiveDungeonProfile(level);
            SideCombatLifecycleService::ResetLevelRuntime(scene, level);
            SideCombatLifecycleService::ResetCombatants(scene, level);
        }
    }

    void SideCombatSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        if (!scene)
            return;

        const float dt = std::min(0.05f, ts.GetSeconds());
        auto& registry = scene->GetRegistry();

        // Route gameplay commands through the action layer so commands and
        // physical input share the same edge-detection path.
        const std::vector<std::string> commands = CommandBus::DrainGameplayCommands("side:");
        if (!commands.empty())
        {
            // side:item:N maps onto the data-driven item slot table; resolve
            // the tuning of the first active level to translate N into the
            // slot's action id (extra slots need no code).
            const SideCombatTuningService::SideCombatTuning* itemTuning = nullptr;
            for (auto levelEntity : registry.view<SideCombatLevelComponent>())
            {
                auto& level = registry.get<SideCombatLevelComponent>(levelEntity);
                if (level.PlayOnStart)
                {
                    itemTuning = &SideCombatTuningService::GetTuning(level);
                    break;
                }
            }

            for (const std::string& command : commands)
            {
                if (command.rfind("side:item:", 0) == 0)
                {
                    const std::string key = command.substr(10);
                    bool routed = false;
                    if (itemTuning)
                    {
                        for (const auto& slot : itemTuning->ItemSlots)
                        {
                            if (std::to_string(slot.Slot) == key)
                            {
                                InputBindingService::InjectActionPress(slot.ActionId);
                                routed = true;
                                break;
                            }
                        }
                    }
                    if (!routed)
                        WT_CORE_WARN("SideCombat: no item slot matches command '{}'", command);
                    continue;
                }
                if (command == "side:basic")
                    InputBindingService::InjectActionPress("side.basic");
                else if (command == "side:launcher")
                    InputBindingService::InjectActionPress("side.launcher");
                else if (command == "side:magic")
                    InputBindingService::InjectActionPress("side.magic");
                else if (command == "side:support")
                    InputBindingService::InjectActionPress("side.support");
                else if (command == "side:dash")
                    InputBindingService::InjectActionPress("side.dash");
                else if (command == "side:break_limit")
                    InputBindingService::InjectActionPress("side.break_limit");
            }
        }

        float horizontal = 0.0f;
        if (InputBindingService::IsActionDown("move.left"))
            horizontal -= 1.0f;
        if (InputBindingService::IsActionDown("move.right"))
            horizontal += 1.0f;

        float lane = 0.0f;
        if (InputBindingService::IsActionDown("move.down"))
            lane -= 1.0f;
        if (InputBindingService::IsActionDown("move.up"))
            lane += 1.0f;

        const SideCombatPlayerService::PlayerInputState input{
            horizontal,
            lane
        };

        for (auto levelEntity : registry.view<SideCombatLevelComponent>())
        {
            auto& level = registry.get<SideCombatLevelComponent>(levelEntity);
            if (!level.PlayOnStart)
                continue;

            level.RuntimeElapsed += dt;
            SideCombatFeedbackService::UpdateStartFade(scene, level, dt);
            SideCombatFeedbackService::UpdateCameraFeedback(scene, level, dt);

            if (InputBindingService::IsActionPressed("game.pause"))
                level.RuntimePaused = !level.RuntimePaused;

            Entity player = SideCombatEntityReferenceService::ResolvePlayer(scene, level);
            Entity boss = SideCombatEntityReferenceService::ResolveBoss(scene, level);

            if (!level.RuntimePaused)
            {
                const auto& tuning = SideCombatTuningService::GetTuning(level);
                float simulationDt = dt;
                float simulationTimeScale = 1.0f;
                if (level.RuntimeHitPauseTimer > 0.0f)
                {
                    level.RuntimeHitPauseTimer = std::max(0.0f, level.RuntimeHitPauseTimer - dt);
                    simulationTimeScale = std::min(
                        simulationTimeScale,
                        std::clamp(tuning.Feedback.HitPauseTimeScale, 0.0f, 1.0f));
                }
                if (level.RuntimeCinematicTimer > 0.0f)
                {
                    simulationTimeScale = std::min(
                        simulationTimeScale,
                        std::clamp(level.RuntimeCinematicTimeScale, 0.02f, 1.0f));
                }
                simulationDt = dt * simulationTimeScale;

                SideCombatComboService::UpdateCombo(level, simulationDt);
                SideCombatPlayerService::UpdatePlayer(scene, level, player, simulationDt, input);
                SideCombatEnemyAIService::UpdateEnemies(scene, level, player, simulationDt);
                SideCombatHitboxService::UpdateHitboxes(scene, level, simulationDt);
                SideCombatPhysicsService::UpdateCombatants(scene, level, simulationDt);
                SideCombatOutcomeService::UpdateDeathsAndVictory(scene, level, player);
                SideCombatPickupService::UpdatePickups(scene, level, player, simulationDt);
                SideCombatOutcomeService::UpdateResultTransition(scene, level, dt);
            }

            SideCombatHudService::UpdateUI(scene, level, player, boss);
        }
    }

} // namespace Wheatear
