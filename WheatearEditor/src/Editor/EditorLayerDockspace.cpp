#include "wepch.h"
#include "EditorLayerBase.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Wheatear {

    void EditorLayerBase::BuildDefaultDockspaceLayout(uint32_t dockspaceID)
    {
        if (!m_RequestDefaultDockspaceLayout || m_DefaultDockspaceLayoutBuilt)
            return;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGuiID dockspace = static_cast<ImGuiID>(dockspaceID);

        ImGui::DockBuilderRemoveNode(dockspace);
        ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodePos(dockspace, viewport->WorkPos);
        ImGui::DockBuilderSetNodeSize(dockspace, viewport->WorkSize);

        ImGuiID main = dockspace;
        ImGuiID left = ImGui::DockBuilderSplitNode(main, ImGuiDir_Left, 0.20f, nullptr, &main);
        ImGuiID right = ImGui::DockBuilderSplitNode(main, ImGuiDir_Right, 0.26f, nullptr, &main);
        ImGuiID bottom = ImGui::DockBuilderSplitNode(main, ImGuiDir_Down, 0.24f, nullptr, &main);
        ImGuiID bottomRight = ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Right, 0.45f, nullptr, &bottom);
        ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.30f, nullptr, &right);
        ImGuiID canvas = ImGui::DockBuilderSplitNode(main, ImGuiDir_Right, 0.38f, nullptr, &main);

        ImGui::DockBuilderDockWindow("Scene Hierarchy", left);
        ImGui::DockBuilderDockWindow("Properties", right);
        ImGui::DockBuilderDockWindow("Stats", rightBottom);
        ImGui::DockBuilderDockWindow("Player Build", rightBottom);
        ImGui::DockBuilderDockWindow("Console", rightBottom);
        ImGui::DockBuilderDockWindow("Content Browser", bottom);
        ImGui::DockBuilderDockWindow("Animation Editor", bottomRight);
        ImGui::DockBuilderDockWindow("Sprite Sheet Picker", bottomRight);
        ImGui::DockBuilderDockWindow("Viewport", main);
        ImGui::DockBuilderDockWindow("UI Canvas Editor", canvas);

        ImGui::DockBuilderFinish(dockspace);

        if (const char* iniPath = ImGui::GetIO().IniFilename)
            ImGui::SaveIniSettingsToDisk(iniPath);

        m_DefaultDockspaceLayoutBuilt = true;
        m_RequestDefaultDockspaceLayout = false;
    }

} // namespace Wheatear
