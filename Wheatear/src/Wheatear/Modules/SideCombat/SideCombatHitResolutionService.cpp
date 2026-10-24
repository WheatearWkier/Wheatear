#include "wtpch.h"
#include "SideCombatHitResolutionService.h"

#include "SideCombatActionService.h"
#include "SideCombatGuardService.h"
#include "Wheatear/Gameplay/Action/ActionDebugHistory.h"
#include "Wheatear/Gameplay/Behavior/BehaviorVM.h"
#include "Wheatear/Gameplay/Services/GameplayCombatService.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

#include <algorithm>
#include <sstream>

namespace Wheatear::SideCombatHitResolutionService {

    namespace {

        static UUID ResolveEntityUUID(Scene* scene, entt::entity entity)
        {
            if (!scene || entity == entt::null)
                return 0;

            auto& registry = scene->GetRegistry();
            if (!registry.valid(entity) || !registry.all_of<IDComponent>(entity))
                return 0;

            return registry.get<IDComponent>(entity).ID;
        }

        static std::string ResolveLedgerActionId(const SideHitboxComponent& hitbox)
        {
            if (!hitbox.ActionRecipeId.empty())
                return hitbox.ActionRecipeId;

            return hitbox.Team == (int)SideCombatTeam::Enemy
                ? "side.enemy_hitbox"
                : "side.player_hitbox";
        }

        static std::string FormatValue(float value)
        {
            std::ostringstream stream;
            stream.setf(std::ios::fixed);
            stream.precision(2);
            stream << value;
            return stream.str();
        }

        static void RecordLedgerEntry(WAO::EffectLedger& ledger,
            const WAO::ActionIntent& intent,
            WAO::EffectType type,
            const std::string& detail,
            float value,
            bool applied)
        {
            ledger.Record({
                intent.ActionId,
                type,
                intent.Actor,
                intent.ExplicitTarget,
                detail,
                value,
                applied
            });
        }

        static void RecordHitLedger(Scene* scene,
            entt::entity targetEntity,
            const SideCombatantComponent& target,
            const SideHitboxComponent& hitbox,
            float requestedDamage,
            const HitResolutionResult& result,
            bool launched,
            float launchVelocity)
        {
            WAO::ActionIntent intent;
            intent.ActionId = ResolveLedgerActionId(hitbox);
            intent.Actor = ResolveEntityUUID(scene, static_cast<entt::entity>(hitbox.RuntimeOwnerEntity));
            intent.ExplicitTarget = ResolveEntityUUID(scene, targetEntity);
            intent.WorldPoint = target.RuntimeGroundPosition;
            intent.Source = hitbox.Team == (int)SideCombatTeam::Enemy
                ? "SideCombat.EnemyHit"
                : "SideCombat.PlayerHit";

            WAO::EffectLedger ledger;
            ledger.BeginAction(intent);
            RecordLedgerEntry(ledger,
                intent,
                WAO::EffectType::Damage,
                "Damage requested " + FormatValue(requestedDamage),
                result.Damage,
                result.Damage > 0.0f);
            RecordLedgerEntry(ledger,
                intent,
                WAO::EffectType::HitStun,
                "HitStun",
                hitbox.HitStun,
                target.Alive && hitbox.HitStun > 0.0f);

            if (launched)
            {
                RecordLedgerEntry(ledger,
                    intent,
                    WAO::EffectType::Launch,
                    "Launch",
                    launchVelocity,
                    true);
            }

            if (hitbox.ProtectionGain > 0.0f || result.BossProtectionTriggered)
            {
                RecordLedgerEntry(ledger,
                    intent,
                    WAO::EffectType::ModifyAttribute,
                    result.BossProtectionTriggered ? "BossProtectionRecovery" : "BossProtection",
                    target.RuntimeProtection,
                    true);
            }

            if (result.PlayerWasHit)
            {
                RecordLedgerEntry(ledger,
                    intent,
                    WAO::EffectType::EmitSignal,
                    "PlayerHitTaken",
                    1.0f,
                    true);
            }

            if (result.Blocked)
            {
                RecordLedgerEntry(ledger,
                    intent,
                    WAO::EffectType::ModifyAttribute,
                    "GuardBlocked",
                    result.Damage,
                    true);
            }

            if (result.Parried)
            {
                RecordLedgerEntry(ledger,
                    intent,
                    WAO::EffectType::HitStun,
                    "Parried",
                    hitbox.HitStun,
                    true);
            }

            if (result.TargetDied)
            {
                RecordLedgerEntry(ledger,
                    intent,
                    WAO::EffectType::AddState,
                    "Dead",
                    1.0f,
                    true);
            }

            WAO::ActionDebugHistory::Record(ledger, true, "Side hit resolved");
        }

