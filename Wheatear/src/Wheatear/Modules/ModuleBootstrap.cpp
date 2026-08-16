#include "wtpch.h"
#include "ModuleBootstrap.h"

#include "Wheatear/Modules/ArcadeCombat/ArcadeCombatActionResolver.h"
#include "Wheatear/Modules/ArcadeCombat/ArcadeCombatSignalHandlers.h"
#include "Wheatear/Modules/ArcadeCombat/ArcadeCombatSystem.h"
#include "Wheatear/Modules/Progression/ProgressionSystem.h"
#include "Wheatear/Modules/SideCombat/SideCombatComponents.h"
#include "Wheatear/Modules/SideCombat/SideCombatFeedbackService.h"
#include "Wheatear/Modules/SideCombat/SideCombatSkillRegistry.h"
#include "Wheatear/Modules/SideCombat/SideCombatSystem.h"
#include "Wheatear/Modules/SideCombat/SideCombatTuningService.h"
#include "Wheatear/Modules/TacticalCombat/TacticalCombatSystem.h"
#include "Wheatear/Modules/TurnCombat/TurnCombatSystem.h"
#include "Wheatear/Modules/VisualNovel/VisualNovelSystem.h"
#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Gameplay/Action/ActionAssetLoader.h"
#include "Wheatear/Gameplay/Action/ActionResolver.h"
#include "Wheatear/Gameplay/Action/EffectRegistry.h"
#include "Wheatear/Runtime/CommandBus.h"
#include "Wheatear/Scene/SceneSystemRegistry.h"
#include "Wheatear/Scripting/EventScriptSystem.h"

namespace Wheatear {

    namespace {

        // Example custom effect semantics. These are *registrations*, not
        // enum entries: any new behaviour authored as data can be added the
        // same way, then used from the WAO Action Editor
        // (effects: - type: Damage, customType: lifesteal, value: 0.3).
        void RegisterExampleCustomEffects()
        {
            WAO::EffectRegistry::Register("lifesteal", "吸血 (按攻击比例回血)",
                [](WAO::CustomEffectContext& context)
                {
                    if (!context.Vars)
                        return false;
                    const float attack = context.Vars->Get("source.attack", 0.0f);
                    const float heal = attack * std::max(0.0f, context.InValue);
                    if (heal <= 0.0f)
                        return false;
                    const float health = context.Vars->Get("target.health", 0.0f);
                    const float maxHealth = context.Vars->Get("target.max_health", 0.0f);
                    if (health >= maxHealth - 0.001f)
                        return false;
                    context.Vars->Set("target.health", std::min(maxHealth, health + heal));
                    context.OutValue = heal;
                    return true;
                });

            WAO::EffectRegistry::Register("mana_leech", "吸蓝 (按攻击比例回蓝)",
                [](WAO::CustomEffectContext& context)
                {
                    if (!context.Vars)
                        return false;
                    const float attack = context.Vars->Get("source.attack", 0.0f);
                    const float gain = attack * std::max(0.0f, context.InValue);
                    if (gain <= 0.0f)
                        return false;
                    const float mana = context.Vars->Get("controller.mana", 0.0f);
                    const float maxMana = context.Vars->Get("controller.max_mana", 1.0f);
                    if (mana >= maxMana - 0.001f)
                        return false;
                    context.Vars->Set("controller.mana", std::min(maxMana, mana + gain));
                    context.OutValue = gain;
                    return true;
                });

            WAO::EffectRegistry::Register("full_heal", "满血恢复",
                [](WAO::CustomEffectContext& context)
                {
                    if (!context.Vars)
                        return false;
                    const float health = context.Vars->Get("target.health", 0.0f);
                    const float maxHealth = context.Vars->Get("target.max_health", 0.0f);
                    if (health >= maxHealth - 0.001f)
                        return false;
                    context.Vars->Set("target.health", maxHealth);
                    context.OutValue = maxHealth - health;
                    return true;
                });
        }

