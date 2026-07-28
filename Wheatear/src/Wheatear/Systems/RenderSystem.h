#pragma once
#include "ISystem.h"
#include "Wheatear/Renderer/EditorCamera.h"

namespace Wheatear {

    class RenderSystem : public ISystem
    {
    public:
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
        void RenderWithEditorCamera(Scene* scene, EditorCamera& camera);

    private:
        glm::mat4 ComputeLightSpaceMatrix(Scene* scene);
        void RenderSceneShadow(Scene* scene);
        void CollectLights(Scene* scene);
        void RenderScene2D(Scene* scene);
        void RenderScene3D(Scene* scene);
    };

} // namespace Wheatear