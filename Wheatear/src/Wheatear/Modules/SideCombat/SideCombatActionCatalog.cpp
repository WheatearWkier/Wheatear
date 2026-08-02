#include "wtpch.h"
#include "SideCombatActionCatalog.h"

#include "Wheatear/Gameplay/Action/ActionDatabase.h"
#include "Wheatear/Gameplay/Action/StateRegistry.h"

#include <algorithm>
#include <cmath>

namespace Wheatear::SideCombatActionCatalog {

    namespace {

        WAO::EffectSpec DamageEffect(float attackScale, float flatDamage)
        {
            WAO::EffectSpec effect;
            effect.Type = WAO::EffectType::Damage;
            effect.AttributeId = "attack";
            effect.Value = attackScale * 100.0f + flatDamage;
            return effect;
        }

        WAO::EffectSpec LaunchEffect(const glm::vec2& launchVelocity, float hitStun)
        {
            WAO::EffectSpec effect;
            effect.Type = WAO::EffectType::Launch;
            effect.Value = launchVelocity.y;
            effect.Seconds = hitStun;
            return effect;
        }

        WAO::EffectSpec StateEffect(const std::string& id, int turns, float power)
        {
            WAO::EffectSpec effect;
            effect.Type = WAO::EffectType::AddState;
            effect.StateId = id;
            effect.Turns = turns;
            effect.Value = power;
            return effect;
        }

        const char* KindTag(SideAttackKind kind)
        {
            switch (kind)
            {
            case SideAttackKind::Basic: return "Attack.Basic";
            case SideAttackKind::Launcher: return "Attack.Launcher";
            case SideAttackKind::MagicBolt: return "Attack.Magic";
            case SideAttackKind::AllySupport: return "Attack.Support";
            case SideAttackKind::BreakLimit: return "Attack.BreakLimit";
            case SideAttackKind::EnemyProjectile: return "Attack.EnemyProjectile";
            case SideAttackKind::EnemyShockwave: return "Attack.EnemyShockwave";
            case SideAttackKind::EnemyMelee: return "Attack.EnemyMelee";
            default: return "Attack.Unknown";
            }
        }

        std::string IconPathFor(const std::string& attackId, SideAttackKind kind)
        {
            if (attackId == "launcher" || attackId == "air_chase")
                return "assets/vertical_slice/side_combat/ui/icons/skill_launcher.png";
            if (attackId == "magic_bolt")
                return "assets/vertical_slice/side_combat/ui/icons/skill_magic.png";
            if (attackId == "ally_support")
                return "assets/vertical_slice/side_combat/ui/icons/skill_support.png";
            if (attackId == "break_limit")
                return "assets/vertical_slice/side_combat/ui/icons/skill_break_limit.png";
            if (kind == SideAttackKind::EnemyShockwave)
                return "assets/vertical_slice/side_combat/ui/icons/enemy_shockwave.png";
            if (kind == SideAttackKind::EnemyMelee)
                return "assets/vertical_slice/side_combat/ui/icons/enemy_claw.png";
            return "assets/vertical_slice/side_combat/ui/icons/skill_basic.png";
        }

        std::string AnimationIdFor(const std::string& attackId)
        {
            return "side_" + attackId;
        }

        float ActionDurationFor(const SideCombatTuningService::SideAttackTuning& attack,
            SideAttackKind kind)
        {
            if (kind == SideAttackKind::MagicBolt && std::abs(attack.Velocity.x) > 0.001f)
            {
                return std::max({
                    attack.Startup + attack.Recovery + 0.10f,
                    attack.CancelWindowEnd,
                    attack.Startup + 0.16f
                });
            }

            return std::max(0.01f, attack.Startup + attack.Lifetime + attack.Recovery);
        }

