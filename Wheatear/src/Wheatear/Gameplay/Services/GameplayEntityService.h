#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/UUID.h"

namespace Wheatear {

    class Entity;
    class Scene;

} // namespace Wheatear

namespace Wheatear::GameplayEntityService {

    WHEATEAR_API Entity Resolve(Scene* scene, UUID id);
    WHEATEAR_API UUID GetID(Entity entity);
    WHEATEAR_API bool IsSame(Entity entity, UUID id);

} // namespace Wheatear::GameplayEntityService
