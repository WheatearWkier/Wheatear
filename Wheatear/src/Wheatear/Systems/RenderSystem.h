#pragma once
#include "ISystem.h"
#include "Wheatear/Renderer/EditorCamera.h"

#include "entt.hpp"
#include <glm/glm.hpp>
#include <vector>

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

        // Reused across frames to avoid per-frame heap allocation for the
        // back-to-front sprite sort.
        struct SpriteSortItem
        {
            entt::entity Entity = entt::null;
            glm::mat4 World = glm::mat4(1.0f);
            int SortingLayer = 0;
            int OrderInLayer = 0;
            bool IsCircle = false;
        };
        std::vector<SpriteSortItem> m_SpriteSortBuffer;
    };

} // namespace Wheatear
