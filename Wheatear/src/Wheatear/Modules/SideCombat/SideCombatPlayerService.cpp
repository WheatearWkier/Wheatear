#include "wtpch.h"
#include "SideCombatPlayerService.h"

#include "SideCombatActionCatalog.h"
#include "SideCombatActionService.h"
#include "SideCombatFeedbackService.h"
#include "SideCombatHitboxService.h"
#include "SideCombatHitResolutionService.h"
#include "SideCombatMath.h"
#include "SideCombatTargetService.h"
#include "SideCombatTuningService.h"
#include "Wheatear/Gameplay/Action/ActionDatabase.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

#include <algorithm>
#include <string>

namespace Wheatear::SideCombatPlayerService {

    namespace {

        using SideCombatActionService::BeginPlayerAction;
        using SideCombatActionService::CanStartPlayerAction;
        using SideCombatActionService::ClearPlayerAction;
        using SideCombatActionService::GetPlayerActionMovementScale;
        using SideCombatActionService::IsPlayerActionActive;
        using SideCombatHitboxService::CreateHitbox;
        using SideCombatHitResolutionService::IsBossEntity;
        using SideCombatHitResolutionService::IsControlledAirborne;
        using SideCombatTargetService::FindNearestAliveEnemy;
        using SideCombatTuningService::ApplyPlayerTuning;
        using SideCombatTuningService::GetAttack;
        using SideCombatTuningService::GetTuning;
        using SideCombatTuningService::IsBreakLimitDebugAvailable;
        using SideCombatTuningService::IsBreakLimitOfficiallyAvailable;
        using SideCombatTuningService::IsSkillUnlocked;

        static void ApplyAuthoringRecipeFields(WAO::ActionRecipe& recipe,
            const WAO::ActionRecipe* authored)
        {
            if (!authored)
                return;

            if (!authored->DisplayName.empty())
                recipe.DisplayName = authored->DisplayName;
            if (!authored->Description.empty())
                recipe.Description = authored->Description;
            if (!authored->IconPath.empty())
                recipe.IconPath = authored->IconPath;
            if (!authored->AnimationId.empty())
                recipe.AnimationId = authored->AnimationId;
            if (!authored->SoundPath.empty())
                recipe.SoundPath = authored->SoundPath;
            if (!authored->EffectPath.empty())
                recipe.EffectPath = authored->EffectPath;
            if (!authored->Tags.empty())
                recipe.Tags = authored->Tags;
            if (!authored->Signals.empty())
                recipe.Signals = authored->Signals;
            if (!authored->ResourceCost.empty())
                recipe.ResourceCost = authored->ResourceCost;
        }

        static WAO::ActionRecipe RegisterRuntimeRecipe(const std::string& attackId,
            const SideCombatTuningService::SideAttackTuning& attack,
            SideAttackKind kind,
            const std::string& displayName,
            const std::string& description,
            float cooldown,
            float resourceCost = 0.0f)
        {
            const std::string recipeId = SideCombatActionCatalog::ActionRecipeId(attackId);
            const WAO::ActionRecipe* authored = WAO::ActionDatabase::Find(recipeId);
            WAO::ActionRecipe recipe = SideCombatActionCatalog::BuildActionRecipe(attackId,
                attack,
                kind,
                displayName,
                description,
                cooldown,
                resourceCost);
            ApplyAuthoringRecipeFields(recipe, authored);
            WAO::ActionDatabase::Register(recipe);
            return recipe;
        }

        static float ResourceCost(const WAO::ActionRecipe& recipe,
            const std::string& id,
            float fallback)
        {
            const auto it = recipe.ResourceCost.find(id);
            return it != recipe.ResourceCost.end() ? it->second : fallback;
        }

