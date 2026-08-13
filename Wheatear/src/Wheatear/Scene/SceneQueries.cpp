#include "wtpch.h"
#include "SceneQueries.h"

#include "Components.h"
#include "Entity.h"
#include "Scene.h"

#include <string>

namespace Wheatear::SceneQueries {

    Entity FindEntityByName(Scene* scene, const std::string& name)
    {
        if (!scene || name.empty())
            return {};
        return scene->GetEntityByName(name);
    }

    Entity FindEntityByUUID(Scene* scene, UUID uuid)
    {
        if (!scene || static_cast<uint64_t>(uuid) == 0)
            return {};
        return scene->FindEntityByUUID(uuid);
    }

    bool HasEntity(Scene* scene, const std::string& name)
    {
        return static_cast<bool>(FindEntityByName(scene, name));
    }

} // namespace Wheatear::SceneQueries
