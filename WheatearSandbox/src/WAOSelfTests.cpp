#include "Wheatear/Gameplay/Action/ActionRunner.h"
#include "Wheatear/Gameplay/Action/EffectFormula.h"
#include "Wheatear/Gameplay/Action/EffectRegistry.h"
#include "Wheatear/Gameplay/Action/StateRegistry.h"
#include "Wheatear/Assets/AssetDatabase.h"
#include "Wheatear/Modules/RTS/RTSComponents.h"
#include "Wheatear/Modules/RTS/RTSUtilities.h"
#include "Wheatear/Navigation/NavGrid.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneSerializer.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>

namespace {

    int g_Failures = 0;

    void Check(bool condition, const char* name)
    {
        if (condition)
        {
            std::cout << "[PASS] " << name << '\n';
            return;
        }

        std::cerr << "[FAIL] " << name << '\n';
        ++g_Failures;
    }

    bool NearlyEqual(float a, float b)
    {
        return std::abs(a - b) <= 0.001f;
    }

    Wheatear::WAO::ActionRuntime MakeRuntime(float health, float maxHealth,
        float mana = 50.0f)
    {
        Wheatear::WAO::ActionRuntime runtime;
        runtime.Attributes.Set("Health", health);
        runtime.Attributes.Set("MaxHealth", maxHealth);
        runtime.Attributes.Set("hp", health);
        runtime.Attributes.Set("max_hp", maxHealth);
        runtime.Attributes.Set("atk", 10.0f);
        runtime.Attributes.Set("attack", 10.0f);
        runtime.Attributes.Set("mana", mana);
        runtime.Resources["mana"] = mana;
        return runtime;
    }

    Wheatear::WAO::ActionIntent MakeIntent(const char* actionId)
    {
        Wheatear::WAO::ActionIntent intent;
        intent.Actor = 1;
        intent.ExplicitTarget = 2;
        intent.ActionId = actionId;
        return intent;
    }

} // namespace

