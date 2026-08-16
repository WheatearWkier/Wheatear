#pragma once
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include "Wheatear/Scene/Entity.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorCommands.h"
namespace Wheatear {

    // Structural component edits (reset / remove) are suppressed while the
    // editor plays a runtime copy; the hierarchy panel flips this flag.
    inline bool& ComponentDrawReadOnlyState()
    {
        static bool value = false;
        return value;
    }
    inline void SetComponentDrawReadOnly(bool readOnly)
    {
        ComponentDrawReadOnlyState() = readOnly;
    }
    inline bool IsComponentDrawReadOnly()
    {
        return ComponentDrawReadOnlyState();
    }

    template<typename T, typename UIFunction>
    void DrawComponent(const std::string& name, Entity entity, UIFunction uiFunction)
    {
        constexpr ImGuiTreeNodeFlags treeNodeFlags =
            ImGuiTreeNodeFlags_DefaultOpen |
            ImGuiTreeNodeFlags_Framed |
            ImGuiTreeNodeFlags_SpanAvailWidth |
            ImGuiTreeNodeFlags_AllowOverlap |
            ImGuiTreeNodeFlags_FramePadding;
        if (!entity.HasComponent<T>())
            return;
        auto& component = entity.GetComponent<T>();
        ImGui::PushID(static_cast<const void*>(&typeid(T)));
        const ImVec2 contentRegion = ImGui::GetContentRegionAvail();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
        const float lineHeight = ImGui::GetFrameHeight();
        ImGui::Separator();
        const bool open = ImGui::TreeNodeEx(
            "##ComponentHeader",
            treeNodeFlags,
            "%s", name.c_str()
        );
        const float componentTop = ImGui::GetItemRectMin().y;
        ImGui::PopStyleVar();
        ImGui::SameLine(contentRegion.x - lineHeight * 0.5f);
        const bool readOnly = IsComponentDrawReadOnly();
        ImGui::BeginDisabled(readOnly);
        if (ImGui::Button("...", ImVec2{ lineHeight, lineHeight }))
            ImGui::OpenPopup("ComponentSettings");
        ImGui::EndDisabled();
        bool removeComponent = false;
        if (ImGui::BeginPopup("ComponentSettings"))
        {
            ImGui::BeginDisabled(readOnly);
            if (ImGui::MenuItem(EditorLocale::Text("Reset", "重置")))
            {
                T before = entity.GetComponent<T>();
                T after{};
                entity.GetComponent<T>() = after;
                CommandHistory::Get().Push(MakeComponentValueCommand(entity, before, after));
                ImGui::CloseCurrentPopup();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(EditorLocale::Text("Remove Component", "移除组件")))
                removeComponent = true;
            ImGui::EndDisabled();
            ImGui::EndPopup();
        }
        if (open)
        {
            static std::unordered_map<uint32_t, T> s_EditStartValues;
            static std::unordered_map<uint32_t, bool> s_Editing;

            const uint32_t key = static_cast<uint32_t>(entity);
            const T beforeFrame = component;
            uiFunction(component);

            const float componentBottom = ImGui::GetCursorScreenPos().y;
            const ImVec2 mouse = ImGui::GetMousePos();
            const ImVec2 windowMin = ImGui::GetWindowPos();
            const ImVec2 windowMax = {
                windowMin.x + ImGui::GetWindowSize().x,
                windowMin.y + ImGui::GetWindowSize().y
            };
            const bool mouseInsideComponent =
                mouse.x >= windowMin.x && mouse.x <= windowMax.x
                && mouse.y >= componentTop && mouse.y <= componentBottom;
            const bool anyItemActive = ImGui::IsAnyItemActive();

            if (anyItemActive && mouseInsideComponent && !s_Editing[key])
            {
                s_EditStartValues[key] = beforeFrame;
                s_Editing[key] = true;
            }
            else if (!anyItemActive && s_Editing[key])
            {
                if (entity.HasComponent<T>())
                {
                    T after = entity.GetComponent<T>();
                    CommandHistory::Get().Push(MakeComponentValueCommand(entity, s_EditStartValues[key], after));
                }
                s_EditStartValues.erase(key);
                s_Editing.erase(key);
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
        if (removeComponent)
        {
            auto cmd = std::make_unique<RemoveComponentCommand<T>>(entity);
            cmd->Execute();
            CommandHistory::Get().Push(std::move(cmd));
        }
    }
} // namespace Wheatear
#include "Drawers/TransformDrawer.h"
#include "Drawers/CameraDrawer.h"
#include "Drawers/SpriteRendererDrawer.h"
#include "Drawers/SpriteAnimatorDrawer.h"
#include "Drawers/CircleRendererDrawer.h"
#include "Drawers/PhysicsDrawers.h"
#include "Drawers/EventScriptDrawer.h"
#include "Drawers/UIDrawers.h"
#include "Drawers/AudioDrawer.h"
#include "Drawers/Mesh3DDrawers.h"
