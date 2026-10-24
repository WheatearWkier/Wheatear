#pragma once
#include "Wheatear/Scene/Entity.h"

namespace Wheatear {
    void DrawMeshRendererComponent(Entity entity);
    void DrawDirectionalLightComponent(Entity entity);
    void DrawPointLightComponent(Entity entity);
    void DrawTerrainComponent(Entity entity);
    void DrawNavObstacleComponent(Entity entity);
} // namespace Wheatear