        static void UpdatePlayerAction(Scene* scene,
            SideCombatLevelComponent& level,
            Entity player,
            SideCombatantComponent& combatant,
            SidePlayerControllerComponent& controller,
            float dt)
        {
            if (!IsPlayerActionActive(controller))
            {
                ClearPlayerAction(controller);
                return;
            }

            const auto& tuning = GetTuning(level);
            const std::string attackId = controller.RuntimeActionAttackId;
            const auto& attack = GetAttack(tuning, attackId);
            controller.RuntimeActionTimer += dt;

            if (!controller.RuntimeActionHitboxSpawned &&
                controller.RuntimeActionTimer >= controller.RuntimeActionHitboxTime)
            {
                glm::vec2 origin = combatant.RuntimeGroundPosition;
                float sourceAirHeight = combatant.RuntimeAirHeight;
                float facing = combatant.RuntimeFacing;

                if (controller.RuntimeActionKind == SideAttackKind::AllySupport ||
                    controller.RuntimeActionKind == SideAttackKind::BreakLimit)
                {
                    Entity target = FindNearestAliveEnemy(scene, player.GetComponent<TransformComponent>().Translation);
                    if (target && target.HasComponent<SideCombatantComponent>())
                    {
                        const auto& targetCombatant = target.GetComponent<SideCombatantComponent>();
                        origin = targetCombatant.RuntimeGroundPosition;
                        sourceAirHeight = targetCombatant.RuntimeAirHeight;
                        if (controller.RuntimeActionKind == SideAttackKind::BreakLimit)
                        {
                            facing = SideCombatMath::SignNonZero(origin.x - combatant.RuntimeGroundPosition.x);
                            combatant.RuntimeFacing = facing;
                        }
                    }
                    else if (controller.RuntimeActionKind == SideAttackKind::AllySupport)
                    {
                        origin += glm::vec2{ combatant.RuntimeFacing * 2.0f, 0.0f };
                    }
                }

                CreateHitbox(scene,
                    controller.RuntimeActionEntityName.empty() ? "Side_PlayerAction" : controller.RuntimeActionEntityName,
                    static_cast<entt::entity>(player),
                    origin,
                    sourceAirHeight,
                    facing,
                    controller.RuntimeActionKind,
                    controller.RuntimeActionRecipeId,
                    (int)SideCombatTeam::Player,
                    attack,
                    combatant.Attack * attack.DamageScale + attack.DamageFlat,
                    tuning);
                controller.RuntimeActionHitboxSpawned = true;
            }

            if (controller.RuntimeActionTimer >= controller.RuntimeActionDuration)
                ClearPlayerAction(controller);
        }

        static void CreatePlayerBasic(Scene* scene,
            SideCombatLevelComponent& level,
            Entity,
            SideCombatantComponent& combatant,
            SidePlayerControllerComponent& controller)
        {
            const auto& tuning = GetTuning(level);
            if (!combatant.RuntimeOnGround)
            {
                if (!IsSkillUnlocked(level, tuning, "air_basic"))
                    return;
                if (controller.RuntimeAirActionsRemaining <= 0)
                    return;

                --controller.RuntimeAirActionsRemaining;
                controller.RuntimeAttackChain = 0;
                controller.RuntimeAttackChainTimer = tuning.Player.BasicChainWindow;
                const auto& attack = GetAttack(tuning, "air_basic");
                const WAO::ActionRecipe recipe = RegisterRuntimeRecipe("air_basic",
                    attack,
                    SideAttackKind::Basic,
                    "Air Slash",
                    "Air combo filler with hang-time support.",
                    tuning.AirCombo.AirBasicCooldown);
                controller.RuntimeBasicCooldown = recipe.Cooldown;
                BeginPlayerAction(controller, attack, "air_basic", recipe.Id, "Side_PlayerAirSlash", SideAttackKind::Basic);
                return;
            }

            if (!IsSkillUnlocked(level, tuning, "basic_attack"))
                return;

            const int chain = controller.RuntimeAttackChainTimer > 0.0f
                ? (controller.RuntimeAttackChain % 3) + 1
                : 1;
            controller.RuntimeAttackChain = chain;
            controller.RuntimeAttackChainTimer = tuning.Player.BasicChainWindow;
            const std::string attackId = "basic" + std::to_string(chain);
            const auto& attack = GetAttack(tuning, attackId);
            const WAO::ActionRecipe recipe = RegisterRuntimeRecipe(attackId,
                attack,
                SideAttackKind::Basic,
                chain == 3 ? "Basic Finisher" : "Basic Slash",
                "Ground basic chain action.",
                chain == 3 ? controller.BasicCooldown + tuning.Player.BasicFinisherExtraCooldown : controller.BasicCooldown);
            controller.RuntimeBasicCooldown = recipe.Cooldown;
            BeginPlayerAction(controller, attack, attackId, recipe.Id, "Side_PlayerSlash", SideAttackKind::Basic);
        }

