#include "wtpch.h"
#include "ArcadeCombatSignalHandlers.h"

#include "ArcadeCombatProjectileService.h"
#include "Wheatear/Gameplay/Action/ActionSignalRouter.h"

namespace Wheatear::ArcadeCombatSignalHandlers {

    namespace {

        bool& Registered()
        {
            static bool registered = false;
            return registered;
        }

        void HandleProjectileSpawn(const WAO::ActionSignalContext& context)
        {
            const auto* payload = static_cast<const ProjectileSpawnPayload*>(context.TransientPayload);
            if (!payload || !payload->SceneContext)
                return;

            ArcadeCombatProjectileService::CreateProjectile(payload->SceneContext,
                payload->EntityName,
                payload->Position,
                payload->Velocity,
                payload->Damage,
                payload->Lifetime,
                payload->Radius,
                payload->Team,
                payload->Color,
                payload->Heavy,
                payload->Melee);
        }

    } // namespace

    void RegisterHandlers()
    {
        if (Registered())
            return;

        WAO::ActionSignalRouter::RegisterHandler(SpawnProjectileSignal, HandleProjectileSpawn);
        Registered() = true;
    }

    void EmitProjectileSpawn(const std::string& actionId,
        const std::string& source,
        const std::string& detail,
        const ProjectileSpawnPayload& payload)
    {
        WAO::ActionSignalContext context;
        context.Intent.ActionId = actionId;
        context.Intent.Source = source;
        context.ActionId = actionId;
        context.SignalId = SpawnProjectileSignal;
        context.Source = source;
        context.Detail = detail;
        context.TransientPayload = &payload;
        WAO::ActionSignalRouter::Emit(context);
    }

} // namespace Wheatear::ArcadeCombatSignalHandlers
