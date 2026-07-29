#include "wtpch.h"
#include "SceneQueries.h"

#include "Components.h"
#include "Entity.h"
#include "Scene.h"

namespace Wheatear::SceneQueries {

    Entity FindEntityByName(Scene* scene, const std::string& name)
    {
        if (!scene || name.empty())
            return {};

        auto& registry = scene->GetRegistry();
        for (auto entity : registry.view<TagComponent>())
        {
            if (registry.get<TagComponent>(entity).Tag == name)
                return { entity, scene };
        }
        return {};
    }

    Entity FindEntityByUUID(Scene* scene, UUID uuid)
    {
        if (!scene || static_cast<uint64_t>(uuid) == 0)
            return {};

        auto& registry = scene->GetRegistry();
        for (auto entity : registry.view<IDComponent>())
        {
            if (registry.get<IDComponent>(entity).ID == uuid)
                return { entity, scene };
        }
        return {};
    }

    bool HasEntity(Scene* scene, const std::string& name)
    {
        return static_cast<bool>(FindEntityByName(scene, name));
    }

} // namespace Wheatear::SceneQueries
