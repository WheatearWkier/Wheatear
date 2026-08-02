#include "wtpch.h"
#include "SideCombatLifecycleService.h"

#include "SideCombatTuningService.h"
#include "SideCombatVisualService.h"
#include "Wheatear/Modules/Common/GameplayVisualService.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <array>

namespace Wheatear::SideCombatLifecycleService {

    namespace {

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

        static void CreateWaveShadow(Scene* scene,
            const std::string& name,
            const glm::vec2& position,
            const glm::vec2& scale)
        {
            if (!scene || scene->GetEntityByName(name))
                return;

            Entity shadow = scene->CreateEntity(name);
            auto& transform = shadow.GetComponent<TransformComponent>();
            transform.Translation = { position.x, position.y - 0.02f, -0.07f };
            transform.Scale = { scale.x, scale.y, 1.0f };

            auto& sprite = shadow.AddComponent<SpriteRendererComponent>();
            sprite.Color = { 0.0f, 0.0f, 0.0f, 0.0f };
            if (Ref<Texture2D> texture = GameplayVisualService::LoadTextureCached(
                "assets/vertical_slice/side_combat/ui/blob_shadow_soft.png"))
            {
                sprite.Texture = texture;
            }
        }

        static void CreateWaveGrunt(Scene* scene,
            const std::string& name,
            const glm::vec2& position,
            float health,
            float attack)
        {
            if (!scene || scene->GetEntityByName(name))
                return;

            Entity enemy = scene->CreateEntity(name);
            auto& transform = enemy.GetComponent<TransformComponent>();
            transform.Translation = { position.x, position.y, -0.04f };
            transform.Scale = { 1.72f, 1.72f, 1.0f };

            auto& sprite = enemy.AddComponent<SpriteRendererComponent>();
            sprite.Color = { 1.0f, 1.0f, 1.0f, 0.0f };
            if (Ref<Texture2D> texture = GameplayVisualService::LoadTextureCached(
                "assets/vertical_slice/side_combat/enemies/en_claw_beast_idle_01.png"))
            {
                sprite.Texture = texture;
            }

            auto& combatant = enemy.AddComponent<SideCombatantComponent>();
            combatant.Team = (int)SideCombatTeam::Enemy;
            combatant.MaxHealth = health;
            combatant.Health = health;
            combatant.Attack = attack;
            combatant.Defense = 8.0f;
            combatant.MoveSpeed = 3.9f;
            combatant.CollisionSize = { 0.86f, 0.48f };
            combatant.CollisionHeight = 1.05f;
            combatant.KnockbackResistance = 0.04f;

            auto& ai = enemy.AddComponent<SideEnemyAIComponent>();
            ai.Kind = SideEnemyKind::Grunt;
            ai.AggroRange = 8.0f;
            ai.AttackRange = 1.18f;
            ai.PreferredRange = 0.95f;
            ai.AttackInterval = 1.25f;
            ai.PatrolMinX = position.x - 1.6f;
            ai.PatrolMaxX = position.x + 1.6f;
            ai.LaneTolerance = 0.42f;

            CreateWaveShadow(scene, name + "_Shadow", position, { 1.05f, 0.34f });
        }

        static void EnsureWaveEnemies(Scene* scene, SideCombatLevelComponent& level)
        {
            if (!scene || !level.WaveModeEnabled || level.RuntimeWaveSpawnsCreated)
                return;

            CreateWaveGrunt(scene, "SC_Wave1_Claw_A", { -3.95f, -2.64f }, 260.0f, 28.0f);
            CreateWaveGrunt(scene, "SC_Wave1_Claw_B", { -2.55f, -2.18f }, 245.0f, 27.0f);
            CreateWaveGrunt(scene, "SC_Wave2_Claw_A", { 0.30f, -2.62f }, 290.0f, 31.0f);
            CreateWaveGrunt(scene, "SC_Wave2_Claw_B", { 1.65f, -2.08f }, 275.0f, 30.0f);
            CreateWaveGrunt(scene, "SC_Wave2_Claw_C", { 2.75f, -2.78f }, 300.0f, 32.0f);
            level.RuntimeWaveSpawnsCreated = true;
        }

