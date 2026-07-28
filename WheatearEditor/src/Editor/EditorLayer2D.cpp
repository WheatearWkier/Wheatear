#include "wtpch.h"
#include "EditorLayer2D.h"

#include "Wheatear/Renderer/Renderer2D.h"

#include <imgui/imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Wheatear {

    EditorLayer2D::EditorLayer2D()
        : EditorLayerBase("EditorLayer2D")
    {
    }

    // =========================================================================
    // OnOverlayRender：物理碰撞体 + 场景 Gizmo 叠加层
    // =========================================================================

    void EditorLayer2D::OnOverlayRender()
    {
        // 根据场景状态选择相机来源
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

        // ── 物理碰撞体线框 ──────────────────────────────────────────────────
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

    // =========================================================================
    // OnImGuiExtra：2D 专属设置面板
    // =========================================================================

    void EditorLayer2D::OnImGuiExtra()
    {
        ImGui::Begin("Settings (2D)");

        ImGui::Checkbox("Show physics colliders", &m_ShowPhysicsColliders);

        // 预留：未来可在此添加 2D 专属选项
        // ImGui::Checkbox("Show sprite bounds", &m_ShowSpriteBounds);

        ImGui::End();
    }

} // namespace Wheatear
