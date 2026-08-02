#pragma once

#include "SideCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

#include <glm/glm.hpp>

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::SideCombatTargetService {

    WHEATEAR_API Entity FindNearestAliveEnemy(Scene* scene, const glm::vec3& origin);

} // namespace Wheatear::SideCombatTargetService
