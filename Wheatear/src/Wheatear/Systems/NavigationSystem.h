#pragma once

#include "ISystem.h"

namespace Wheatear {

    // Builds the scene nav grid from TerrainComponent + NavObstacleComponent
    // entities and renders the optional debug overlay (F3).
    class NavigationSystem : public ISystem
    {
    public:
        void OnRuntimeStart(Scene* scene) override;
        void OnEditorStart(Scene* scene) override;
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
        void OnUpdateEditor(Scene* scene, Timestep ts) override;

        static bool Rebuild(Scene* scene);

    private:
        void DrawDebug(Scene* scene);

        bool m_F3DownLast = false;
    };

} // namespace Wheatear