        static void ApplyWaveActivation(Scene* scene,
            const SideCombatLevelComponent& level)
        {
            if (!scene || !level.WaveModeEnabled)
                return;

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
                if (!activeWave)
                    combatant.RuntimeVelocity = { 0.0f, 0.0f };

                if (registry.all_of<SpriteRendererComponent>(e))
                    registry.get<SpriteRendererComponent>(e).Color.a = activeWave ? 1.0f : 0.0f;

                if (registry.all_of<TagComponent>(e))
                {
                    const std::string shadowName = registry.get<TagComponent>(e).Tag + "_Shadow";
                    SetSpriteAlpha(scene, shadowName, activeWave ? 0.38f : 0.0f);
                }
            }
        }

    } // namespace

    void ResetLevelRuntime(Scene* scene, SideCombatLevelComponent& level)
    {
        const auto& tuning = SideCombatTuningService::GetTuning(level);
        level.ComboDropDelay = std::max(0.0f, tuning.Combat.ComboDropDelay);
        level.RuntimeElapsed = 0.0f;
        level.RuntimeFadeAlpha = 1.0f;
        level.RuntimePaused = false;
        level.RuntimeVictory = false;
        level.RuntimeDefeat = false;
        level.RuntimeResultTimer = 0.0f;
        level.RuntimeResultCommandIssued = false;
        level.RuntimeRequestedCommand.clear();
        level.RuntimeComboCount = 0;
        level.RuntimeBestCombo = 0;
        level.RuntimeComboTimer = 0.0f;
        level.RuntimeCollectedPickups = 0;
        level.RuntimeRewardsSpawned = false;
        level.RuntimePlayerHitsTaken = 0;
        level.RuntimeResultExperience = 0;
        level.RuntimeResultRepeatExperience = 0;
        level.RuntimeResultFirstClear = false;
        level.RuntimePlayerEntity = 0;
        level.RuntimeBossEntity = 0;
        level.RuntimeResultGrade.clear();
        level.RuntimeResultSummary.clear();
        level.RuntimeHitPauseTimer = 0.0f;
        level.RuntimeCameraShakeTimer = 0.0f;
        level.RuntimeCameraShakeDuration = 0.0f;
        level.RuntimeCameraShakeStrength = 0.0f;
        level.RuntimeCameraBaseTranslation = { 0.0f, 0.0f, 0.0f };
        level.RuntimeCameraBaseCaptured = false;
        level.RuntimeWaveIndex = 0;
        level.RuntimeWaveRightWall = level.WaveModeEnabled
            ? std::clamp(GetWaveRightWall(level, 0), level.ArenaMin.x, level.ArenaMax.x)
            : level.ArenaMax.x;
        level.RuntimeWaveSpawnsCreated = false;

        UIRuntimeTools::SetImageAlpha(scene, level.FadeEntityName, 1.0f);
    }

    void ResetCombatants(Scene* scene, SideCombatLevelComponent& level)
    {
        if (!scene)
            return;

        const auto& tuning = SideCombatTuningService::GetTuning(level);
        EnsureWaveEnemies(scene, level);
        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<TransformComponent, SideCombatantComponent>())
        {
            auto& transform = registry.get<TransformComponent>(e);
            auto& combatant = registry.get<SideCombatantComponent>(e);
            if (combatant.Team == (int)SideCombatTeam::Player)
            {
                const auto& progress = GameProgress::GetState();
                combatant.MaxHealth = std::max(combatant.MaxHealth, static_cast<float>(progress.Attributes.HP));
                combatant.Attack = std::max(combatant.Attack, static_cast<float>(progress.Attributes.ATK));
                combatant.Defense = std::max(combatant.Defense, static_cast<float>(progress.Attributes.DEF));
            }

            combatant.Health = std::max(0.0f, combatant.MaxHealth);
            combatant.Alive = combatant.Health > 0.0f;
            combatant.ControlsLocked = false;
            combatant.RuntimeVelocity = { 0.0f, 0.0f };
            combatant.RuntimeGroundPosition = { transform.Translation.x, transform.Translation.y };
            combatant.RuntimeAirHeight = 0.0f;
            combatant.RuntimeAirVelocity = 0.0f;
            combatant.RuntimeHitStun = 0.0f;
            combatant.RuntimeInvulnerableTimer = 0.0f;
            combatant.RuntimeProtection = 0.0f;
            combatant.RuntimeProtectionMax = registry.all_of<SideEnemyAIComponent>(e) &&
                registry.get<SideEnemyAIComponent>(e).Kind == SideEnemyKind::BearBoss
                ? std::max(1.0f, tuning.Protection.BossProtectionMax)
                : 100.0f;
            combatant.RuntimeState = combatant.Alive ? SideCombatState::Normal : SideCombatState::Dead;
            combatant.RuntimeStateTimer = 0.0f;
            combatant.RuntimeDeathProcessed = false;
            combatant.RuntimeRemoveAfterDeath = false;
            combatant.RuntimeDeathTimer = 0.0f;
            combatant.RuntimeOnGround = true;
            combatant.RuntimeVisualClipKey.clear();
            combatant.RuntimeVisualTimer = 0.0f;

            if (registry.all_of<SidePlayerControllerComponent>(e))
            {
                auto& controller = registry.get<SidePlayerControllerComponent>(e);
                SideCombatTuningService::ApplyPlayerTuning(tuning, combatant, controller);
            }

            if (registry.all_of<SideEnemyAIComponent>(e))
            {
                auto& ai = registry.get<SideEnemyAIComponent>(e);
                SideCombatTuningService::ApplyBearBossTuning(tuning, combatant, ai);
            }

            if (combatant.Team == (int)SideCombatTeam::Enemy && transform.Translation.x != 0.0f)
                combatant.RuntimeFacing = transform.Translation.x > 0.0f ? -1.0f : 1.0f;

            if (registry.all_of<SpriteRendererComponent>(e))
                registry.get<SpriteRendererComponent>(e).Color.a = 1.0f;
            SideCombatVisualService::UpdateCombatantVisual(scene, { e, scene }, level, tuning, 0.0f);
        }

        for (auto e : registry.view<SidePlayerControllerComponent>())
        {
            auto& controller = registry.get<SidePlayerControllerComponent>(e);
            controller.RuntimeBasicCooldown = 0.0f;
            controller.RuntimeLauncherCooldown = 0.0f;
            controller.RuntimeMagicBoltCooldown = 0.0f;
            controller.RuntimeAllySupportCooldown = 0.0f;
            controller.RuntimeBreakLimitCooldown = 0.0f;
            controller.RuntimeMagicSwordGaugeMax = std::max(1.0f, tuning.AirCombo.MagicSwordGaugeMax);
            controller.RuntimeMagicSwordGauge = controller.RuntimeMagicSwordGaugeMax;
            controller.RuntimeJumpsRemaining = controller.MaxJumps;
            controller.RuntimeJumpBufferTimer = 0.0f;
            controller.RuntimeCoyoteTimer = 0.0f;
            controller.RuntimeAttackChain = 0;
            controller.RuntimeAttackChainTimer = 0.0f;
            controller.RuntimeAirActionsRemaining = tuning.AirCombo.AirActionLimit;
            controller.RuntimeActionAttackId.clear();
            controller.RuntimeActionEntityName.clear();
            controller.RuntimeActionKind = SideAttackKind::Basic;
            controller.RuntimeActionTimer = 0.0f;
            controller.RuntimeActionDuration = 0.0f;
            controller.RuntimeActionHitboxTime = 0.0f;
            controller.RuntimeActionCancelStart = 0.0f;
            controller.RuntimeActionCancelEnd = 0.0f;
            controller.RuntimeActionMovementScale = 1.0f;
            controller.RuntimeActionHitboxSpawned = false;
        }

        for (auto e : registry.view<SideEnemyAIComponent>())
        {
            auto& ai = registry.get<SideEnemyAIComponent>(e);
            ai.RuntimeAttackTimer = tuning.Enemy.InitialAttackDelay;
            ai.RuntimeDecisionTimer = 0.0f;
            ai.RuntimeAwake = true;
            ai.RuntimeActionAttackId.clear();
            ai.RuntimeActionEntityName.clear();
            ai.RuntimeActionKind = SideAttackKind::EnemyMelee;
            ai.RuntimeActionTimer = 0.0f;
            ai.RuntimeActionDuration = 0.0f;
            ai.RuntimeActionHitboxTime = 0.0f;
            ai.RuntimeActionMovementScale = 1.0f;
            ai.RuntimeActionFacing = 1.0f;
            ai.RuntimeActionHitboxSpawned = false;
        }

        ApplyWaveActivation(scene, level);
    }

} // namespace Wheatear::SideCombatLifecycleService
