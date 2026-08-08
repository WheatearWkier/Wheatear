#pragma once
#include "ISystem.h"
#include "Wheatear/Renderer/EditorCamera.h"

namespace Wheatear {

    class RenderSystem : public ISystem
    {
    public:
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
        void RenderWithEditorCamera(Scene* scene,
            EditorCamera& camera,
            bool includeUI = false);
        void RenderWithSceneCamera(Scene* scene,
            const Camera& camera,
            const glm::mat4& cameraTransform,
            bool includeUI);

    private:
        glm::mat4 ComputeLightSpaceMatrix(Scene* scene, bool respectEditorVisibility);
        void RenderSceneShadow(Scene* scene, bool respectEditorVisibility);
        void CollectLights(Scene* scene, bool respectEditorVisibility);
        void RenderScene2D(Scene* scene, bool respectEditorVisibility);
        void RenderScene3D(Scene* scene, bool respectEditorVisibility);
    };

} // namespace Wheatear
