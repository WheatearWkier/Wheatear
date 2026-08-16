#include "wtpch.h"
#include "SideCombatPlayerService.h"

#include "SideCombatActionService.h"
#include "SideCombatFeedbackService.h"
#include "SideCombatHitboxService.h"
#include "SideCombatHitResolutionService.h"
#include "SideCombatMath.h"
#include "SideCombatSkillRegistry.h"
#include "SideCombatTargetService.h"
#include "Wheatear/Input/InputBindingService.h"
#include "SideCombatTuningService.h"
#include "Wheatear/Gameplay/Action/ActionRecipeQueries.h"
#include "Wheatear/Gameplay/Action/EffectFormula.h"
#include "Wheatear/Gameplay/Action/EffectRegistry.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

#include <algorithm>
#include <cmath>
#include <limits>
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
        using SideCombatTargetService::FindNearestAliveEnemy;
        using SideCombatTuningService::ApplyPlayerTuning;
        using SideCombatTuningService::GetAttack;
        using SideCombatTuningService::GetTuning;
        using SideCombatTuningService::IsBreakLimitDebugAvailable;
        using SideCombatTuningService::IsBreakLimitOfficiallyAvailable;
        using SideCombatTuningService::IsSkillUnlocked;

        static std::string ActionRecipeId(const std::string& attackId)
        {
            return WAO::ComposeActionId("side", attackId);
        }

        static bool HasMana(const SidePlayerControllerComponent& controller, float cost)
        {
            return controller.RuntimeMana + 0.001f >= std::max(0.0f, cost);
        }

        static bool SpendMana(SidePlayerControllerComponent& controller, float cost)
        {
            cost = std::max(0.0f, cost);
            if (!HasMana(controller, cost))
                return false;

            controller.RuntimeMana = std::max(0.0f, controller.RuntimeMana - cost);
            return true;
        }

        static float RecipeResourceCost(const WAO::ActionRecipe* recipe,
            const std::string& resourceId,
            float fallback)
        {
            return recipe
                ? std::max(0.0f, WAO::ResourceCost(*recipe, resourceId, fallback))
                : std::max(0.0f, fallback);
        }

        static float BreakLimitGaugeCost(const SideCombatTuningService::SideCombatTuning& tuning,
            const SidePlayerControllerComponent& controller,
            const WAO::ActionRecipe* recipe = nullptr)
        {
            const float configuredCost = RecipeResourceCost(recipe,
                "magic_sword",
                tuning.AirCombo.BreakLimitGaugeCost);
            return std::max(configuredCost, controller.RuntimeMagicSwordGaugeMax * 0.5f);
        }

        static Entity FindBreakLimitTarget(Scene* scene, const glm::vec2& origin)
        {
            if (!scene)
                return {};

            auto& registry = scene->GetRegistry();
            Entity best;
            float bestDistanceSquared = std::numeric_limits<float>::max();
            for (auto e : registry.view<SideCombatantComponent, SideEnemyAIComponent>())
            {
                const auto& targetCombatant = registry.get<SideCombatantComponent>(e);
                const auto& ai = registry.get<SideEnemyAIComponent>(e);
                if (ai.Kind != SideEnemyKind::BearBoss ||
                    !targetCombatant.Alive ||
                    targetCombatant.RuntimeState != SideCombatState::SuperArmor ||
                    targetCombatant.RuntimeProtection <= 0.0f)
                {
                    continue;
                }

                const glm::vec2 delta = targetCombatant.RuntimeGroundPosition - origin;
                const float distanceSquared = glm::dot(delta, delta);
                if (distanceSquared < bestDistanceSquared)
                {
                    bestDistanceSquared = distanceSquared;
                    best = { e, scene };
                }
            }

            return best;
        }

        // Builds the attribute dictionary handed to formula evaluation and
        // custom effect handlers for an item use (source == target == player).
        static WAO::AttributeStore BuildItemAttributeStore(
            const SideCombatantComponent& combatant,
            const SidePlayerControllerComponent& controller)
        {
            WAO::AttributeStore vars;
            vars.Set("source.health", combatant.Health);
            vars.Set("source.max_health", combatant.MaxHealth);
            vars.Set("source.attack", combatant.Attack);
            vars.Set("source.defense", combatant.Defense);
            vars.Set("source.move_speed", combatant.MoveSpeed);
            vars.Set("target.health", combatant.Health);
            vars.Set("target.max_health", combatant.MaxHealth);
            vars.Set("target.attack", combatant.Attack);
            vars.Set("target.defense", combatant.Defense);
            vars.Set("controller.mana", controller.RuntimeMana);
            vars.Set("controller.max_mana", std::max(1.0f, controller.MaxMana));
            vars.Set("controller.heal_amount", controller.HealItemAmount);
            vars.Set("controller.mana_amount", controller.ManaItemAmount);
            vars.Set("controller.attack_buff_multiplier", controller.AttackBuffMultiplier);
            vars.Set("controller.attack_buff_duration", controller.AttackBuffDuration);
            return vars;
        }

        // Writes the mutated attribute dictionary back into the components.
        static void WriteBackItemAttributes(const WAO::AttributeStore& vars,
            SideCombatantComponent& combatant,
            SidePlayerControllerComponent& controller)
        {
            combatant.Health = std::clamp(
                vars.Get("target.health", combatant.Health),
                0.0f,
                combatant.MaxHealth);
            controller.RuntimeMana = std::clamp(
                vars.Get("controller.mana", controller.RuntimeMana),
                0.0f,
                std::max(1.0f, controller.MaxMana));
        }

        // Applies one WAO recipe effect to the combatant/controller for the
        // item-slot use case. Returns true when the effect was applied.
        static bool ApplyItemRecipeEffect(const WAO::EffectSpec& effect,
            SideCombatantComponent& combatant,
            SidePlayerControllerComponent& controller,
            float cooldown)
        {
            // Custom registered effect semantics run first: the handler sees
            // the attribute dictionary and may mutate it (write-back happens
            // in the caller through WriteBackItemAttributes).
            if (!effect.CustomType.empty())
            {
                WAO::CustomEffectContext context;
                WAO::AttributeStore vars = BuildItemAttributeStore(combatant, controller);
                context.Vars = &vars;
                context.InValue = effect.Value;
                if (WAO::EffectRegistry::Run(effect.CustomType, context))
                {
                    WriteBackItemAttributes(vars, combatant, controller);
                    // Cooldown arms via the mana/heal/attack fields the
                    // handler may have touched; a dedicated cooldown is not
                    // part of the shared attribute contract, so reuse the
                    // generic heal field as the item's cooldown gate.
                    controller.RuntimeHealItemCooldown = std::max(
                        controller.RuntimeHealItemCooldown, cooldown);
                    return true;
                }
                return false;
            }

            // Formula override: evaluate the expression against the attribute
            // dictionary and use the result as the effect's value.
            if (!effect.Formula.empty())
            {
                WAO::AttributeStore vars = BuildItemAttributeStore(combatant, controller);
                const float evaluated = WAO::EvaluateEffectFormula(effect.Formula, vars, effect.Value);
                if (evaluated != effect.Value)
                {
                    WAO::EffectSpec resolved = effect;
                    resolved.Value = evaluated;
                    return ApplyItemRecipeEffect(resolved, combatant, controller, cooldown);
                }
            }

            switch (effect.Type)
            {
            case WAO::EffectType::Heal:
                if (controller.RuntimeHealItemCooldown > 0.0f)
                    return false;
                if (combatant.Health >= combatant.MaxHealth - 0.001f)
                    return false;
                combatant.Health = std::min(combatant.MaxHealth,
                    combatant.Health + std::max(0.0f, effect.Value));
                controller.RuntimeHealItemCooldown = std::max(0.0f, cooldown);
                return true;
            case WAO::EffectType::ConsumeResource:
            case WAO::EffectType::ModifyAttribute:
            {
                // Negative resource cost restores mana (manaCost: -20);
                // attribute ids containing "mana" behave the same way.
                const std::string attributeId = effect.AttributeId;
                const bool manaLike = attributeId.find("mana") != std::string::npos
                    || attributeId.find("Mana") != std::string::npos;
                if (manaLike)
                {
                    if (controller.RuntimeManaItemCooldown > 0.0f)
                        return false;
                    const float delta = effect.Type == WAO::EffectType::ConsumeResource
                        ? -effect.Value
                        : effect.Value;
                    if (delta < 0.0f && controller.RuntimeMana >= controller.RuntimeManaMax - 0.001f)
                        return false;
                    controller.RuntimeMana = std::clamp(
                        controller.RuntimeMana + delta,
                        0.0f,
                        controller.RuntimeManaMax);
                    controller.RuntimeManaItemCooldown = std::max(0.0f, cooldown);
                    return true;
                }
                if (attributeId.find("attack") != std::string::npos
                    || attributeId.find("Attack") != std::string::npos)
                {
                    if (controller.RuntimeAttackBuffItemCooldown > 0.0f)
                        return false;
                    controller.RuntimeAttackBuffMultiplier = std::max(1.0f, effect.Value);
                    controller.RuntimeAttackBuffTimer = std::max(0.0f, effect.Seconds);
                    controller.RuntimeAttackBuffItemCooldown = std::max(0.0f, cooldown);
                    return true;
                }
                return false;
            }
            default:
                // Damage / states / signals on the player are out of scope for
                // consumables; author the effect with the kinds above.
                return false;
            }
        }

        static bool UseCombatItem(const SideCombatTuningService::SideCombatTuning& tuning,
            const SideCombatTuningService::SideItemSlotTuning& itemSlot,
            SideCombatantComponent& combatant,
            SidePlayerControllerComponent& controller)
        {
            // WAO-recipe items execute the recipe's effects (Heal /
            // ConsumeResource / ModifyAttribute / ...), authored in the WAO
            // Action Editor; the recipe's cooldown wins over the slot value.
            if (!itemSlot.RecipeId.empty())
            {
                const WAO::ActionRecipe* recipe =
                    WAO::FindRecipeOrWarn(itemSlot.RecipeId, "SideCombatItem");
                if (recipe)
                {
                    const float cooldown = recipe->Cooldown > 0.0f
                        ? recipe->Cooldown : itemSlot.Cooldown;
                    bool anyApplied = false;
                    for (const WAO::EffectSpec& effect : recipe->Effects)
                    {
                        if (ApplyItemRecipeEffect(effect, combatant, controller, cooldown))
                            anyApplied = true;
                    }
                    return anyApplied;
                }
            }

            // Data-driven item slots: the tuning table decides which effect a
            // slot carries; amounts still come from the controller's authored
            // fields so scenes keep their per-entity tuning.
            switch (itemSlot.Kind)
            {
            case SideCombatTuningService::SideItemSlotKind::Heal:
                if (controller.RuntimeHealItemCooldown > 0.0f)
                    return false;
                if (combatant.Health >= combatant.MaxHealth - 0.001f)
                    return false;
                combatant.Health = std::min(combatant.MaxHealth,
                    combatant.Health + std::max(0.0f, controller.HealItemAmount));
                controller.RuntimeHealItemCooldown = std::max(0.0f, itemSlot.Cooldown);
                return true;
            case SideCombatTuningService::SideItemSlotKind::Mana:
                if (controller.RuntimeManaItemCooldown > 0.0f)
                    return false;
                if (controller.RuntimeMana >= controller.RuntimeManaMax - 0.001f)
                    return false;
                controller.RuntimeMana = std::min(controller.RuntimeManaMax,
                    controller.RuntimeMana + std::max(0.0f, controller.ManaItemAmount));
                controller.RuntimeManaItemCooldown = std::max(0.0f, itemSlot.Cooldown);
                return true;
            case SideCombatTuningService::SideItemSlotKind::AttackBuff:
                if (controller.RuntimeAttackBuffItemCooldown > 0.0f)
                    return false;
                controller.RuntimeAttackBuffMultiplier = std::max(1.0f, controller.AttackBuffMultiplier);
                controller.RuntimeAttackBuffTimer = std::max(0.0f, controller.AttackBuffDuration);
                controller.RuntimeAttackBuffItemCooldown = std::max(0.0f, itemSlot.Cooldown);
                return true;
            default:
                return false;
            }
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

            if (controller.RuntimeActionKind == SideAttackKind::Dash)
            {
                const float dashSpeed = std::max(controller.DashSpeed, std::abs(attack.Velocity.x));
                combatant.RuntimeVelocity.x = combatant.RuntimeFacing * dashSpeed;
                if (combatant.RuntimeOnGround)
                {
                    combatant.RuntimeVelocity.y = SideCombatMath::Approach(
                        combatant.RuntimeVelocity.y,
                        0.0f,
                        controller.GroundFriction * dt);
                }

                const float invulnerableRemaining = controller.DashInvulnerableTime - controller.RuntimeActionTimer;
                if (invulnerableRemaining > 0.0f)
                    combatant.RuntimeInvulnerableTimer = std::max(combatant.RuntimeInvulnerableTimer, invulnerableRemaining);

                if (!controller.RuntimeActionHitboxSpawned &&
                    controller.RuntimeActionTimer >= controller.RuntimeActionHitboxTime)
                {
                    CreateHitbox(scene,
                        controller.RuntimeActionEntityName.empty() ? "Side_PlayerDash" : controller.RuntimeActionEntityName,
                        static_cast<entt::entity>(player),
                        combatant.RuntimeGroundPosition,
                        combatant.RuntimeAirHeight,
                        combatant.RuntimeFacing,
                        controller.RuntimeActionKind,
                        controller.RuntimeActionRecipeId,
                        (int)SideCombatTeam::Player,
                        attack,
                        combatant.Attack * std::max(1.0f, controller.RuntimeAttackBuffMultiplier) * attack.DamageScale + attack.DamageFlat,
                        tuning);
                    controller.RuntimeActionHitboxSpawned = true;
                }

                if (controller.RuntimeActionTimer >= controller.RuntimeActionDuration)
                    ClearPlayerAction(controller);
                return;
            }

            if (!controller.RuntimeActionHitboxSpawned &&
                controller.RuntimeActionTimer >= controller.RuntimeActionHitboxTime)
            {
                glm::vec2 origin = combatant.RuntimeGroundPosition;
                float sourceAirHeight = combatant.RuntimeAirHeight;
                float facing = combatant.RuntimeFacing;

                if (controller.RuntimeActionKind == SideAttackKind::AllySupport ||
                    controller.RuntimeActionKind == SideAttackKind::BreakLimit)
                {
                    Entity target = controller.RuntimeActionKind == SideAttackKind::BreakLimit
                        ? FindBreakLimitTarget(scene, combatant.RuntimeGroundPosition)
                        : FindNearestAliveEnemy(scene, player.GetComponent<TransformComponent>().Translation);
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
                    else
                    {
                        controller.RuntimeActionHitboxSpawned = true;
                        return;
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
                    combatant.Attack * std::max(1.0f, controller.RuntimeAttackBuffMultiplier) * attack.DamageScale + attack.DamageFlat,
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
                const std::string recipeId = ActionRecipeId("air_basic");
                const WAO::ActionRecipe* recipe = WAO::FindRecipeOrWarn(recipeId, "SideCombat.Player");
                controller.RuntimeBasicCooldown = (recipe && recipe->Cooldown > 0.0f)
                    ? recipe->Cooldown
                    : tuning.AirCombo.AirBasicCooldown;
                BeginPlayerAction(controller, attack, "air_basic", recipeId, "Side_PlayerAirSlash", SideAttackKind::Basic);
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
            const std::string recipeId = ActionRecipeId(attackId);
            const WAO::ActionRecipe* recipe = WAO::FindRecipeOrWarn(recipeId, "SideCombat.Player");
            controller.RuntimeBasicCooldown = (recipe && recipe->Cooldown > 0.0f)
                ? recipe->Cooldown
                : (chain == 3 ? controller.BasicCooldown + tuning.Player.BasicFinisherExtraCooldown : controller.BasicCooldown);
            BeginPlayerAction(controller, attack, attackId, recipeId, "Side_PlayerSlash", SideAttackKind::Basic);
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
            if (!SpendMana(controller, controller.LauncherManaCost))
                return;

            controller.RuntimeAttackChainTimer = tuning.Player.LauncherChainWindow;

            const std::string attackId = airborne ? "air_chase" : "launcher";
            const auto& attack = GetAttack(tuning, attackId);
            const std::string recipeId = ActionRecipeId(attackId);
            const WAO::ActionRecipe* recipe = WAO::FindRecipeOrWarn(recipeId, "SideCombat.Player");
            controller.RuntimeLauncherCooldown = (recipe && recipe->Cooldown > 0.0f)
                ? recipe->Cooldown
                : (airborne ? tuning.AirCombo.AirChaseCooldown : controller.LauncherCooldown);
            if (airborne)
                --controller.RuntimeAirActionsRemaining;
            else
                controller.RuntimeAirActionsRemaining = tuning.AirCombo.AirActionLimit;

            BeginPlayerAction(controller,
                attack,
                attackId,
                recipeId,
                airborne ? "Side_PlayerAirChase" : "Side_PlayerLauncher",
                SideAttackKind::Launcher);
        }

        static void CreatePlayerDash(Scene*,
            SideCombatLevelComponent& level,
            Entity,
            SideCombatantComponent& combatant,
            SidePlayerControllerComponent& controller)
        {
            const auto& tuning = GetTuning(level);
            if (!IsSkillUnlocked(level, tuning, "dash"))
                return;

            const auto& attack = GetAttack(tuning, "dash");
            const std::string recipeId = ActionRecipeId("dash");
            const WAO::ActionRecipe* recipe = WAO::FindRecipeOrWarn(recipeId, "SideCombat.Player");
            const float manaCost = RecipeResourceCost(recipe, "mana", controller.DashManaCost);
            if (!SpendMana(controller, manaCost))
                return;

            controller.RuntimeDashCooldown = (recipe && recipe->Cooldown > 0.0f)
                ? recipe->Cooldown
                : controller.DashCooldown;
            combatant.RuntimeInvulnerableTimer = std::max(
                combatant.RuntimeInvulnerableTimer,
                controller.DashInvulnerableTime);
            BeginPlayerAction(controller, attack, "dash", recipeId, "Side_PlayerDash", SideAttackKind::Dash);
        }

        static bool CanUseBreakLimit(Scene* scene,
            const SideCombatLevelComponent& level,
            const SideCombatTuningService::SideCombatTuning& tuning,
            Entity target,
            const SidePlayerControllerComponent& controller)
        {
            if (!scene || !target || !target.HasComponent<SideCombatantComponent>())
                return false;
            if (!IsBreakLimitOfficiallyAvailable(level, tuning) && !IsBreakLimitDebugAvailable(level, tuning))
                return false;
            if (controller.RuntimeBreakLimitCooldown > 0.0f)
                return false;
            if (controller.RuntimeMagicSwordGauge + 0.001f < BreakLimitGaugeCost(tuning, controller))
                return false;
            if (level.RuntimeComboCount < tuning.AirCombo.BreakLimitMinCombo)
                return false;

            auto& targetCombatant = target.GetComponent<SideCombatantComponent>();
            if (!targetCombatant.Alive ||
                !IsBossEntity(scene, static_cast<entt::entity>(target)) ||
                targetCombatant.RuntimeState != SideCombatState::SuperArmor ||
                targetCombatant.RuntimeProtection <= 0.0f)
            {
                return false;
            }

            return true;
        }

        static void CreateBreakLimit(Scene* scene,
            SideCombatLevelComponent& level,
            Entity player,
            SideCombatantComponent& combatant,
            SidePlayerControllerComponent& controller)
        {
            const auto& tuning = GetTuning(level);
            Entity target = FindBreakLimitTarget(scene, combatant.RuntimeGroundPosition);
            if (!CanUseBreakLimit(scene, level, tuning, target, controller))
                return;

            const auto& attack = GetAttack(tuning, "break_limit");
            const std::string recipeId = ActionRecipeId("break_limit");
            const WAO::ActionRecipe* recipe = WAO::FindRecipeOrWarn(recipeId, "SideCombat.Player");
            controller.RuntimeBreakLimitCooldown = (recipe && recipe->Cooldown > 0.0f)
                ? recipe->Cooldown
                : tuning.AirCombo.BreakLimitCooldown;
            controller.RuntimeMagicSwordGauge = std::max(0.0f,
                controller.RuntimeMagicSwordGauge - BreakLimitGaugeCost(tuning, controller, recipe));
            controller.RuntimeAttackChainTimer = tuning.Player.LauncherChainWindow;

            if (target && target.HasComponent<SideCombatantComponent>())
            {
                const auto& targetCombatant = target.GetComponent<SideCombatantComponent>();
                combatant.RuntimeFacing = SideCombatMath::SignNonZero(targetCombatant.RuntimeGroundPosition.x - combatant.RuntimeGroundPosition.x);
            }

            BeginPlayerAction(controller, attack, "break_limit", recipeId, "Side_BreakLimit", SideAttackKind::BreakLimit);
            SideCombatFeedbackService::TriggerCinematicFocus(
                scene,
                level,
                player.GetUUID(),
                tuning.Feedback.BreakLimitCinematicDuration,
                tuning.Feedback.BreakLimitCinematicTimeScale,
                tuning.Feedback.BreakLimitCameraZoom,
                tuning.Feedback.BreakLimitCameraOffset);
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
            if (!SpendMana(controller, controller.MagicBoltManaCost))
                return;

            controller.RuntimeAttackChainTimer = tuning.Player.MagicChainWindow;

            const auto& attack = GetAttack(tuning, "magic_bolt");
            const std::string recipeId = ActionRecipeId("magic_bolt");
            const WAO::ActionRecipe* recipe = WAO::FindRecipeOrWarn(recipeId, "SideCombat.Player");
            controller.RuntimeMagicBoltCooldown = (recipe && recipe->Cooldown > 0.0f)
                ? recipe->Cooldown
                : controller.MagicBoltCooldown;
            BeginPlayerAction(controller, attack, "magic_bolt", recipeId, "Side_PlayerMagicBolt", SideAttackKind::MagicBolt);
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
            if (!SpendMana(controller, controller.AllySupportManaCost))
                return;

            controller.RuntimeAttackChainTimer = tuning.Player.SupportChainWindow;

            const auto& attack = GetAttack(tuning, "ally_support");
            const std::string recipeId = ActionRecipeId("ally_support");
            const WAO::ActionRecipe* recipe = WAO::FindRecipeOrWarn(recipeId, "SideCombat.Player");
            controller.RuntimeAllySupportCooldown = (recipe && recipe->Cooldown > 0.0f)
                ? recipe->Cooldown
                : controller.AllySupportCooldown;
            BeginPlayerAction(controller, attack, "ally_support", recipeId, "Side_AllySupport", SideAttackKind::AllySupport);
        }

    } // namespace

    void UpdatePlayer(Scene* scene,
        SideCombatLevelComponent& level,
        Entity player,
        float dt,
        const PlayerInputState& input)
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
        controller.RuntimeDashCooldown = std::max(0.0f, controller.RuntimeDashCooldown - dt);
        controller.RuntimeHealItemCooldown = std::max(0.0f, controller.RuntimeHealItemCooldown - dt);
        controller.RuntimeManaItemCooldown = std::max(0.0f, controller.RuntimeManaItemCooldown - dt);
        controller.RuntimeAttackBuffItemCooldown = std::max(0.0f, controller.RuntimeAttackBuffItemCooldown - dt);
        controller.RuntimeBreakLimitCooldown = std::max(0.0f, controller.RuntimeBreakLimitCooldown - dt);
        controller.RuntimeAttackChainTimer = std::max(0.0f, controller.RuntimeAttackChainTimer - dt);
        controller.RuntimeAttackBuffTimer = std::max(0.0f, controller.RuntimeAttackBuffTimer - dt);
        if (controller.RuntimeAttackBuffTimer <= 0.0f)
            controller.RuntimeAttackBuffMultiplier = 1.0f;
        controller.RuntimeManaMax = std::max(1.0f, controller.MaxMana);
        controller.RuntimeMana = std::clamp(controller.RuntimeMana, 0.0f, controller.RuntimeManaMax);
        controller.RuntimeJumpBufferTimer = std::max(0.0f, controller.RuntimeJumpBufferTimer - dt);
        controller.RuntimeCoyoteTimer = std::max(0.0f, controller.RuntimeCoyoteTimer - dt);
        if (controller.RuntimeAttackChainTimer <= 0.0f)
            controller.RuntimeAttackChain = 0;

        const bool inputLockedByCinematic = level.RuntimeCinematicTimer > 0.0f;

        if (!inputLockedByCinematic && InputBindingService::IsActionPressed("side.jump"))
            controller.RuntimeJumpBufferTimer = std::max(0.0f, controller.JumpBufferTime);

        if (combatant.ControlsLocked || level.RuntimeVictory || level.RuntimeDefeat)
            return;

        // Poll every data-driven item slot; extra slots added in the tuning
        // table (with a matching input action) light up automatically.
        if (!inputLockedByCinematic)
        {
            for (const auto& itemSlot : tuning.ItemSlots)
            {
                if (InputBindingService::IsActionPressed(itemSlot.ActionId))
                    UseCombatItem(tuning, itemSlot, combatant, controller);
            }
        }

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
        if (inputLockedByCinematic)
        {
            if (player.HasComponent<SpriteRendererComponent>())
                player.GetComponent<SpriteRendererComponent>().FlipX = combatant.RuntimeFacing < 0.0f;
            return;
        }

        const bool canStartAction = CanStartPlayerAction(controller);
        const float actionMovementScale = GetPlayerActionMovementScale(controller);
        const bool dashActive = IsPlayerActionActive(controller) &&
            controller.RuntimeActionKind == SideAttackKind::Dash;

        if (!dashActive && input.Horizontal != 0.0f)
        {
            combatant.RuntimeFacing = SideCombatMath::SignNonZero(input.Horizontal);
            const float targetSpeed = input.Horizontal * combatant.MoveSpeed * actionMovementScale;
            const float accel = combatant.RuntimeOnGround ? tuning.Player.GroundAcceleration : controller.AirControl;
            combatant.RuntimeVelocity.x = SideCombatMath::Approach(combatant.RuntimeVelocity.x, targetSpeed, accel * dt);
        }
        else if (!dashActive && combatant.RuntimeOnGround)
        {
            combatant.RuntimeVelocity.x = SideCombatMath::Approach(
                combatant.RuntimeVelocity.x,
                0.0f,
                controller.GroundFriction * dt);
        }

        if (!dashActive && input.Lane != 0.0f)
        {
            const float targetLaneSpeed = input.Lane * combatant.MoveSpeed * controller.LaneSpeedScale * tuning.LaneSpeedScale * actionMovementScale;
            combatant.RuntimeVelocity.y = SideCombatMath::Approach(
                combatant.RuntimeVelocity.y,
                targetLaneSpeed,
                std::max(controller.LaneAcceleration, tuning.LaneAcceleration) * dt);
        }
        else if (!dashActive && combatant.RuntimeOnGround)
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

        // Poll every data-driven skill slot. Extra slots added in the tuning
        // table (with a matching input action) light up automatically; the
        // slot's kind selects the runtime behaviour. Slots keep the legacy
        // priority semantics: at most one skill starts per frame.
        bool skillStarted = false;
        for (const auto& skillSlot : tuning.SkillSlots)
        {
            if (skillStarted)
                break;
            if (!skillSlot.Enabled)
                continue;
            const bool pressed = InputBindingService::IsActionPressed(skillSlot.ActionId);
            if (!pressed)
                continue;

            switch (skillSlot.Kind)
            {
            case SideCombatTuningService::SideSkillSlotKind::BreakLimit:
                if (canStartAction)
                {
                    CreateBreakLimit(scene, level, player, combatant, controller);
                    skillStarted = true;
                }
                break;
            case SideCombatTuningService::SideSkillSlotKind::Dash:
                if (controller.RuntimeDashCooldown <= 0.0f && canStartAction)
                {
                    CreatePlayerDash(scene, level, player, combatant, controller);
                    skillStarted = true;
                }
                break;
            case SideCombatTuningService::SideSkillSlotKind::AllySupport:
                if (controller.RuntimeAllySupportCooldown <= 0.0f && canStartAction)
                {
                    CreateAllySupport(scene, level, player, combatant, controller);
                    skillStarted = true;
                }
                break;
            case SideCombatTuningService::SideSkillSlotKind::MagicBolt:
                if (controller.RuntimeMagicBoltCooldown <= 0.0f && canStartAction)
                {
                    CreatePlayerMagicBolt(scene, level, player, combatant, controller);
                    skillStarted = true;
                }
                break;
            case SideCombatTuningService::SideSkillSlotKind::Launcher:
                // Down + basic is the classic launcher shortcut; keep it
                // working only for the default launcher slot.
                if (skillSlot.SlotId == "launcher"
                    && InputBindingService::IsActionDown("move.down")
                    && InputBindingService::IsActionPressed("side.basic"))
                {
                    CreatePlayerLauncher(scene, level, player, combatant, controller);
                    skillStarted = true;
                }
                else if (canStartAction)
                {
                    CreatePlayerLauncher(scene, level, player, combatant, controller);
                    skillStarted = true;
                }
                break;
            case SideCombatTuningService::SideSkillSlotKind::Custom:
            {
                // Registered custom behaviour (SideCombatSkillRegistry);
                // new skill semantics are one registration, then authored as
                // a skill slot with kind: custom + customBehavior: <id>.
                if (canStartAction)
                {
                    SideCombatSkillRegistry::SkillBehaviorContext behaviorContext;
                    behaviorContext.Scene = scene;
                    behaviorContext.Level = &level;
                    behaviorContext.Player = player;
                    behaviorContext.Combatant = &combatant;
                    behaviorContext.Controller = &controller;
                    if (SideCombatSkillRegistry::Run(skillSlot.CustomBehavior, behaviorContext))
                        skillStarted = true;
                }
                break;
            }
            case SideCombatTuningService::SideSkillSlotKind::Basic:
            default:
                if (!InputBindingService::IsActionDown("move.down")
                    && controller.RuntimeBasicCooldown <= 0.0f && canStartAction)
                {
                    CreatePlayerBasic(scene, level, player, combatant, controller);
                    skillStarted = true;
                }
                break;
            }
        }

        if (player.HasComponent<SpriteRendererComponent>())
            player.GetComponent<SpriteRendererComponent>().FlipX = combatant.RuntimeFacing < 0.0f;
    }

} // namespace Wheatear::SideCombatPlayerService
