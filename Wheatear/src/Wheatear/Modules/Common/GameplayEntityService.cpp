#include "wtpch.h"
#include "GameplayEntityService.h"

#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"

namespace Wheatear::GameplayEntityService {

    Entity Resolve(Scene* scene, UUID id)
    {
        if (!scene || static_cast<uint64_t>(id) == 0)
            return {};
        return scene->GetEntityByUUID(id);
    }

    UUID GetID(Entity entity)
    {
        return entity ? entity.GetUUID() : UUID(0);
    }

    bool IsSame(Entity entity, UUID id)
    {
        return entity && entity.GetUUID() == id;
    }

} // namespace Wheatear::GameplayEntityService
