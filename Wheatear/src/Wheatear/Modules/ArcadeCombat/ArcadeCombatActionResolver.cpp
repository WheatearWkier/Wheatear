#include "wtpch.h"
#include "ArcadeCombatActionResolver.h"

#include "ArcadeCombatSignalHandlers.h"
#include "ArcadeCombatComponents.h"
#include "Wheatear/Gameplay/Action/ActionRecipeQueries.h"
#include "Wheatear/Gameplay/Action/ActionResolver.h"
#include "Wheatear/Gameplay/Action/ActionSignalRouter.h"
#include "Wheatear/Modules/Common/GameplayAudioService.h"

namespace Wheatear::ArcadeCombatActionResolver {

    namespace {

        void Record(WAO::EffectLedger& ledger,
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

        float ProjectileSoundVolume(const ArcadeCombatSignalHandlers::ProjectileSpawnPayload& payload)
        {
            if (payload.Melee)
                return 0.48f;
            if (payload.Heavy)
                return 0.46f;
            if (payload.Team == (int)ArcadeTeam::Enemy)
                return 0.36f;
            return 0.34f;
        }

        WAO::ActionResolveResult ResolveProjectileAction(const WAO::ActionResolveContext& context,
            const WAO::ActionRecipe& recipe)
        {
            WAO::ActionResolveResult result;
            result.Handled = true;
            result.Success = false;
            result.Detail = context.Detail.empty() ? "Arcade projectile action" : context.Detail;
            result.Ledger.BeginAction(context.Intent);

            const auto* payload = static_cast<const ArcadeCombatSignalHandlers::ProjectileSpawnPayload*>(
                context.TransientPayload);
            if (!payload || !payload->SceneContext)
            {
                Record(result.Ledger,
                    context.Intent,
                    WAO::EffectType::None,
                    "Projectile payload missing",
                    0.0f,
                    false);
                return result;
            }

            const float damage = payload->Damage > 0.0f
                ? payload->Damage
                : WAO::PrimaryEffectValue(recipe, WAO::EffectType::Damage, 0.0f);
            Record(result.Ledger,
                context.Intent,
                WAO::EffectType::Damage,
                "Projectile carries damage",
                damage,
                true);

            GameplayAudioService::PlaySFX(recipe.SoundPath, ProjectileSoundVolume(*payload));

            for (const std::string& signal : recipe.Signals)
            {
                WAO::ActionSignalContext signalContext;
                signalContext.Intent = context.Intent;
                signalContext.ActionId = recipe.Id;
                signalContext.SignalId = signal;
                signalContext.Source = context.Intent.Source;
                signalContext.Detail = result.Detail;
                signalContext.TransientPayload = context.TransientPayload;
                WAO::ActionSignalRouter::Emit(signalContext);
                Record(result.Ledger,
                    context.Intent,
                    WAO::EffectType::EmitSignal,
                    "Emit " + signal,
                    0.0f,
                    true);
            }

            result.Success = true;
            return result;
        }

    } // namespace

    void RegisterResolver()
    {
        WAO::ActionResolverRegistry::RegisterResolver("arcade.", ResolveProjectileAction);
    }

} // namespace Wheatear::ArcadeCombatActionResolver
