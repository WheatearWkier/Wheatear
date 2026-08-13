#include "wtpch.h"
#include "SideCombatActionService.h"

#include "Wheatear/Gameplay/Action/ActionRecipeQueries.h"
#include "Wheatear/Gameplay/Action/ActionResolver.h"
#include "Wheatear/Gameplay/Services/GameplayAudioService.h"

#include <algorithm>
#include <cmath>

namespace Wheatear::SideCombatActionService {

    namespace {

        static void PlaySfx(const std::string& path, float volume)
        {
            if (!path.empty())
                GameplayAudioService::PlaySFX(path, volume);
        }

        static const WAO::ActionRecipe* FindRecipe(const std::string& recipeId)
        {
            return recipeId.empty() ? nullptr : WAO::FindRecipeOrWarn(recipeId, "SideCombat");
        }

        static float ResolveRecipeDuration(const WAO::ActionRecipe* recipe, float fallback)
        {
            if (!recipe || recipe->Duration <= 0.0f)
                return fallback;
            return std::max(0.01f, recipe->Duration);
        }

        static float ResolveRecipeHitTime(const WAO::ActionRecipe* recipe, float fallback, float duration)
        {
            const float hitTime = recipe && recipe->HitTime > 0.0f ? recipe->HitTime : fallback;
            return std::clamp(hitTime, 0.0f, duration);
        }

        static float ResolveRecipeMovementScale(const WAO::ActionRecipe* recipe, float fallback)
        {
            if (!recipe || recipe->MovementScale <= 0.0f)
                return fallback;

            if (std::abs(recipe->MovementScale - 1.0f) <= 0.0001f)
                return fallback;

            return recipe->MovementScale;
        }

        static void ResolveRecipeAction(const WAO::ActionRecipe* recipe,
            const std::string& source,
            const std::string& detail)
        {
            if (!recipe)
                return;

            WAO::ActionResolveContext context;
            context.Intent.ActionId = recipe->Id;
            context.Intent.Source = source;
            context.Detail = detail;
            WAO::ActionOrchestrator::ExecuteWithRecipe(context, *recipe);
        }

    } // namespace

    float GetActionDuration(const SideCombatTuningService::SideAttackTuning& attack)
    {
        return std::max(0.0f, attack.Startup) +
            std::max(0.01f, attack.Lifetime) +
            std::max(0.0f, attack.Recovery);
    }

    bool IsPlayerActionActive(const SidePlayerControllerComponent& controller)
    {
        return !controller.RuntimeActionAttackId.empty() &&
            controller.RuntimeActionTimer < controller.RuntimeActionDuration;
    }

    bool CanStartPlayerAction(const SidePlayerControllerComponent& controller)
    {
        if (!IsPlayerActionActive(controller))
            return true;

        return controller.RuntimeActionTimer >= controller.RuntimeActionCancelStart &&
            controller.RuntimeActionTimer <= controller.RuntimeActionCancelEnd;
    }

    float GetPlayerActionMovementScale(const SidePlayerControllerComponent& controller)
    {
        return IsPlayerActionActive(controller)
            ? std::clamp(controller.RuntimeActionMovementScale, 0.0f, 1.0f)
            : 1.0f;
    }