        static void CreatePlayerLauncher(Scene*,
            SideCombatLevelComponent& level,
            Entity,
            SideCombatantComponent& combatant,
            SidePlayerControllerComponent& controller)
        {
            const auto& tuning = GetTuning(level);
            const bool airborne = !combatant.RuntimeOnGround;
            if (!IsSkillUnlocked(level, tuning, airborne ? "air_chase" : "launcher"))
                return;
            if (airborne && controller.RuntimeAirActionsRemaining <= 0)
                return;

            controller.RuntimeAttackChainTimer = tuning.Player.LauncherChainWindow;

            const std::string attackId = airborne ? "air_chase" : "launcher";
            const auto& attack = GetAttack(tuning, attackId);
            const WAO::ActionRecipe recipe = RegisterRuntimeRecipe(attackId,
                attack,
                SideAttackKind::Launcher,
                airborne ? "Air Chase" : "Launcher",
                airborne ? "Air relaunch tool for combo extension." : "Ground opener that sends targets upward.",
                airborne ? tuning.AirCombo.AirChaseCooldown : controller.LauncherCooldown);
            controller.RuntimeLauncherCooldown = recipe.Cooldown;
            if (airborne)
                --controller.RuntimeAirActionsRemaining;
            else
                controller.RuntimeAirActionsRemaining = tuning.AirCombo.AirActionLimit;

            BeginPlayerAction(controller,
                attack,
                attackId,
                recipe.Id,
                airborne ? "Side_PlayerAirChase" : "Side_PlayerLauncher",
                SideAttackKind::Launcher);
        }

        static bool CanUseBreakLimit(Scene* scene,
            const SideCombatLevelComponent& level,
            const SideCombatTuningService::SideCombatTuning& tuning,
            Entity target,
            const SideCombatantComponent& combatant,
            const SidePlayerControllerComponent& controller)
        {
            if (!scene || !target || !target.HasComponent<SideCombatantComponent>())
                return false;
            if (!IsBreakLimitOfficiallyAvailable(level, tuning) && !IsBreakLimitDebugAvailable(level, tuning))
                return false;
            if (combatant.RuntimeOnGround || controller.RuntimeBreakLimitCooldown > 0.0f)
                return false;
            if (controller.RuntimeMagicSwordGauge + 0.001f < tuning.AirCombo.BreakLimitGaugeCost)
                return false;
            if (level.RuntimeComboCount < tuning.AirCombo.BreakLimitMinCombo)
                return false;
            if (combatant.RuntimeAirHeight > tuning.AirCombo.BreakLimitMaxHeight)
                return false;
            if (combatant.RuntimeAirVelocity > tuning.AirCombo.BreakLimitFallingVelocity)
                return false;

            auto& targetCombatant = target.GetComponent<SideCombatantComponent>();
            if (!targetCombatant.Alive || !IsControlledAirborne(targetCombatant))
                return false;
            if (targetCombatant.RuntimeAirVelocity > tuning.AirCombo.BreakLimitFallingVelocity)
                return false;

            if (IsBossEntity(scene, static_cast<entt::entity>(target)) &&
                targetCombatant.RuntimeProtection < tuning.Protection.BossProtectionBreakLimitThreshold)
            {
                return false;
            }

            return true;
        }

