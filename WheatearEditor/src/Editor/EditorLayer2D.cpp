#include "wtpch.h"
#include "EditorLayer2D.h"

#include "Wheatear/Renderer/Renderer2D.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

#include <imgui/imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Wheatear {

    EditorLayer2D::EditorLayer2D()
        : EditorLayerBase("EditorLayer2D")
    {
    }

    // =========================================================================
    // =========================================================================

    void EditorLayer2D::OnOverlayRender()
    {
        if (GetSceneState() == SceneState::Play)
        {
            Entity camera = GetActiveScene()->GetPrimaryCameraEntity();
            if (!camera) return;

            const auto& camComp = camera.GetComponent<CameraComponent>();
            const auto& camTC   = camera.GetComponent<TransformComponent>();
            Renderer2D::BeginScene(camComp.Camera, camTC.GetTransform());
        }
        else
        {
            Renderer2D::BeginScene(GetEditorCamera());
        }

        if (m_ShowPhysicsColliders)
        {
            for (auto e : GetActiveScene()->GetAllEntitiesWith<TransformComponent, BoxCollider2DComponent>())
            {
                auto [tc, bc] = GetActiveScene()->GetRegistry()
                    .get<TransformComponent, BoxCollider2DComponent>(e);

                const glm::mat4 t =
                    glm::translate(glm::mat4(1.0f), tc.Translation + glm::vec3(bc.Offset, 0.001f))
                    * glm::rotate(glm::mat4(1.0f), tc.Rotation.z, { 0, 0, 1 })
                    * glm::scale(glm::mat4(1.0f), tc.Scale * glm::vec3(bc.Size * 2.0f, 1.0f));

                Renderer2D::DrawRect(t, { 0, 1, 0, 1 });
            }

            for (auto e : GetActiveScene()->GetAllEntitiesWith<TransformComponent, CircleCollider2DComponent>())
            {
                auto [tc, cc] = GetActiveScene()->GetRegistry()
                    .get<TransformComponent, CircleCollider2DComponent>(e);

                const glm::mat4 t =
                    glm::translate(glm::mat4(1.0f), tc.Translation + glm::vec3(cc.Offset, 0.001f))
                    * glm::scale(glm::mat4(1.0f), tc.Scale * glm::vec3(cc.Radius * 2.0f));

                Renderer2D::DrawCircle(t, { 0, 1, 0, 1 }, 0.03f);
            }
        }

        Renderer2D::EndScene();
    }

    void EditorLayer2D::OnImGuiExtra()
    {
    }

} // namespace Wheatear