    void ClearPlayerAction(SidePlayerControllerComponent& controller)
    {
        controller.RuntimeActionAttackId.clear();
        controller.RuntimeActionRecipeId.clear();
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

    void BeginPlayerAction(SidePlayerControllerComponent& controller,
        const SideCombatTuningService::SideAttackTuning& attack,
        const std::string& attackId,
        const std::string& recipeId,
        const std::string& entityName,
        SideAttackKind kind)
    {
        const WAO::ActionRecipe* recipe = FindRecipe(recipeId);
        float duration = ResolveRecipeDuration(recipe, GetActionDuration(attack));
        if (!recipe && kind == SideAttackKind::MagicBolt && std::abs(attack.Velocity.x) > 0.001f)
        {
            duration = std::max({
                attack.Startup + attack.Recovery + 0.10f,
                attack.CancelWindowEnd,
                attack.Startup + 0.16f
            });
        }
        const float recipeCancelStart = recipe && recipe->CancelStart > 0.0f
            ? recipe->CancelStart
            : attack.CancelWindowStart;
        const float recipeCancelEnd = recipe && recipe->CancelEnd > 0.0f
            ? recipe->CancelEnd
            : attack.CancelWindowEnd;
        float cancelStart = std::clamp(recipeCancelStart, 0.0f, duration);
        float cancelEnd = recipeCancelEnd > 0.0f
            ? std::clamp(recipeCancelEnd, cancelStart, duration)
            : duration;
        if (recipeCancelStart <= 0.0f && recipeCancelEnd <= 0.0f)
        {
            cancelStart = duration;
            cancelEnd = duration;
        }

        controller.RuntimeActionAttackId = attackId;
        controller.RuntimeActionRecipeId = recipe ? recipe->Id : (recipeId.empty() ? attackId : recipeId);
        controller.RuntimeActionEntityName = entityName;
        controller.RuntimeActionKind = kind;
        controller.RuntimeActionTimer = 0.0f;
        controller.RuntimeActionDuration = duration;
        controller.RuntimeActionHitboxTime = ResolveRecipeHitTime(recipe, attack.Startup, duration);
        controller.RuntimeActionCancelStart = cancelStart;
        controller.RuntimeActionCancelEnd = cancelEnd;
        controller.RuntimeActionMovementScale = ResolveRecipeMovementScale(recipe, attack.MovementScale);
        controller.RuntimeActionHitboxSpawned = false;
        if (++controller.RuntimeActionSequence == 0)
            ++controller.RuntimeActionSequence;
        PlaySfx(recipe && !recipe->SoundPath.empty() ? recipe->SoundPath : attack.SwingSound, attack.SoundVolume);
        ResolveRecipeAction(recipe, "SideCombat.Player", "start " + entityName);
    }

    bool IsEnemyActionActive(const SideEnemyAIComponent& ai)
    {
        return !ai.RuntimeActionAttackId.empty() &&
            ai.RuntimeActionTimer < ai.RuntimeActionDuration;
    }

    void ClearEnemyAction(SideEnemyAIComponent& ai)
    {
        ai.RuntimeActionAttackId.clear();
        ai.RuntimeActionRecipeId.clear();
        ai.RuntimeActionEntityName.clear();
        ai.RuntimeActionKind = SideAttackKind::EnemyMelee;
        ai.RuntimeActionTimer = 0.0f;
        ai.RuntimeActionDuration = 0.0f;
        ai.RuntimeActionHitboxTime = 0.0f;
        ai.RuntimeActionMovementScale = 1.0f;
        ai.RuntimeActionFacing = 1.0f;
        ai.RuntimeActionHitboxSpawned = false;
    }

    void BeginEnemyAction(SideEnemyAIComponent& ai,
        const SideCombatTuningService::SideAttackTuning& attack,
        const std::string& attackId,
        const std::string& recipeId,
        const std::string& entityName,
        SideAttackKind kind,
        float facing)
    {
        const WAO::ActionRecipe* recipe = FindRecipe(recipeId);
        ai.RuntimeActionAttackId = attackId;
        ai.RuntimeActionRecipeId = recipe ? recipe->Id : (recipeId.empty() ? attackId : recipeId);
        ai.RuntimeActionEntityName = entityName;
        ai.RuntimeActionKind = kind;
        ai.RuntimeActionTimer = 0.0f;
        ai.RuntimeActionDuration = ResolveRecipeDuration(recipe, GetActionDuration(attack));
        ai.RuntimeActionHitboxTime = ResolveRecipeHitTime(recipe, attack.Startup, ai.RuntimeActionDuration);
        ai.RuntimeActionMovementScale = ResolveRecipeMovementScale(recipe, attack.MovementScale);
        ai.RuntimeActionFacing = facing;
        ai.RuntimeActionHitboxSpawned = false;
        if (++ai.RuntimeActionSequence == 0)
            ++ai.RuntimeActionSequence;
        PlaySfx(recipe && !recipe->SoundPath.empty() ? recipe->SoundPath : attack.SwingSound, attack.SoundVolume);
        ResolveRecipeAction(recipe, "SideCombat.Enemy", "start " + entityName);
    }

} // namespace Wheatear::SideCombatActionService
