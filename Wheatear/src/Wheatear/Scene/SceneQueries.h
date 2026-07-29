#pragma once

#include "Wheatear/Core/UUID.h"

#include <string>

namespace Wheatear {

    class Entity;
    class Scene;

} // namespace Wheatear

namespace Wheatear::SceneQueries {

    Entity FindEntityByName(Scene* scene, const std::string& name);
    Entity FindEntityByUUID(Scene* scene, UUID uuid);
    bool HasEntity(Scene* scene, const std::string& name);

} // namespace Wheatear::SceneQueries