        // Example custom skill behaviour: a self-damaging berserk that trades
        // health for an attack buff. One registration makes it selectable in
        // the editor's Skill Slots tab (kind: custom, customBehavior: berserk).
        void RegisterExampleSkillBehaviors()
        {
            SideCombatSkillRegistry::Register("berserk", "狂化 (扣血换攻击增益)",
                [](SideCombatSkillRegistry::SkillBehaviorContext& context)
                {
                    if (!context.Combatant || !context.Controller || !context.Level)
                        return;

                    auto& combatant = *context.Combatant;
                    auto& controller = *context.Controller;

                    // Cost: 15% of max health (skip when it would kill).
                    const float cost = combatant.MaxHealth * 0.15f;
                    if (combatant.Health <= cost + 1.0f)
                        return;

                    combatant.Health -= cost;
                    controller.RuntimeAttackBuffMultiplier = std::max(2.0f, controller.AttackBuffMultiplier);
                    controller.RuntimeAttackBuffTimer = 6.0f;
                    SideCombatFeedbackService::PlaySfx(
                        SideCombatTuningService::GetTuning(*context.Level).Feedback.JumpSound, 0.5f);
                });
        }

    } // namespace

    void RegisterDefaultGameplayModules()
    {
        CommandBus::RegisterGameplayCommandPrefix("vn:");
        CommandBus::RegisterGameplayCommandPrefix("gamesave:");
        CommandBus::RegisterGameplayCommandPrefix("turn:");
        CommandBus::RegisterGameplayCommandPrefix("tactic:");
        CommandBus::RegisterGameplayCommandPrefix("side:");

        ArcadeCombatSignalHandlers::RegisterHandlers();
        ArcadeCombatActionResolver::RegisterResolver();
        WAO::RegisterRecipePreviewResolver("side.", "Side action recipe resolved");
        WAO::RegisterRecipePreviewResolver("turn.", "Turn action recipe resolved");
        WAO::RegisterRecipePreviewResolver("tactical.", "Tactical action recipe resolved");
        RegisterExampleCustomEffects();
        RegisterExampleSkillBehaviors();
        AssetAliasRegistry::Load();
        const size_t actionRecipeCount = WAO::ActionAssetLoader::LoadManifest(
            AssetAliasRegistry::Path("wao.action_sets", "assets/gameplay/actions/action_sets.yaml"));
        if (actionRecipeCount == 0)
        {
            WAO::ActionAssetLoader::LoadDirectory(
                AssetAliasRegistry::Path("wao.action_directory", "assets/gameplay/actions"));
        }

        SceneSystemRegistry::RegisterRuntimeSystem(
            "VisualNovel",
            []() -> Scope<ISystem> { return CreateScope<VisualNovelSystem>(); });

        SceneSystemRegistry::RegisterRuntimeSystem(
            "ArcadeCombat",
            []() -> Scope<ISystem> { return CreateScope<ArcadeCombatSystem>(); });

        SceneSystemRegistry::RegisterRuntimeSystem(
            "SideCombat",
            []() -> Scope<ISystem> { return CreateScope<SideCombatSystem>(); });

        SceneSystemRegistry::RegisterRuntimeSystem(
            "TurnCombat",
            []() -> Scope<ISystem> { return CreateScope<TurnCombatSystem>(); });

        SceneSystemRegistry::RegisterRuntimeSystem(
            "TacticalCombat",
            []() -> Scope<ISystem> { return CreateScope<TacticalCombatSystem>(); });

        SceneSystemRegistry::RegisterRuntimeSystem(
            "Progression",
            []() -> Scope<ISystem> { return CreateScope<ProgressionSystem>(); });

        SceneSystemRegistry::RegisterRuntimeSystem(
            "EventScript",
            []() -> Scope<ISystem> { return CreateScope<EventScriptSystem>(); });
    }

} // namespace Wheatear