        static bool ApplyBossProtectionOnHit(Scene* scene,
            SideCombatLevelComponent& level,
            const SideCombatTuningService::SideCombatTuning& tuning,
            entt::entity targetEntity,
            SideCombatantComponent& target,
            const SideHitboxComponent& hitbox)
        {
            if (hitbox.Team != (int)SideCombatTeam::Player)
                return false;
            if (!IsBossEntity(scene, targetEntity))
                return false;

            target.RuntimeProtectionMax = std::max(1.0f, tuning.Protection.BossProtectionMax);
            if (hitbox.AttackKind == SideAttackKind::BreakLimit)
            {
                target.RuntimeProtection = 0.0f;
                SetCombatState(target, SideCombatState::Broken, std::max(0.16f, hitbox.HitStun));
                return false;
            }

            float gain = std::max(0.0f, hitbox.ProtectionGain);
            if (!IsControlledAirborne(target) && hitbox.LaunchVelocity.y <= 0.0f)
                gain *= 0.5f;

            target.RuntimeProtection = std::clamp(
                target.RuntimeProtection + gain,
                0.0f,
                target.RuntimeProtectionMax);

            if (target.RuntimeProtection < target.RuntimeProtectionMax - 0.001f)
                return false;

            EnterBossProtectionRecovery(target, tuning.Protection);
            level.RuntimeComboCount = 0;
            level.RuntimeComboTimer = 0.0f;
            return true;
        }

    } // namespace

    float CalculateDamage(float rawDamage,
        float defense,
        const SideCombatTuningService::SideCombatRuleTuning& rules)
    {
        return GameplayCombatService::DamageWithDefense(rawDamage, defense, rules.MinDamage, rules.DefenseBase);
    }

    bool IsBossEntity(Scene* scene, entt::entity entity)
    {
        if (!scene)
            return false;

        auto& registry = scene->GetRegistry();
        return registry.all_of<SideEnemyAIComponent>(entity) &&
            registry.get<SideEnemyAIComponent>(entity).Kind == SideEnemyKind::BearBoss;
    }

    bool IsControlledAirborne(const SideCombatantComponent& combatant)
    {
        return !combatant.RuntimeOnGround || combatant.RuntimeAirHeight > 0.05f;
    }

    bool CanEnemyAct(const SideCombatantComponent& combatant)
    {
        return combatant.Alive &&
            combatant.RuntimeHitStun <= 0.0f &&
            !IsControlledAirborne(combatant) &&
            (combatant.RuntimeState == SideCombatState::Normal ||
                combatant.RuntimeState == SideCombatState::SuperArmor);
    }

    void SetCombatState(SideCombatantComponent& combatant,
        SideCombatState state,
        float duration)
    {
        combatant.RuntimeState = state;
        combatant.RuntimeStateTimer = std::max(0.0f, duration);
    }

    void EnterBossProtectionRecovery(SideCombatantComponent& boss,
        const SideCombatTuningService::SideProtectionTuning& protection)
    {
        boss.RuntimeProtection = boss.RuntimeProtectionMax;
        boss.RuntimeHitStun = 0.0f;
        boss.RuntimeInvulnerableTimer = 0.0f;
        boss.RuntimeVelocity = { 0.0f, 0.0f };
        boss.RuntimeAirVelocity = std::min(
            boss.RuntimeAirVelocity,
            protection.BossProtectionForceFallVelocity);
        SetCombatState(boss, SideCombatState::SuperArmor, protection.BossProtectionLimitTime);
    }

