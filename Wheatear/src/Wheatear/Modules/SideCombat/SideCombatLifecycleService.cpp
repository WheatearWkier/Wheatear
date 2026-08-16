#include "wtpch.h"
#include "SideCombatLifecycleService.h"

#include "SideCombatTuningService.h"
#include "SideCombatVisualService.h"
#include "Wheatear/Gameplay/Services/GameplayVisualService.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <cstdlib>

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

    namespace {

        // Maps a SideEnemyKind onto the enemyTypes: tuning key. The boss is
        // not part of the wave table (it lives on the scene via
        // BossEntityName and follows the classic bear-boss flow).
        static const char* EnemyKindId(SideEnemyKind kind)
        {
            switch (kind)
            {
            case SideEnemyKind::Thrower: return "thrower";
            case SideEnemyKind::Pouncer: return "pouncer";
            case SideEnemyKind::BearBoss: return "bear_boss";
            default: return "grunt";
            }
        }

        static std::string EnemyIdleTexture(const SideCombatTuningService::SideCombatTuning& tuning)
        {
            auto it = tuning.GruntAnimations.Clips.find("idle");
            if (it != tuning.GruntAnimations.Clips.end() && !it->second.Atlas.SheetPath.empty())
                return it->second.Atlas.SheetPath;
            return "assets/vertical_slice/side_combat/sheets/runtime_enemies/en_claw_beast_idle_sheet.png";
        }

    } // namespace

    void SpawnWavesFromTable(Scene* scene, SideCombatLevelComponent& level)
    {
        if (!scene)
            return;
        if (level.WaveSpawns.empty())
        {
            WT_CORE_INFO("SideCombat: wave table empty for level '{}'; keeping scene-placed enemies.",
                level.LevelId);
            return;
        }

        const auto& tuning = SideCombatTuningService::GetTuning(level);
        auto& registry = scene->GetRegistry();

        // Remove statically placed enemies (except the boss) so the wave
        // table is the single source of enemy composition.
        Entity bossEntity = scene->GetEntityByName(level.BossEntityName);
        std::vector<entt::entity> staleEnemies;
        for (auto e : registry.view<SideCombatantComponent, SideEnemyAIComponent>())
        {
            if (registry.get<SideCombatantComponent>(e).Team != (int)SideCombatTeam::Enemy)
                continue;
            if (bossEntity && e == static_cast<entt::entity>(bossEntity))
                continue;
            staleEnemies.push_back(e);
        }
        for (entt::entity e : staleEnemies)
            scene->DestroyEntityImmediate(Entity{ e, scene });

        const std::string idleTexture = EnemyIdleTexture(tuning);
        int spawnCounter = 0;
        for (const auto& spawn : level.WaveSpawns)
        {
            if (!spawn.Enabled || spawn.Count <= 0)
                continue;
            const SideEnemyKind kind = static_cast<SideEnemyKind>(
                std::clamp(spawn.EnemyKind, 0, static_cast<int>(SideEnemyKind::BearBoss)));
            if (kind == SideEnemyKind::BearBoss)
                continue; // boss flow stays scene-driven

            const auto& type = [&]() -> const SideCombatTuningService::EnemyTypeDefinition&
            {
                auto it = tuning.EnemyTypes.find(EnemyKindId(kind));
                if (it != tuning.EnemyTypes.end())
                    return it->second;
                static const SideCombatTuningService::EnemyTypeDefinition kDefault;
                return kDefault;
            }();

            const float minX = std::min(spawn.SpawnMinX, spawn.SpawnMaxX);
            const float maxX = std::max(spawn.SpawnMinX, spawn.SpawnMaxX);
            const float groundY = level.GroundY + spawn.GroundYOffset;
            const float hpJitter = std::clamp(spawn.HpVariance, 0.0f, 1.0f);

            for (int i = 0; i < spawn.Count; ++i)
            {
                const float t = spawn.Count > 1
                    ? static_cast<float>(i) / static_cast<float>(spawn.Count - 1)
                    : 0.5f;
                const float x = minX + (maxX - minX) * t;
                const std::string tag = "SC_Enemy_W" + std::to_string(spawn.WaveIndex)
                    + "_K" + std::to_string(spawn.EnemyKind)
                    + "_" + std::to_string(i);

                Entity enemy = scene->CreateEntity(tag);
                // CreateEntity already provides ID/Transform/Tag; only the
                // combat components are added here.
                auto& transform = enemy.GetComponent<TransformComponent>();
                transform.Translation = { x, groundY, -0.04f };
                transform.Scale = type.RenderScale;

                auto& sprite = enemy.AddComponent<SpriteRendererComponent>();
                sprite.Texture = GameplayVisualService::LoadTextureCached(idleTexture);
                sprite.UVMax = { 0.25f, 1.0f };

                // Optional HP jitter so same-type spawns vary a little.
                float maxHealth = type.MaxHealth;
                if (hpJitter > 0.0f)
                {
                    const float jitter = (std::rand() / static_cast<float>(RAND_MAX) * 2.0f - 1.0f) * hpJitter;
                    maxHealth = std::max(1.0f, maxHealth * (1.0f + jitter));
                }

                auto& combatant = enemy.AddComponent<SideCombatantComponent>();
                combatant.Team = (int)SideCombatTeam::Enemy;
                combatant.MaxHealth = maxHealth;
                combatant.Health = maxHealth;
                combatant.Attack = type.Attack;
                combatant.Defense = type.Defense;
                combatant.MoveSpeed = type.MoveSpeed;
                combatant.CollisionSize = type.CollisionSize;
                combatant.CollisionHeight = type.CollisionHeight;
                combatant.KnockbackResistance = type.KnockbackResistance;

                auto& ai = enemy.AddComponent<SideEnemyAIComponent>();
                ai.Kind = kind;
                ai.WaveIndex = std::clamp(spawn.WaveIndex, 0, GetWaveCount(level) - 1);
                ai.AggroRange = type.AggroRange;
                ai.AttackRange = type.AttackRange;
                ai.PreferredRange = type.PreferredRange;
                ai.AttackInterval = type.AttackInterval;
                ai.LaneTolerance = type.LaneTolerance;
                ai.PatrolMinX = minX;
                ai.PatrolMaxX = maxX;

                // Companion shadow entity; the visual service syncs it via the
                // "{tag}_Shadow" convention.
                Entity shadow = scene->CreateEntity(tag + "_Shadow");
                auto& shadowTransform = shadow.GetComponent<TransformComponent>();
                shadowTransform.Translation = { x, groundY, -0.07f };
                shadowTransform.Scale = type.ShadowScale;
                auto& shadowSprite = shadow.AddComponent<SpriteRendererComponent>();
                shadowSprite.Texture = GameplayVisualService::LoadTextureCached(
                    "assets/vertical_slice/side_combat/ui/blob_shadow_soft.png");
                shadowSprite.Color = { 0.0f, 0.0f, 0.0f, 0.54f };

                ++spawnCounter;
            }
        }

        if (spawnCounter > 0)
        {
            WT_CORE_INFO("SideCombat: spawned {} enemy(ies) from the wave table for level '{}'",
                spawnCounter, level.LevelId);
        }
    }

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
        level.RuntimeCinematicTimer = 0.0f;
        level.RuntimeCinematicDuration = 0.0f;
        level.RuntimeCinematicTimeScale = 1.0f;
        level.RuntimeCinematicCameraZoom = 1.0f;
        level.RuntimeCinematicCameraOffset = { 0.0f, 0.0f };
        level.RuntimeCinematicFocusEntity = 0;
        level.RuntimeCameraShakeTimer = 0.0f;
        level.RuntimeCameraShakeDuration = 0.0f;
        level.RuntimeCameraShakeStrength = 0.0f;
        level.RuntimeCameraBaseTranslation = { 0.0f, 0.0f, 0.0f };
        level.RuntimeCameraBaseOrthographicSize = 0.0f;
        level.RuntimeCameraBaseCaptured = false;
        level.RuntimeCameraProjectionCaptured = false;
        level.RuntimeWaveIndex = 0;
        // Normalize arena bounds first: std::clamp requires lo <= hi, and a
        // hand-edited scene may have ArenaMin.x > ArenaMax.x (UB otherwise).
        const float arenaLo = std::min(level.ArenaMin.x, level.ArenaMax.x);
        const float arenaHi = std::max(level.ArenaMin.x, level.ArenaMax.x);
        level.RuntimeWaveRightWall = level.WaveModeEnabled
            ? std::clamp(GetWaveRightWall(level, 0), arenaLo, arenaHi)
            : level.ArenaMax.x;
        level.RuntimeHudLayoutConfigured = false;

        UIRuntimeTools::SetImageAlpha(scene, level.FadeEntityName, 1.0f);
    }

    void ResetCombatants(Scene* scene, SideCombatLevelComponent& level)
    {
        if (!scene)
            return;

        const auto& tuning = SideCombatTuningService::GetTuning(level);
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
            combatant.RuntimeVisualActionSequence = 0;

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
            controller.RuntimeDashCooldown = 0.0f;
            controller.RuntimeHealItemCooldown = 0.0f;
            controller.RuntimeManaItemCooldown = 0.0f;
            controller.RuntimeAttackBuffItemCooldown = 0.0f;
            controller.RuntimeBreakLimitCooldown = 0.0f;
            controller.RuntimeManaMax = std::max(1.0f, controller.MaxMana);
            controller.RuntimeMana = controller.RuntimeManaMax;
            controller.RuntimeMagicSwordGaugeMax = std::max(1.0f, tuning.AirCombo.MagicSwordGaugeMax);
            controller.RuntimeMagicSwordGauge = 0.0f;
            controller.RuntimeAttackBuffTimer = 0.0f;
            controller.RuntimeAttackBuffMultiplier = 1.0f;
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
            controller.RuntimeActionSequence = 0;
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
            ai.RuntimeActionSequence = 0;
            ai.RuntimeLastActionAttackId.clear();
        }

        ApplyWaveActivation(scene, level);
    }

} // namespace Wheatear::SideCombatLifecycleService
