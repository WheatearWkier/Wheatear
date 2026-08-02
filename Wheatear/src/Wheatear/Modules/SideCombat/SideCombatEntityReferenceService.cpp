#include "wtpch.h"
#include "SideCombatEntityReferenceService.h"

#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/SceneQueries.h"

namespace Wheatear::SideCombatEntityReferenceService {

    namespace {

        static Entity ResolveCached(Scene* scene, UUID& cached, const std::string& authorName)
        {
            if (!scene)
                return {};

            if (static_cast<uint64_t>(cached) != 0)
            {
                Entity entity = SceneQueries::FindEntityByUUID(scene, cached);
                if (entity)
                    return entity;
                cached = 0;
            }

            Entity entity = SceneQueries::FindEntityByName(scene, authorName);
            cached = entity ? entity.GetUUID() : UUID(0);
            return entity;
        }

    } // namespace

    void RefreshLevelReferences(Scene* scene, SideCombatLevelComponent& level)
    {
        ResolvePlayer(scene, level);
        ResolveBoss(scene, level);
    }

    Entity ResolvePlayer(Scene* scene, SideCombatLevelComponent& level)
    {
        return ResolveCached(scene, level.RuntimePlayerEntity, level.PlayerEntityName);
    }

    Entity ResolveBoss(Scene* scene, SideCombatLevelComponent& level)
    {
        return ResolveCached(scene, level.RuntimeBossEntity, level.BossEntityName);
    }

} // namespace Wheatear::SideCombatEntityReferenceService