    HitResolutionResult ResolveHit(Scene* scene,
        SideCombatLevelComponent& level,
        const SideCombatTuningService::SideCombatTuning& tuning,
        entt::entity targetEntity,
        SideCombatantComponent& target,
        const SideHitboxComponent& hitbox)
    {
        HitResolutionResult result;

        const float requestedDamage = CalculateDamage(hitbox.Damage, target.Defense, tuning.Combat);
        const SideCombatGuardService::GuardDecision guard =
            SideCombatGuardService::ResolveIncomingHit(target, hitbox);
        result.Blocked = guard.IsBlocked();
        result.Parried = guard.IsParried();
        const float resolvedDamage = result.Parried
            ? 0.0f
            : requestedDamage * guard.DamageMultiplier;
        result.Damage = GameplayCombatService::ApplyDamage(target.Health, resolvedDamage);
        target.Alive = GameplayCombatService::IsAlive(target.Health);
        if (result.Damage > 0.0f || result.Blocked)
            target.RuntimeInvulnerableTimer = tuning.Combat.HitInvulnerableTime;

        const bool bossSuperArmor =
            scene &&
            target.Team == (int)SideCombatTeam::Enemy &&
            IsBossEntity(scene, targetEntity) &&
            target.RuntimeState == SideCombatState::SuperArmor;
        const bool suppressControl = result.Blocked ||
            result.Parried ||
            (bossSuperArmor && hitbox.AttackKind != SideAttackKind::BreakLimit);

        if (result.Parried)
            target.RuntimeHitStun = 0.0f;
        else if (result.Blocked)
            target.RuntimeHitStun = std::max(
                target.RuntimeHitStun,
                hitbox.HitStun * 0.35f);
        else if (!suppressControl)
            target.RuntimeHitStun = std::max(target.RuntimeHitStun, hitbox.HitStun);
        else
            target.RuntimeHitStun = 0.0f;

        result.PlayerWasHit =
            hitbox.Team == (int)SideCombatTeam::Enemy &&
            target.Team == (int)SideCombatTeam::Player &&
            !result.Blocked &&
            !result.Parried;
        if (result.PlayerWasHit)
            ++level.RuntimePlayerHitsTaken;

        if (!target.Alive && !result.Parried)
        {
            result.TargetDied = true;
            SetCombatState(target, SideCombatState::Dead);
            target.RuntimeDeathTimer = 0.0f;
            target.RuntimeRemoveAfterDeath = false;
            if (scene && target.Team == (int)SideCombatTeam::Enemy)
            {
                auto& registry = scene->GetRegistry();
                if (targetEntity != entt::null &&
                    registry.all_of<SideEnemyAIComponent>(targetEntity) &&
                    registry.get<SideEnemyAIComponent>(targetEntity).Kind != SideEnemyKind::BearBoss)
                {
                    target.RuntimeRemoveAfterDeath = true;
                }
            }
        }

        bool launched = false;
        float appliedLaunchVelocity = 0.0f;

        if (scene)
        {
            auto& registry = scene->GetRegistry();
            const entt::entity attackerEntity =
                static_cast<entt::entity>(hitbox.RuntimeOwnerEntity);
            if (result.Parried &&
                attackerEntity != entt::null &&
                registry.valid(attackerEntity))
            {
                if (registry.all_of<SideCombatantComponent>(attackerEntity))
                {
                    auto& attacker = registry.get<SideCombatantComponent>(attackerEntity);
                    attacker.RuntimeHitStun = std::max(
                        attacker.RuntimeHitStun,
                        std::max(0.16f, hitbox.HitStun));
                    attacker.RuntimeVelocity = { 0.0f, 0.0f };
                }
                if (registry.all_of<SideEnemyAIComponent>(attackerEntity))
                    SideCombatActionService::ClearEnemyAction(
                        registry.get<SideEnemyAIComponent>(attackerEntity));
                if (registry.all_of<SidePlayerControllerComponent>(attackerEntity))
                    SideCombatActionService::ClearPlayerAction(
                        registry.get<SidePlayerControllerComponent>(attackerEntity));
            }

            if (target.Team == (int)SideCombatTeam::Enemy &&
                registry.all_of<SideEnemyAIComponent>(targetEntity))
            {
                if (!suppressControl)
                    SideCombatActionService::ClearEnemyAction(registry.get<SideEnemyAIComponent>(targetEntity));
            }
            if (target.Team == (int)SideCombatTeam::Player &&
                registry.all_of<SidePlayerControllerComponent>(targetEntity))
            {
                SideCombatActionService::ClearPlayerAction(registry.get<SidePlayerControllerComponent>(targetEntity));
            }

            if (target.Alive && !result.Parried)
            {
                if (!suppressControl)
                {
                    const float resistanceScale = std::clamp(1.0f - target.KnockbackResistance, 0.25f, 1.0f);
                    target.RuntimeVelocity.x = hitbox.LaunchVelocity.x * resistanceScale;
                    float launchY = hitbox.LaunchVelocity.y;
                    if (hitbox.Team == (int)SideCombatTeam::Player &&
                        registry.all_of<SideEnemyAIComponent>(targetEntity) &&
                        registry.get<SideEnemyAIComponent>(targetEntity).Kind == SideEnemyKind::BearBoss &&
                        launchY > 0.0f)
                    {
                        launchY *= tuning.BossLaunchBonus;
                    }
                    target.RuntimeAirVelocity = std::max(target.RuntimeAirVelocity, launchY * resistanceScale);
                    if (launchY > 0.0f)
                    {
                        launched = true;
                        appliedLaunchVelocity = launchY * resistanceScale;
                        target.RuntimeAirHeight = std::max(target.RuntimeAirHeight, 0.05f);
                        target.RuntimeOnGround = false;
                        SetCombatState(target, SideCombatState::Launched);
                    }
                    else
                    {
                        SetCombatState(target, SideCombatState::HitStun, hitbox.HitStun);
                    }
                }
                else
                {
                    target.RuntimeVelocity = { 0.0f, 0.0f };
                }

                if (!suppressControl || hitbox.AttackKind == SideAttackKind::BreakLimit)
                {
                    result.BossProtectionTriggered = ApplyBossProtectionOnHit(scene,
                        level,
                        tuning,
                        targetEntity,
                        target,
                        hitbox);
                }
            }
            else if (!result.Parried)
            {
                target.RuntimeVelocity = { 0.0f, 0.0f };
                target.RuntimeAirVelocity = 0.0f;
            }
        }

        RecordHitLedger(scene,
            targetEntity,
            target,
            hitbox,
            requestedDamage,
            result,
            launched,
            appliedLaunchVelocity);

        if (scene)
        {
            WAO::BehaviorEvent event;
            event.Id = "combat.hit_received";
            event.Source = ResolveEntityUUID(
                scene,
                static_cast<entt::entity>(hitbox.RuntimeOwnerEntity));
            event.Target = ResolveEntityUUID(scene, targetEntity);
            event.Payload["damage"] = WAO::BehaviorValue(result.Damage);
            event.Payload["requestedDamage"] = WAO::BehaviorValue(requestedDamage);
            event.Payload["hitStun"] = WAO::BehaviorValue(hitbox.HitStun);
            event.Payload["launched"] = WAO::BehaviorValue(launched);
            event.Payload["targetDied"] = WAO::BehaviorValue(result.TargetDied);
            event.Payload["playerWasHit"] = WAO::BehaviorValue(result.PlayerWasHit);
            event.Payload["blocked"] = WAO::BehaviorValue(result.Blocked);
            event.Payload["parried"] = WAO::BehaviorValue(result.Parried);
            event.Payload["attackKind"] = WAO::BehaviorValue(
                static_cast<int>(hitbox.AttackKind));
            WAO::BehaviorVM::EmitEvent(scene, event);

            if (result.Blocked || result.Parried)
            {
                WAO::BehaviorEvent guardEvent;
                guardEvent.Id = "combat.guard_resolved";
                guardEvent.Source = event.Source;
                guardEvent.Target = event.Target;
                guardEvent.Payload["blocked"] = WAO::BehaviorValue(result.Blocked);
                guardEvent.Payload["parried"] = WAO::BehaviorValue(result.Parried);
                guardEvent.Payload["damage"] = WAO::BehaviorValue(result.Damage);
                guardEvent.Payload["attackKind"] = WAO::BehaviorValue(
                    static_cast<int>(hitbox.AttackKind));
                WAO::BehaviorVM::EmitEvent(scene, guardEvent);
            }
        }

        return result;
    }

} // namespace Wheatear::SideCombatHitResolutionService
