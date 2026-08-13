#include "wepch.h"
#include "EditorLayer3D.h"

#include "Editor/EditorLocale.h"
#include "Wheatear/Core/AssetPath.h"

#include "Wheatear/Renderer/Framebuffer.h"

#include "Wheatear/Renderer/Renderer2D.h"
#include "Wheatear/Renderer/Renderer3D.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Utils/PlatformUtils.h"
#include "Panels/SceneHierarchy/SceneHierarchyPanel.h"

#include <imgui/imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <filesystem>
#include <cmath>

namespace Wheatear {

    EditorLayer3D::EditorLayer3D()
        : EditorLayerBase("EditorLayer3D")
    {
    }

    // =========================================================================
    // =========================================================================

    void EditorLayer3D::OnAttach()
    {
        EditorLayerBase::OnAttach();

        Renderer3D::InitSSAO(1920, 1080);
        m_LastSSAOWidth = 1920;
        m_LastSSAOHeight = 1080;

        const std::string defaultHDR = "assets/hdr/default.hdr";
        const std::filesystem::path resolvedDefaultHDR = AssetPath::Resolve(defaultHDR);
        if (std::filesystem::exists(resolvedDefaultHDR))
        {
            m_IBLPath = defaultHDR;
            m_IBL     = IBLPrecompute::ComputeOrLoad(resolvedDefaultHDR.string());
            Renderer3D::SetIBL(m_IBL);
        }
    }

    // =========================================================================
    // =========================================================================

    void EditorLayer3D::OnBeginRender()
    {
        if (GetSceneState() != SceneState::Edit)
            return;

        Renderer3D::BeginScene(GetEditorCamera());
        Renderer3D::DrawEditorSkybox();
        // RenderSystem continues the same 3D pass during Scene::OnUpdateEditor.
    }

    // =========================================================================
    // =========================================================================

    void EditorLayer3D::OnPostSceneUpdate()
    {
        const auto& spec = m_Framebuffer->GetSpecification();

        if (m_LastSSAOWidth != spec.Width ||
            m_LastSSAOHeight != spec.Height)
        {
            Renderer3D::ResizeSSAO(spec.Width, spec.Height);
            m_LastSSAOWidth = spec.Width;
            m_LastSSAOHeight = spec.Height;
        }

        Renderer3D::ComputeSSAO(
            m_Framebuffer->GetColorAttachmentRendererID(1),
            m_Framebuffer->GetDepthAttachmentRendererID(),
            GetEditorCamera().GetProjection()
        );
    }

    // =========================================================================
    // =========================================================================

    void EditorLayer3D::OnOverlayRender()
    {
        if (GetSceneState() == SceneState::Play)
        {
            Entity camera = GetActiveScene()->GetPrimaryCameraEntity();
            if (!camera)
                return;

            const auto& camComp = camera.GetComponent<CameraComponent>();
            const auto& camTC = camera.GetComponent<TransformComponent>();
            Renderer2D::BeginScene(camComp.Camera, camTC.GetTransform());
        }
        else
        {
            Renderer2D::BeginScene(GetEditorCamera());
        }

        if (m_ShowPhysicsColliders)
        {
            // Reserved for future 3D collider gizmos.
        }

        for (auto e : GetActiveScene()->GetAllEntitiesWith<TransformComponent, DirectionalLightComponent>())
        {
            Entity selected = GetHierarchyPanel().GetSelectedEntity();
            if (!selected || static_cast<entt::entity>(selected) != e)
                continue;

            auto [tc, dl] = GetActiveScene()->GetRegistry()
                .get<TransformComponent, DirectionalLightComponent>(e);

            glm::mat4 rot = glm::toMat4(glm::quat(tc.Rotation));
            glm::vec3 dir = glm::normalize(glm::vec3(rot * glm::vec4(0.0f, -1.0f, 0.0f, 0.0f)));

            const glm::vec3 pos = tc.Translation;
            const glm::vec4 color = { 1.0f, 0.9f, 0.3f, 1.0f };
            const float rayLen = 1.5f;
            const float offset = 0.3f;

            Renderer2D::DrawLine(pos, pos + dir * rayLen, color);

            glm::vec3 right = glm::normalize(glm::cross(dir, glm::vec3(0.0f, 1.0f, 0.0f)));
            if (glm::length(right) < 0.01f)
                right = { 1.0f, 0.0f, 0.0f };
            glm::vec3 up = glm::normalize(glm::cross(right, dir));

            for (int dx = -1; dx <= 1; dx += 2)
            {
                for (int dy = -1; dy <= 1; dy += 2)
                {
                    glm::vec3 start = pos + right * (offset * dx) + up * (offset * dy);
                    Renderer2D::DrawLine(start, start + dir * rayLen, color);
                }
            }

            glm::quat circleRotation = glm::rotation(glm::vec3(0.0f, 0.0f, 1.0f), dir);
            glm::mat4 t = glm::translate(glm::mat4(1.0f), pos)
                * glm::toMat4(circleRotation)
                * glm::scale(glm::mat4(1.0f), glm::vec3(0.85f));
            Renderer2D::DrawCircle(t, color, 0.05f);
        }

        for (auto e : GetActiveScene()->GetAllEntitiesWith<TransformComponent, PointLightComponent>())
        {
            Entity selected = GetHierarchyPanel().GetSelectedEntity();
            if (!selected || static_cast<entt::entity>(selected) != e)
                continue;

            auto [tc, pl] = GetActiveScene()->GetRegistry()
                .get<TransformComponent, PointLightComponent>(e);

            const glm::vec3 pos = tc.Translation;
            const glm::vec4 color = { 1.0f, 0.7f, 0.2f, 1.0f };
            float radius = glm::min(std::sqrt(pl.Intensity), 0.5f);

            glm::mat4 t = glm::translate(glm::mat4(1.0f), pos)
                * glm::scale(glm::mat4(1.0f), glm::vec3(radius * 2.0f));
            Renderer2D::DrawCircle(t, color, 0.02f);

            glm::mat4 tXY = glm::translate(glm::mat4(1.0f), pos)
                * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), { 1.0f, 0.0f, 0.0f })
                * glm::scale(glm::mat4(1.0f), glm::vec3(radius * 2.0f));
            Renderer2D::DrawCircle(tXY, color, 0.02f);

