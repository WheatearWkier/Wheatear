#pragma once

#include "ISystem.h"

namespace Wheatear {

    class Scene;
    struct TerrainComponent;

    // Builds/rebuilds the render mesh for every TerrainComponent in the scene.
    class TerrainSystem : public ISystem
    {
    public:
        void OnRuntimeStart(Scene* scene) override;
        void OnEditorStart(Scene* scene) override;
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
        void OnUpdateEditor(Scene* scene, Timestep ts) override;

        static bool BuildTerrain(Scene* scene, TerrainComponent& terrain);
    };

} // namespace Wheatear