int RunWAOSelfTests()
{
    using namespace Wheatear::WAO;

    // Target-applied damage must not touch the actor.
    {
        ActionRuntime actor = MakeRuntime(100.0f, 100.0f);
        ActionRuntime target = MakeRuntime(100.0f, 100.0f);

        ActionRecipe recipe;
        recipe.Id = "test.damage";
        EffectSpec effect;
        effect.Type = EffectType::Damage;
        effect.AttributeId = "hp";
        effect.Value = 10.0f;
        effect.TargetSelector = EffectTargetSelector::Target;
        recipe.Effects.push_back(effect);

        const ActionIntent intent = MakeIntent("test.damage");
        const ActionExecutionResult result = Execute(
            intent, recipe, actor, &target, &actor);

        Check(result.Success, "damage execution succeeds");
        Check(NearlyEqual(target.Attributes.Get("Health"), 90.0f),
            "damage applies to explicit target Health");
        Check(NearlyEqual(actor.Attributes.Get("Health"), 100.0f),
            "damage does not touch actor Health");
    }

    // Heal clamps to MaxHealth.
    {
        ActionRuntime actor = MakeRuntime(100.0f, 100.0f);
        ActionRuntime target = MakeRuntime(95.0f, 100.0f);

        ActionRecipe recipe;
        recipe.Id = "test.heal";
        EffectSpec effect;
        effect.Type = EffectType::Heal;
        effect.AttributeId = "hp";
        effect.Value = 20.0f;
        effect.TargetSelector = EffectTargetSelector::Target;
        recipe.Effects.push_back(effect);

        Execute(MakeIntent("test.heal"), recipe, actor, &target, &actor);
        Check(NearlyEqual(target.Attributes.Get("Health"), 100.0f),
            "heal clamps to MaxHealth");
    }

    // StartCooldown uses Seconds, not Value.
    {
        ActionRuntime actor = MakeRuntime(100.0f, 100.0f);

        ActionRecipe recipe;
        recipe.Id = "test.cooldown";
        EffectSpec effect;
        effect.Type = EffectType::StartCooldown;
        effect.Seconds = 2.5f;
        effect.Value = 0.0f;
        recipe.Effects.push_back(effect);

        Execute(MakeIntent("test.cooldown"), recipe, actor);
        Check(NearlyEqual(actor.Cooldowns["test.cooldown"], 2.5f),
            "StartCooldown uses Seconds");
    }

    // Seconds-based states are visible to HasState.
    {
        ActionRuntime actor = MakeRuntime(100.0f, 100.0f);

        ActionRecipe recipe;
        recipe.Id = "test.state";
        EffectSpec effect;
        effect.Type = EffectType::AddState;
        effect.StateId = StateIds::Stun;
        effect.DurationPolicy = EffectDurationPolicy::Seconds;
        effect.Seconds = 2.0f;
        recipe.Effects.push_back(effect);

        Execute(MakeIntent("test.state"), recipe, actor);
        Check(!actor.States.empty(), "AddState creates a state");
        Check(!actor.States.empty() && NearlyEqual(actor.States.front().RemainingSeconds, 2.0f),
            "seconds-based state keeps its duration");
        Check(HasState(actor.States, StateIds::Stun),
            "seconds-based state is found by HasState");
    }

    // Infinite states do not expire through HasState.
    {
        ActionRuntime actor = MakeRuntime(100.0f, 100.0f);

        ActionRecipe recipe;
        recipe.Id = "test.infinite";
        EffectSpec effect;
        effect.Type = EffectType::AddState;
        effect.StateId = StateIds::Guard;
        effect.DurationPolicy = EffectDurationPolicy::Infinite;
        recipe.Effects.push_back(effect);

        Execute(MakeIntent("test.infinite"), recipe, actor);
        Check(!actor.States.empty() && actor.States.front().Infinite,
            "infinite state is marked Infinite");
        Check(HasState(actor.States, StateIds::Guard),
            "infinite state is found by HasState");
    }

    // ConsumeResource clamps to zero.
    {
        ActionRuntime actor = MakeRuntime(100.0f, 100.0f, 5.0f);

        ActionRecipe recipe;
        recipe.Id = "test.resource";
        EffectSpec effect;
        effect.Type = EffectType::ConsumeResource;
        effect.AttributeId = "mana";
        effect.Value = 10.0f;
        recipe.Effects.push_back(effect);

        Execute(MakeIntent("test.resource"), recipe, actor);
        Check(NearlyEqual(actor.Resources["mana"], 0.0f),
            "ConsumeResource clamps at zero");
    }

    // Formula overrides Value and sees target attributes.
    {
        ActionRuntime actor = MakeRuntime(100.0f, 100.0f);
        ActionRuntime target = MakeRuntime(50.0f, 100.0f);

        ActionRecipe recipe;
        recipe.Id = "test.formula";
        EffectSpec effect;
        effect.Type = EffectType::Heal;
        effect.AttributeId = "hp";
        effect.TargetSelector = EffectTargetSelector::Target;
        effect.Formula = "target.health - 45";
        effect.Value = 1.0f;
        recipe.Effects.push_back(effect);

        Execute(MakeIntent("test.formula"), recipe, actor, &target, &actor);
        Check(NearlyEqual(target.Attributes.Get("Health"), 55.0f),
            "Formula evaluates against target attributes");
    }

    // Invalid formulas are rejected by the validator.
    {
        std::string error;
        Check(!ValidateEffectFormula("target.health +", &error),
            "invalid formula is rejected");
        Check(ValidateEffectFormula("min(target.health, 10)"),
            "valid formula is accepted");
    }

    // Custom effects run through the shared registry and write back target state.
    {
        EffectRegistry::Register("test_bonus", "Test Bonus",
            [](CustomEffectContext& context)
            {
                if (!context.Vars)
                    return false;
                const float health = context.Vars->Get("target.health", 0.0f);
                context.Vars->Set("target.health", health + 7.0f);
                context.OutValue = 7.0f;
                return true;
            });

        ActionRuntime actor = MakeRuntime(100.0f, 100.0f);
        ActionRuntime target = MakeRuntime(50.0f, 100.0f);

        ActionRecipe recipe;
        recipe.Id = "test.custom";
        EffectSpec effect;
        effect.Type = EffectType::Damage;
        effect.CustomType = "test_bonus";
        effect.TargetSelector = EffectTargetSelector::Target;
        effect.Value = 1.0f;
        recipe.Effects.push_back(effect);

        Execute(MakeIntent("test.custom"), recipe, actor, &target, &actor);
        Check(NearlyEqual(target.Attributes.Get("Health"), 57.0f),
            "custom effect writes back to target Health");
    }

    // Scene transform hierarchy: local transforms compose through Parent UUIDs,
    // and parenting rejects cycles.
    {
        Wheatear::Scene scene;
        Wheatear::Entity parent = scene.CreateEntity("Parent");
        parent.GetComponent<Wheatear::TransformComponent>().Translation =
            { 10.0f, 0.0f, 0.0f };

        Wheatear::Entity child = scene.CreateEntity("Child");
        auto& childTransform = child.GetComponent<Wheatear::TransformComponent>();
        childTransform.Translation = { 1.0f, 2.0f, 0.0f };
        childTransform.Parent = parent.GetUUID();

        const glm::vec3 world = scene.GetWorldTranslation(child);
        Check(NearlyEqual(world.x, 11.0f) && NearlyEqual(world.y, 2.0f),
            "world transform composes parent translation");

        Check(!scene.SetParent(parent, child),
            "parenting rejects cycles");

        Check(scene.SetParent(child, {}),
            "clearing parent succeeds");
        const glm::vec3 localWorld = scene.GetWorldTranslation(child);
        Check(NearlyEqual(localWorld.x, 1.0f),
            "cleared parent restores local world position");
    }

    // Scene save/load round-trip preserves transform parent links.
    {
        Wheatear::Ref<Wheatear::Scene> scene = Wheatear::CreateRef<Wheatear::Scene>();
        Wheatear::Entity parent = scene->CreateEntity("Parent");
        parent.GetComponent<Wheatear::TransformComponent>().Translation =
            { 5.0f, 0.0f, 0.0f };
        Wheatear::Entity child = scene->CreateEntity("Child");
        auto& childTransform = child.GetComponent<Wheatear::TransformComponent>();
        childTransform.Translation = { 1.0f, 0.0f, 0.0f };
        childTransform.Parent = parent.GetUUID();

        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / "wheatear_scene_roundtrip.wt";
        Wheatear::SceneSerializer writer(scene);
        const bool saved = writer.SerializeYaml(path);
        Check(saved, "scene serialization succeeds");

        Wheatear::Ref<Wheatear::Scene> loaded = Wheatear::CreateRef<Wheatear::Scene>();
        Wheatear::SceneSerializer reader(loaded);
        const bool loadedOk = reader.DeserializeYaml(path);
        Check(loadedOk, "scene deserialization succeeds");

        if (loadedOk)
        {
            Wheatear::Entity loadedChild = loaded->FindEntityByUUID(child.GetUUID());
            Check(loadedChild && loadedChild.HasComponent<Wheatear::TransformComponent>(),
                "round-trip keeps child entity");
            if (loadedChild && loadedChild.HasComponent<Wheatear::TransformComponent>())
            {
                Check(static_cast<uint64_t>(
                        loadedChild.GetComponent<Wheatear::TransformComponent>().Parent)
                        == static_cast<uint64_t>(parent.GetUUID()),
                    "round-trip keeps parent UUID");
                Check(NearlyEqual(loaded->GetWorldTranslation(loadedChild).x, 6.0f),
                    "round-trip keeps world transform");
            }
        }

        std::error_code removeError;
        std::filesystem::remove(path, removeError);
    }

    // Duplicating a parent duplicates its Transform subtree and remaps parents.
    {
        Wheatear::Scene scene;
        Wheatear::Entity parent = scene.CreateEntity("Parent");
        parent.GetComponent<Wheatear::TransformComponent>().Translation =
            { 10.0f, 0.0f, 0.0f };
        Wheatear::Entity child = scene.CreateEntity("Child");
        auto& childTransform = child.GetComponent<Wheatear::TransformComponent>();
        childTransform.Translation = { 1.0f, 0.0f, 0.0f };
        childTransform.Parent = parent.GetUUID();

        Wheatear::Entity duplicate = scene.DuplicateEntity(parent);
        Check(duplicate && duplicate != parent, "duplicate creates a new root");

        const std::vector<Wheatear::Entity> duplicates =
            scene.CollectSubtree(duplicate);
        Check(duplicates.size() == 2, "duplicate copies the whole subtree");

        bool childRemapped = false;
        for (Wheatear::Entity candidate : duplicates)
        {
            if (candidate == duplicate)
                continue;
            const auto& transform =
                candidate.GetComponent<Wheatear::TransformComponent>();
            childRemapped = transform.Parent == duplicate.GetUUID()
                && NearlyEqual(scene.GetWorldTranslation(candidate).x, 11.0f);
        }
        Check(childRemapped, "duplicate remaps child parent and world transform");
    }

    // Economy/production: a constructed barracks can queue a soldier, deducts
    // its cost, and rejects the order when supply is capped.
    {
        Wheatear::Scene scene;
        Wheatear::Entity controller = scene.CreateEntity("Economy");
        auto& match = controller.AddComponent<Wheatear::RTSMatchComponent>();
        match.Credits[0] = 400.0f;
        match.Supply[0] = 0;
        match.SupplyCap[0] = 60;

        Wheatear::Entity barracks = scene.CreateEntity("Barracks");
        auto& unit = barracks.AddComponent<Wheatear::RTSUnitComponent>();
        unit.Team = Wheatear::RTSTeam::Player;
        unit.Type = Wheatear::RTSUnitType::Building;
        unit.BuildingKind = Wheatear::RTSBuildingKind::Barracks;
        unit.HP = 600.0f;
        auto& production =
            barracks.AddComponent<Wheatear::RTSProductionComponent>();
        production.Team = Wheatear::RTSTeam::Player;
        production.Constructed = true;

        Check(Wheatear::RTS::EnqueueUnit(&scene, Wheatear::RTSTeam::Player,
            Wheatear::RTSUnitType::Soldier), "economy queues a soldier");
        Check(production.Queue.size() == 1, "production queue receives order");
        Check(NearlyEqual(match.Credits[0], 350.0f), "soldier cost is deducted");

        match.Supply[0] = match.SupplyCap[0];
        Check(!Wheatear::RTS::EnqueueUnit(&scene, Wheatear::RTSTeam::Player,
            Wheatear::RTSUnitType::Soldier), "production rejects supply cap");
    }

    // Animator state transitions: first clip starts immediately, later clips
    // crossfade through NextClipIndex, and re-requesting the same clip is a
    // no-op.
    {
        Wheatear::Animator3DComponent animator;
        animator.PlayClip(0, 0.2f);
        Check(animator.CurrentClipIndex == 0, "animator starts first clip");
        animator.PlayClip(1, 0.2f);
        Check(animator.NextClipIndex == 1, "animator crossfades to next clip");
        const int next = animator.NextClipIndex;
        animator.PlayClip(1, 0.2f);
        Check(animator.NextClipIndex == next, "animator ignores duplicate play");
    }

    // Navigation: A* must route around a blocked wall instead of crossing it.
    {
        Wheatear::Navigation::NavGrid grid;
        const std::vector<float> heights(21 * 21, 0.0f);
        Check(grid.Build(20, 20, 1.0f, { -10.0f, 0.0f, -10.0f }, heights),
            "nav grid builds");
        grid.BlockWorldAABB({ -1.0f, 0.0f, -5.0f }, { 1.0f, 0.0f, 5.0f }, 0.0f);

        std::vector<glm::vec3> path;
        Check(grid.FindPath({ -5.0f, 0.0f, 0.0f }, { 5.0f, 0.0f, 0.0f }, path),
            "nav path found around obstacle");
        Check(path.size() >= 2, "nav path has waypoints");

        bool crossesObstacle = false;
        for (const glm::vec3& point : path)
        {
            if (point.x > -1.2f && point.x < 1.2f
                && point.z > -5.2f && point.z < 5.2f)
            {
                crossesObstacle = true;
                break;
            }
        }
        Check(!crossesObstacle, "nav path avoids blocked cells");
        Check(glm::length(path.back() - glm::vec3(5.0f, 0.0f, 0.0f)) < 0.1f,
            "nav path reaches destination");

        glm::vec3 insideObstacle(0.0f, 0.0f, 0.0f);
        grid.ResolveCircle(insideObstacle, 0.45f);
        const bool stillInside = insideObstacle.x > -1.2f && insideObstacle.x < 1.2f
            && insideObstacle.z > -5.2f && insideObstacle.z < 5.2f;
        Check(!stillInside && grid.IsWalkableWorld(insideObstacle),
            "nav resolves circle out of obstacle cells");
    }

    // Asset UUIDs resolve to project-relative paths and can be looked up in
    // both directions; path normalization accepts Windows separators.
    {
        const Wheatear::UUID modelId(0x51A7000000000001ull);
        Wheatear::AssetDatabase::Get().SetMapping(modelId, "assets/models/Box.glb");
        Check(Wheatear::AssetDatabase::Get().GetPath(modelId) == "assets/models/Box.glb",
            "asset UUID resolves to its path");
        Check(static_cast<uint64_t>(Wheatear::AssetDatabase::Get().FindByPath(
            "assets\\models\\Box.glb")) == static_cast<uint64_t>(modelId),
            "asset path lookup normalizes separators");
        Check(static_cast<uint64_t>(Wheatear::AssetDatabase::Get().FindByPath(
            "assets/models/Missing.glb")) == 0,
            "unknown asset path has no UUID");
    }

    if (g_Failures == 0)
    {
        std::cout << "WAO self-tests passed.\n";
        return 0;
    }

    std::cerr << g_Failures << " WAO self-test(s) failed.\n";
    return 1;
}