            glm::mat4 tYZ = glm::translate(glm::mat4(1.0f), pos)
                * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), { 0.0f, 0.0f, 1.0f })
                * glm::scale(glm::mat4(1.0f), glm::vec3(radius * 2.0f));
            Renderer2D::DrawCircle(tYZ, color, 0.02f);
        }

        if (GetSceneState() == SceneState::Edit)
            Renderer3D::DrawEditorGrid();

        Renderer2D::EndScene();
    }

    // =========================================================================
    // OnImGuiExtra    // =========================================================================

    void EditorLayer3D::OnImGuiExtra()
    {
        ImGui::Begin("Settings (3D)");

        ImGui::Text(EditorLocale::Text("Environment (IBL)", "环境 (IBL)"));

        if (m_IBL && m_IBL->IsValid())
        {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Active");
            ImGui::SameLine();
            ImGui::TextDisabled("%s",
                std::filesystem::path(m_IBLPath).filename().string().c_str());
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "None (gradient fallback)");
        }

        if (ImGui::SliderFloat(EditorLocale::Text("IBL Intensity", "IBL 强度"), &m_IBLIntensity, 0.0f, 3.0f))
            Renderer3D::SetIBLIntensity(m_IBLIntensity);

        if (ImGui::Button(EditorLocale::Text("Load HDR...", "加载 HDR...")))
        {
            std::string path = FileDialogs::OpenFile("HDR Image (*.hdr)\0*.hdr\0");
            if (!path.empty())
            {
                m_IBLPath     = path;
                m_IBL         = IBLPrecompute::ComputeOrLoad(path);
                m_IBLIntensity = 1.0f;
                Renderer3D::SetIBL(m_IBL);
                Renderer3D::SetIBLIntensity(1.0f);
            }
        }

        if (m_IBL && m_IBL->IsValid())
        {
            ImGui::SameLine();
            if (ImGui::Button(EditorLocale::Text("Clear", "清空")))
            {
                m_IBL.reset();
                m_IBLPath.clear();
                Renderer3D::ClearIBL();
            }
        }

        // SSAO
        ImGui::Separator();
        ImGui::Text(EditorLocale::Text("SSAO (Screen Space Ambient Occlusion)", "SSAO（屏幕空间环境光遮蔽）"));

        ImGui::Checkbox("Enable##ssao", &Renderer3D::SSAOEnabled());
        if (Renderer3D::SSAOEnabled())
        {
            ImGui::SliderFloat(EditorLocale::Text("Radius##ssao", "半径##ssao"), &Renderer3D::SSAORadius(), 0.05f, 2.0f);
            ImGui::SliderFloat(EditorLocale::Text("Bias##ssao", "偏移##ssao"), &Renderer3D::SSAOBias(), 0.001f, 0.1f);
            ImGui::SliderFloat(EditorLocale::Text("Power##ssao", "强度##ssao"), &Renderer3D::SSAOPower(), 0.5f, 4.0f);
        }

        // ImGui::Separator();
        // ImGui::Text(EditorLocale::Text("3D Physics (coming soon)", "3D 物理（即将推出）"));
        // ImGui::Text(EditorLocale::Text("3D Animation (coming soon)", "3D 动画（即将推出）"));

        ImGui::End();
    }

} // namespace Wheatear