        WAO::ActionRecipe MakeRecipe(const std::string& recipeId,
            const std::string& attackId,
            const std::string& displayName,
            const std::string& description,
            const SideCombatTuningService::SideAttackTuning& attack,
            SideAttackKind kind,
            float cooldown,
            float resourceCost)
        {
            WAO::ActionRecipe recipe;
            recipe.Id = recipeId;
            recipe.DisplayName = displayName.empty() ? attackId : displayName;
            recipe.Description = description;
            recipe.IconPath = IconPathFor(attackId, kind);
            recipe.AnimationId = AnimationIdFor(attackId);
            recipe.SoundPath = attack.SwingSound;
            recipe.EffectPath = attack.TextureFramePattern;
            recipe.Cooldown = std::max(0.0f, cooldown);
            recipe.Duration = ActionDurationFor(attack, kind);
            recipe.Startup = attack.Startup;
            recipe.Recovery = attack.Recovery;
            recipe.HitTime = attack.Startup;
            recipe.CancelStart = attack.CancelWindowStart;
            recipe.CancelEnd = attack.CancelWindowEnd;
            recipe.MovementScale = attack.MovementScale;
            recipe.Tags = { "Gameplay.SideCombat", "Gameplay.Combat", KindTag(kind) };
            if (resourceCost > 0.0f)
                recipe.ResourceCost["magic_sword"] = resourceCost;

            recipe.Effects.push_back(DamageEffect(attack.DamageScale, attack.DamageFlat));
            if (attack.LaunchVelocity.y > 0.0f)
                recipe.Effects.push_back(LaunchEffect(attack.LaunchVelocity, attack.HitStun));
            if (kind == SideAttackKind::BreakLimit)
                recipe.Effects.push_back(StateEffect(WAO::StateIds::Stun, 1, 0.25f));
            recipe.Signals = { "side.action.start", "side.action.hit" };
            return recipe;
        }

        void RegisterAnchor(const std::string& attackId,
            const std::string& displayName,
            const std::string& description,
            SideAttackKind kind,
            float cooldown)
        {
            SideCombatTuningService::SideAttackTuning attack;
            attack.Startup = 0.05f;
            attack.Lifetime = 0.14f;
            attack.Recovery = 0.12f;
            attack.MovementScale = 0.8f;
            WAO::ActionDatabase::Register(MakeRecipe(ActionRecipeId(attackId),
                attackId,
                displayName,
                description,
                attack,
                kind,
                cooldown,
                0.0f));
        }

    } // namespace

    std::string ActionRecipeId(const std::string& attackId)
    {
        return "side." + attackId;
    }

    WAO::ActionRecipe BuildActionRecipe(const std::string& attackId,
        const SideCombatTuningService::SideAttackTuning& attack,
        SideAttackKind kind,
        const std::string& displayName,
        const std::string& description,
        float cooldown,
        float resourceCost)
    {
        return MakeRecipe(ActionRecipeId(attackId),
            attackId,
            displayName,
            description,
            attack,
            kind,
            cooldown,
            resourceCost);
    }

    void RegisterActionRecipes()
    {
        RegisterAnchor("basic1", "Basic Slash I", "First ground-chain slash.", SideAttackKind::Basic, 0.19f);
        RegisterAnchor("basic2", "Basic Slash II", "Second ground-chain slash.", SideAttackKind::Basic, 0.19f);
        RegisterAnchor("basic3", "Basic Finisher", "Ground-chain finisher with stronger lift.", SideAttackKind::Basic, 0.34f);
        RegisterAnchor("air_basic", "Air Slash", "Air combo filler with hang-time support.", SideAttackKind::Basic, 0.14f);
        RegisterAnchor("launcher", "Launcher", "Ground opener that sends targets upward.", SideAttackKind::Launcher, 0.42f);
        RegisterAnchor("air_chase", "Air Chase", "Air relaunch tool for combo extension.", SideAttackKind::Launcher, 0.24f);
        RegisterAnchor("magic_bolt", "Magic Bolt", "Ranged magic hit used inside combo routes.", SideAttackKind::MagicBolt, 0.64f);
        RegisterAnchor("ally_support", "Ally Support", "Partner assist hit with high control value.", SideAttackKind::AllySupport, 4.8f);
        RegisterAnchor("break_limit", "Break Limit Chase", "Advanced reset that extends air combo resources.", SideAttackKind::BreakLimit, 3.0f);
        RegisterAnchor("enemy_claw", "Enemy Claw", "Basic enemy melee action.", SideAttackKind::EnemyMelee, 1.2f);
        RegisterAnchor("bear_charge", "Bear Charge", "Boss charge action.", SideAttackKind::EnemyMelee, 1.2f);
        RegisterAnchor("bear_shockwave", "Bear Shockwave", "Boss ranged shockwave action.", SideAttackKind::EnemyShockwave, 1.2f);
    }

} // namespace Wheatear::SideCombatActionCatalog