        static void CreateBreakLimitChase(Scene* scene,
            SideCombatLevelComponent& level,
            Entity player,
            SideCombatantComponent& combatant,
            SidePlayerControllerComponent& controller)
        {
            const auto& tuning = GetTuning(level);
            Entity target = FindNearestAliveEnemy(scene, player.GetComponent<TransformComponent>().Translation);
            if (!CanUseBreakLimit(scene, level, tuning, target, combatant, controller))
                return;

            const auto& attack = GetAttack(tuning, "break_limit");
            const WAO::ActionRecipe recipe = RegisterRuntimeRecipe("break_limit",
                attack,
                SideAttackKind::BreakLimit,
                "Break Limit Chase",
                "Advanced reset that extends air combo resources.",
                tuning.AirCombo.BreakLimitCooldown,
                tuning.AirCombo.BreakLimitGaugeCost);
            controller.RuntimeBreakLimitCooldown = recipe.Cooldown;
            controller.RuntimeMagicSwordGauge = std::max(0.0f,
                controller.RuntimeMagicSwordGauge - ResourceCost(recipe, "magic_sword", tuning.AirCombo.BreakLimitGaugeCost));
            controller.RuntimeJumpsRemaining = std::max(controller.RuntimeJumpsRemaining, 1);
            controller.RuntimeAirActionsRemaining = tuning.AirCombo.AirActionLimitAfterBreak;
            controller.RuntimeAttackChainTimer = tuning.Player.LauncherChainWindow;
            combatant.RuntimeAirHeight = std::max(0.05f,
                combatant.RuntimeAirHeight + tuning.AirCombo.BreakLimitHeightBoost);
            combatant.RuntimeAirVelocity = std::max(combatant.RuntimeAirVelocity,
                tuning.AirCombo.BreakLimitHangImpulse);

            if (target && target.HasComponent<SideCombatantComponent>())
            {
                const auto& targetCombatant = target.GetComponent<SideCombatantComponent>();
                combatant.RuntimeFacing = SideCombatMath::SignNonZero(targetCombatant.RuntimeGroundPosition.x - combatant.RuntimeGroundPosition.x);
            }

            BeginPlayerAction(controller, attack, "break_limit", recipe.Id, "Side_BreakLimitChase", SideAttackKind::BreakLimit);
        }

        static void CreatePlayerMagicBolt(Scene*,
            SideCombatLevelComponent& level,
            Entity,
            SideCombatantComponent&,
            SidePlayerControllerComponent& controller)
        {
            const auto& tuning = GetTuning(level);
            if (!IsSkillUnlocked(level, tuning, "magic_bolt"))
                return;

            controller.RuntimeAttackChainTimer = tuning.Player.MagicChainWindow;

            const auto& attack = GetAttack(tuning, "magic_bolt");
            const WAO::ActionRecipe recipe = RegisterRuntimeRecipe("magic_bolt",
                attack,
                SideAttackKind::MagicBolt,
                "Magic Bolt",
                "Ranged magic hit used inside combo routes.",
                controller.MagicBoltCooldown);
            controller.RuntimeMagicBoltCooldown = recipe.Cooldown;
            BeginPlayerAction(controller, attack, "magic_bolt", recipe.Id, "Side_PlayerMagicBolt", SideAttackKind::MagicBolt);
        }

