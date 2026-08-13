#include "wtpch.h"
#include "SideCombatOutcomeService.h"

#include "SideCombatPickupService.h"
#include "SideCombatResultService.h"
#include "SideCombatTuningService.h"
#include "Wheatear/Gameplay/Services/GameplayFlowService.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear::SideCombatOutcomeService {

    namespace {

        constexpr float GruntDeathRemoveDelay = 0.78f;

        static int GetWaveCount(const SideCombatLevelComponent& level)
        {
            return std::clamp(level.WaveCount, 1, 3);
        }

        static float GetWaveRightWall(const SideCombatLevelComponent& level, int waveIndex)
        {
            switch (std::clamp(waveIndex, 0, 2))
            {
            case 0: return level.Wave1RightWall;
            case 1: return level.Wave2RightWall;
            default: return level.Wave3RightWall;
            }
        }

        static bool HasTagPrefix(const TagComponent& tag, const std::string& prefix)
        {
            return tag.Tag.rfind(prefix, 0) == 0;
        }

        static int GetEnemyWaveIndex(Scene* scene,
            const SideCombatLevelComponent& level,
            entt::entity entity,
            const SideEnemyAIComponent& ai)
        {
            if (!scene)
                return 0;

            auto& registry = scene->GetRegistry();
            if (ai.WaveIndex >= 0)
                return std::clamp(ai.WaveIndex, 0, GetWaveCount(level) - 1);

            if (ai.Kind == SideEnemyKind::BearBoss)
                return GetWaveCount(level) - 1;

            if (registry.all_of<TagComponent>(entity))
            {
                const auto& tag = registry.get<TagComponent>(entity);
                if (HasTagPrefix(tag, "SC_Wave2_"))
                    return 1;
                if (HasTagPrefix(tag, "SC_Wave1_"))
                    return 0;
            }

            return 0;
        }

        static void SetSpriteAlpha(Scene* scene, const std::string& name, float alpha)
        {
            if (!scene)
                return;

            Entity entity = scene->GetEntityByName(name);
            if (entity && entity.HasComponent<SpriteRendererComponent>())
                entity.GetComponent<SpriteRendererComponent>().Color.a = alpha;
        }

        static void DestroyNamedEntity(Scene* scene, const std::string& name)
        {
            if (!scene)
                return;

            Entity entity = scene->GetEntityByName(name);
            if (entity)
                scene->DestroyEntity(entity);
        }

        static void QueueEnemyRemoval(Scene* scene, entt::entity entity)
        {
            if (!scene)
                return;

            auto& registry = scene->GetRegistry();
            if (registry.all_of<SpriteRendererComponent>(entity))
                registry.get<SpriteRendererComponent>(entity).Color.a = 0.0f;

            if (registry.all_of<TagComponent>(entity))
            {
                const std::string tag = registry.get<TagComponent>(entity).Tag;
                SetSpriteAlpha(scene, tag + "_Shadow", 0.0f);
                SetSpriteAlpha(scene, tag + "_HPBack", 0.0f);
                SetSpriteAlpha(scene, tag + "_HPFill", 0.0f);
                DestroyNamedEntity(scene, tag + "_Shadow");
                DestroyNamedEntity(scene, tag + "_HPBack");
                DestroyNamedEntity(scene, tag + "_HPFill");
            }

            scene->DestroyEntity({ entity, scene });
        }

        static bool ShouldRemoveEnemyAfterDeath(const SideEnemyAIComponent* ai)
        {
            return ai && ai->Kind != SideEnemyKind::BearBoss;
        }

        static void ApplyWaveActivation(Scene* scene, SideCombatLevelComponent& level)
        {
            if (!scene || !level.WaveModeEnabled)
                return;

            const auto& tuning = SideCombatTuningService::GetTuning(level);
            auto& registry = scene->GetRegistry();
            for (auto e : registry.view<SideCombatantComponent, SideEnemyAIComponent>())
            {
                auto& combatant = registry.get<SideCombatantComponent>(e);
                auto& ai = registry.get<SideEnemyAIComponent>(e);
                if (combatant.Team != (int)SideCombatTeam::Enemy)
                    continue;

                const bool activeWave = combatant.Alive &&
                    GetEnemyWaveIndex(scene, level, e, ai) == level.RuntimeWaveIndex;
                ai.RuntimeAwake = activeWave;
                combatant.Invulnerable = !activeWave && combatant.Alive;
                if (activeWave)
                {
                    ai.RuntimeAttackTimer = std::max(ai.RuntimeAttackTimer, tuning.Enemy.InitialAttackDelay);
                }
                else
                {
                    combatant.RuntimeVelocity = { 0.0f, 0.0f };
                }

                if (registry.all_of<SpriteRendererComponent>(e))
                    registry.get<SpriteRendererComponent>(e).Color.a = activeWave ? 1.0f : 0.0f;

                if (registry.all_of<TagComponent>(e))
                {
                    const std::string shadowName = registry.get<TagComponent>(e).Tag + "_Shadow";
                    SetSpriteAlpha(scene, shadowName, activeWave ? 0.38f : 0.0f);
                }
            }
        }

        static void AdvanceWave(Scene* scene, SideCombatLevelComponent& level)
        {
            level.RuntimeWaveIndex = std::min(level.RuntimeWaveIndex + 1, GetWaveCount(level) - 1);
            level.RuntimeWaveRightWall = std::clamp(
                GetWaveRightWall(level, level.RuntimeWaveIndex),
                level.ArenaMin.x,
                level.ArenaMax.x);
            level.RuntimeComboCount = 0;
            level.RuntimeComboTimer = 0.0f;
            ApplyWaveActivation(scene, level);
        }

        static std::unordered_map<std::string, int> BuildResultRewardAmounts(
            const SideCombatLevelComponent& level)
        {
            std::unordered_map<std::string, int> amounts;
            for (const auto& reward : level.DeathRewards)
            {
                if (!reward.Enabled ||
                    reward.ItemId.empty() ||
                    reward.Amount <= 0 ||
                    reward.EnemyKind != static_cast<int>(SideEnemyKind::BearBoss))
                {
                    continue;
                }

                amounts[reward.ItemId] += reward.Amount;
            }
            return amounts;
        }

    } // namespace

    void UpdateDeathsAndVictory(Scene* scene,
        SideCombatLevelComponent& level,
        Entity player)
    {
        if (!scene)
            return;

        bool anyAliveEnemy = false;
        bool anyDeathPending = false;
        std::vector<entt::entity> enemiesToRemove;
        auto& registry = scene->GetRegistry();

        for (auto e : registry.view<TransformComponent, SideCombatantComponent>())
        {
            auto& transform = registry.get<TransformComponent>(e);
            auto& combatant = registry.get<SideCombatantComponent>(e);
            if (combatant.Team != (int)SideCombatTeam::Enemy)
                continue;

            if (combatant.Alive)
            {
                if (!level.WaveModeEnabled)
                {
                    anyAliveEnemy = true;
                }
                else if (registry.all_of<SideEnemyAIComponent>(e) &&
                    GetEnemyWaveIndex(scene, level, e, registry.get<SideEnemyAIComponent>(e)) == level.RuntimeWaveIndex)
                {
                    anyAliveEnemy = true;
                }
                continue;
            }

            const SideEnemyAIComponent* ai = registry.all_of<SideEnemyAIComponent>(e)
                ? &registry.get<SideEnemyAIComponent>(e)
                : nullptr;
            const bool removeAfterDeath = ShouldRemoveEnemyAfterDeath(ai);
            if (removeAfterDeath)
            {
                combatant.RuntimeRemoveAfterDeath = true;
                if (combatant.RuntimeDeathTimer >= GruntDeathRemoveDelay)
                {
                    enemiesToRemove.push_back(e);
                    continue;
                }

                if (!level.WaveModeEnabled ||
                    (ai && GetEnemyWaveIndex(scene, level, e, *ai) == level.RuntimeWaveIndex))
                {
                    anyDeathPending = true;
                }
            }

            if (combatant.RuntimeDeathProcessed)
                continue;

            combatant.RuntimeDeathProcessed = true;
            combatant.RuntimeVelocity = { 0.0f, 0.0f };
            const std::string sourceName = registry.all_of<TagComponent>(e)
                ? registry.get<TagComponent>(e).Tag
                : std::string{};
            SideCombatPickupService::SpawnDeathRewards(scene, level, sourceName, transform, ai);

            if (!removeAfterDeath && registry.all_of<SpriteRendererComponent>(e))
                registry.get<SpriteRendererComponent>(e).Color.a = 0.18f;
        }

        for (entt::entity enemy : enemiesToRemove)
            QueueEnemyRemoval(scene, enemy);

        if (!anyAliveEnemy && anyDeathPending && !level.RuntimeVictory && !level.RuntimeDefeat)
            return;

        if (!anyAliveEnemy &&
            level.WaveModeEnabled &&
            level.RuntimeWaveIndex + 1 < GetWaveCount(level) &&
            !level.RuntimeVictory &&
            !level.RuntimeDefeat)
        {
            AdvanceWave(scene, level);
            return;
        }

        if (!anyAliveEnemy && !level.RuntimeVictory && !level.RuntimeDefeat)
        {
            level.RuntimeVictory = true;
            level.RuntimeResultTimer = 0.0f;
            level.RuntimeResultCommandIssued = false;
            level.RuntimeRewardsSpawned = true;
            SideCombatResultService::RefreshResult(level);
            level.RuntimeResultFirstClear = GameProgress::RecordDungeonClear(
                level.LevelId,
                level.RuntimeBestCombo,
                level.RuntimeResultExperience,
                level.RuntimeResultRepeatExperience);
            SideCombatResultService::RefreshResult(level);
            GameProgress::RecordLastDungeonResult(
                level.LevelId,
                level.RuntimeResultGrade,
                level.RuntimeResultFirstClear,
                level.RuntimeBestCombo,
                level.RuntimePlayerHitsTaken,
                level.RuntimeElapsed,
                level.RuntimeResultFirstClear ? level.RuntimeResultExperience : level.RuntimeResultRepeatExperience,
                level.FirstClearRewardText,
                BuildResultRewardAmounts(level));
            if (player && player.HasComponent<SideCombatantComponent>())
                player.GetComponent<SideCombatantComponent>().ControlsLocked = true;
        }
    }

    void UpdateResultTransition(Scene* scene,
        SideCombatLevelComponent& level,
        float dt)
    {
        if (!level.RuntimeVictory && !level.RuntimeDefeat)
            return;

        level.RuntimeResultTimer += dt;
        const bool victory = level.RuntimeVictory;
        const std::string& command = victory ? level.VictorySceneCommand : level.DefeatSceneCommand;
        if (command.empty())
            return;

        const float delay = std::max(0.0f, victory ? level.VictoryReturnDelay : level.DefeatReturnDelay);
        const float fadeDuration = std::max(0.01f, level.ResultSceneFadeDuration);
        const float fadeStart = std::max(0.0f, delay - fadeDuration);
        const float fade = std::clamp((level.RuntimeResultTimer - fadeStart) / fadeDuration, 0.0f, 1.0f);
        UIRuntimeTools::SetImageAlpha(scene, level.FadeEntityName, fade);

        GameplayFlowService::TryIssueDelayedCommand(level.RuntimeResultTimer,
            delay,
            level.RuntimeResultCommandIssued,
            level.RuntimeRequestedCommand,
            command);
    }

} // namespace Wheatear::SideCombatOutcomeService
