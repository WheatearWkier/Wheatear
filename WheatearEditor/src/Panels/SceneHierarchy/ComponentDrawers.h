#pragma once
// ── DrawComponent<T> 模板 ────────────────────────────────────────────────
// 负责绘制折叠标题 / 右上�?"+" 设置按钮 / Remove component 菜单
// 所�?Drawer.cpp 只需 include 这一个头文件即可使用，不要在本文件外引用
// ────────────────────────────────────────────────────────────────────────
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include "Wheatear/Scene/Entity.h"
namespace Wheatear {
    template<typename T, typename UIFunction>
    void DrawComponent(const std::string& name, Entity entity, UIFunction uiFunction)
    {
        constexpr ImGuiTreeNodeFlags treeNodeFlags =
            ImGuiTreeNodeFlags_DefaultOpen |
            ImGuiTreeNodeFlags_Framed |
            ImGuiTreeNodeFlags_SpanAvailWidth |
            ImGuiTreeNodeFlags_AllowItemOverlap |
            ImGuiTreeNodeFlags_FramePadding;
        if (!entity.HasComponent<T>())
            return;
        auto& component = entity.GetComponent<T>();
        const ImVec2 contentRegion = ImGui::GetContentRegionAvail();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
        const float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
        ImGui::Separator();
        const bool open = ImGui::TreeNodeEx(
            reinterpret_cast<void*>(typeid(T).hash_code()),
            treeNodeFlags,
            "%s", name.c_str()
        );
        ImGui::PopStyleVar();
        // 设置按钮放右�?        ImGui::SameLine(contentRegion.x - lineHeight * 0.5f);
        if (ImGui::Button("...", ImVec2{ lineHeight, lineHeight }))
            ImGui::OpenPopup("ComponentSettings");
        bool removeComponent = false;
        if (ImGui::BeginPopup("ComponentSettings"))
        {
            if (ImGui::MenuItem("Reset"))
            {
                // 重置为默认值：先移除再添加
                entity.RemoveComponent<T>();
                entity.AddComponent<T>();
                ImGui::CloseCurrentPopup();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Remove Component"))
                removeComponent = true;
            ImGui::EndPopup();
        }
        if (open)
        {
            uiFunction(component);
            ImGui::TreePop();
        }
        if (removeComponent)
            entity.RemoveComponent<T>();
    }
} // namespace Wheatear
// ── 统一出口：外部只需 include 这一个头 ──────────────────────────────────
#include "Drawers/TransformDrawer.h"
#include "Drawers/CameraDrawer.h"
#include "Drawers/SpriteRendererDrawer.h"
#include "Drawers/SpriteAnimatorDrawer.h"
#include "Drawers/CircleRendererDrawer.h"
#include "Drawers/PhysicsDrawers.h"
#include "Drawers/ScriptDrawer.h"
#include "Drawers/UIDrawers.h"
#include "Drawers/AudioDrawer.h"
#include "Drawers/Mesh3DDrawers.h"