        static void CreateAllySupport(Scene*,
            SideCombatLevelComponent& level,
            Entity,
            SideCombatantComponent&,
            SidePlayerControllerComponent& controller)
        {
            const auto& tuning = GetTuning(level);
            if (!IsSkillUnlocked(level, tuning, "ally_support"))
                return;

            controller.RuntimeAttackChainTimer = tuning.Player.SupportChainWindow;

            const auto& attack = GetAttack(tuning, "ally_support");
            const WAO::ActionRecipe recipe = RegisterRuntimeRecipe("ally_support",
                attack,
                SideAttackKind::AllySupport,
                "Ally Support",
                "Partner assist hit with high control value.",
                controller.AllySupportCooldown);
            controller.RuntimeAllySupportCooldown = recipe.Cooldown;
            BeginPlayerAction(controller, attack, "ally_support", recipe.Id, "Side_AllySupport", SideAttackKind::AllySupport);
        }

    } // namespace

    void UpdatePlayer(Scene* scene,
        SideCombatLevelComponent& level,
        Entity player,
        float dt,
        const PlayerInputState& input,
        const PlayerInputState& previousInput)
    {
        if (!player || !player.HasComponent<TransformComponent>() ||
            !player.HasComponent<SideCombatantComponent>() ||
            !player.HasComponent<SidePlayerControllerComponent>())
            return;

        auto& combatant = player.GetComponent<SideCombatantComponent>();
        auto& controller = player.GetComponent<SidePlayerControllerComponent>();
        const auto& tuning = GetTuning(level);
        ApplyPlayerTuning(tuning, combatant, controller);

        if (!combatant.Alive)
        {
            if (!level.RuntimeDefeat)
            {
                level.RuntimeDefeat = true;
                level.RuntimeResultTimer = 0.0f;
                level.RuntimeResultCommandIssued = false;
                level.RuntimeComboCount = 0;
                combatant.ControlsLocked = true;
            }
            return;
        }

        controller.RuntimeBasicCooldown = std::max(0.0f, controller.RuntimeBasicCooldown - dt);
        controller.RuntimeLauncherCooldown = std::max(0.0f, controller.RuntimeLauncherCooldown - dt);
        controller.RuntimeMagicBoltCooldown = std::max(0.0f, controller.RuntimeMagicBoltCooldown - dt);
        controller.RuntimeAllySupportCooldown = std::max(0.0f, controller.RuntimeAllySupportCooldown - dt);
        controller.RuntimeBreakLimitCooldown = std::max(0.0f, controller.RuntimeBreakLimitCooldown - dt);
        controller.RuntimeAttackChainTimer = std::max(0.0f, controller.RuntimeAttackChainTimer - dt);
        controller.RuntimeJumpBufferTimer = std::max(0.0f, controller.RuntimeJumpBufferTimer - dt);
        controller.RuntimeCoyoteTimer = std::max(0.0f, controller.RuntimeCoyoteTimer - dt);
        if (controller.RuntimeAttackChainTimer <= 0.0f)
            controller.RuntimeAttackChain = 0;

        if (input.JumpPressed && !previousInput.JumpPressed)
            controller.RuntimeJumpBufferTimer = std::max(0.0f, controller.JumpBufferTime);

        if (combatant.ControlsLocked || level.RuntimeVictory || level.RuntimeDefeat)
            return;

        if (combatant.RuntimeHitStun > 0.0f ||
            combatant.RuntimeState == SideCombatState::Knockdown ||
            combatant.RuntimeState == SideCombatState::Dead)
        {
            ClearPlayerAction(controller);
            if (player.HasComponent<SpriteRendererComponent>())
                player.GetComponent<SpriteRendererComponent>().FlipX = combatant.RuntimeFacing < 0.0f;
            return;
        }

        UpdatePlayerAction(scene, level, player, combatant, controller, dt);
        const bool canStartAction = CanStartPlayerAction(controller);
        const float actionMovementScale = GetPlayerActionMovementScale(controller);

        if (input.Horizontal != 0.0f)
        {
            combatant.RuntimeFacing = SideCombatMath::SignNonZero(input.Horizontal);
            const float targetSpeed = input.Horizontal * combatant.MoveSpeed * actionMovementScale;
            const float accel = combatant.RuntimeOnGround ? tuning.Player.GroundAcceleration : controller.AirControl;
            combatant.RuntimeVelocity.x = SideCombatMath::Approach(combatant.RuntimeVelocity.x, targetSpeed, accel * dt);
        }
        else if (combatant.RuntimeOnGround)
        {
            combatant.RuntimeVelocity.x = SideCombatMath::Approach(
                combatant.RuntimeVelocity.x,
                0.0f,
                controller.GroundFriction * dt);
        }

        if (input.Lane != 0.0f)
        {
            const float targetLaneSpeed = input.Lane * combatant.MoveSpeed * controller.LaneSpeedScale * tuning.LaneSpeedScale * actionMovementScale;
            combatant.RuntimeVelocity.y = SideCombatMath::Approach(
                combatant.RuntimeVelocity.y,
                targetLaneSpeed,
                std::max(controller.LaneAcceleration, tuning.LaneAcceleration) * dt);
        }
        else if (combatant.RuntimeOnGround)
        {
            combatant.RuntimeVelocity.y = SideCombatMath::Approach(
                combatant.RuntimeVelocity.y,
                0.0f,
                controller.GroundFriction * dt);
        }

        if (combatant.RuntimeOnGround)
        {
            controller.RuntimeJumpsRemaining = controller.MaxJumps;
            controller.RuntimeAirActionsRemaining = tuning.AirCombo.AirActionLimit;
            controller.RuntimeCoyoteTimer = std::max(controller.RuntimeCoyoteTimer, controller.CoyoteTime);
        }

        const bool canBufferedJump = controller.RuntimeJumpBufferTimer > 0.0f &&
            (controller.RuntimeJumpsRemaining > 0 || controller.RuntimeCoyoteTimer > 0.0f) &&
            canStartAction;
        if (canBufferedJump)
        {
            combatant.RuntimeOnGround = false;
            combatant.RuntimeAirVelocity = controller.JumpImpulse;
            if (controller.RuntimeJumpsRemaining > 0)
                --controller.RuntimeJumpsRemaining;
            else
                controller.RuntimeJumpsRemaining = 0;
            controller.RuntimeAirActionsRemaining = tuning.AirCombo.AirActionLimit;
            controller.RuntimeJumpBufferTimer = 0.0f;
            controller.RuntimeCoyoteTimer = 0.0f;
            SideCombatFeedbackService::PlaySfx(tuning.Feedback.JumpSound, tuning.Feedback.JumpSoundVolume);
        }

        if (input.BreakLimitPressed && !previousInput.BreakLimitPressed && canStartAction)
            CreateBreakLimitChase(scene, level, player, combatant, controller);
        else if (input.SupportPressed && !previousInput.SupportPressed && controller.RuntimeAllySupportCooldown <= 0.0f && canStartAction)
            CreateAllySupport(scene, level, player, combatant, controller);
        else if (input.MagicPressed && !previousInput.MagicPressed && controller.RuntimeMagicBoltCooldown <= 0.0f && canStartAction)
            CreatePlayerMagicBolt(scene, level, player, combatant, controller);
        else if (input.LauncherPressed && !previousInput.LauncherPressed && controller.RuntimeLauncherCooldown <= 0.0f && canStartAction)
            CreatePlayerLauncher(scene, level, player, combatant, controller);
        else if (!input.LauncherPressed && input.BasicPressed && !previousInput.BasicPressed && controller.RuntimeBasicCooldown <= 0.0f && canStartAction)
            CreatePlayerBasic(scene, level, player, combatant, controller);

        if (player.HasComponent<SpriteRendererComponent>())
            player.GetComponent<SpriteRendererComponent>().FlipX = combatant.RuntimeFacing < 0.0f;
    }

} // namespace Wheatear::SideCombatPlayerService
