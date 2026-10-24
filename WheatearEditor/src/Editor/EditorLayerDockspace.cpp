#include "wepch.h"
#include "EditorLayerBase.h"
#include "Editor/EditorFloatingWindow.h"
#include "Wheatear/Core/Application.h"
#include "Wheatear/ImGui/ImGuiLayer.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>

namespace Wheatear {

    void EditorLayerBase::BuildDefaultDockspaceLayout(uint32_t dockspaceID)
    {
        if (!m_RequestDefaultDockspaceLayout || m_DefaultDockspaceLayoutBuilt)
            return;

        ImGuiID dockspace = static_cast<ImGuiID>(dockspaceID);

        ImGui::DockBuilderRemoveNode(dockspace);
        ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
        // Build against the area left after the menu bar and action toolbar,
        // not against the raw viewport, so the initial node never sits under
        // them.
        const ImVec2 contentPos = ImGui::GetCursorScreenPos();
        const ImVec2 contentSize = ImGui::GetContentRegionAvail();
        ImGui::DockBuilderSetNodePos(dockspace, contentPos);
        ImGui::DockBuilderSetNodeSize(dockspace,
            ImVec2(std::max(contentSize.x, 1.0f), std::max(contentSize.y, 1.0f)));

        ImGuiID main = dockspace;
        ImGuiID left = ImGui::DockBuilderSplitNode(main, ImGuiDir_Left, 0.19f, nullptr, &main);
        ImGuiID right = ImGui::DockBuilderSplitNode(main, ImGuiDir_Right, 0.24f, nullptr, &main);
        ImGuiID bottom = ImGui::DockBuilderSplitNode(main, ImGuiDir_Down, 0.28f, nullptr, &main);

        // Curated modern-engine layout:
        //   left    = outliner
        //   center  = viewport and document tabs (UI canvas, animation, atlas)
        //   right   = inspector and secondary tabs
        //   bottom  = content browser / console / build output
        ImGui::DockBuilderDockWindow("Scene Hierarchy", left);

        ImGui::DockBuilderDockWindow("Viewport", main);
        ImGui::DockBuilderDockWindow("UI Canvas Editor", main);
        if (!Is3DWorkspace())
        {
            ImGui::DockBuilderDockWindow("Animation Editor", main);
            ImGui::DockBuilderDockWindow("Sprite Sheet Picker", main);
        }

        ImGui::DockBuilderDockWindow("Properties", right);
        ImGui::DockBuilderDockWindow("Stats", right);
        if (Is3DWorkspace())
            ImGui::DockBuilderDockWindow("Settings (3D)", right);

        ImGui::DockBuilderDockWindow("Content Browser", bottom);
        ImGui::DockBuilderDockWindow("Console", bottom);
        ImGui::DockBuilderDockWindow("Data File Editor", bottom);
        ImGui::DockBuilderDockWindow("Player Build", bottom);

        ImGui::DockBuilderFinish(dockspace);

        // Tools opened later (WAO graph, event scripts, tuning editors) become
        // tabs in the central document area instead of free-floating windows.
        EditorFloatingWindow::SetDefaultDockNode(main);

        if (const ImGuiLayer* guiLayer = Application::Get().GetImGuiLayer();
            guiLayer && !guiLayer->GetIniPath().empty())
        {
            ImGui::SaveIniSettingsToDisk(guiLayer->GetIniPath().c_str());
        }

        m_DefaultDockspaceLayoutBuilt = true;
        m_RequestDefaultDockspaceLayout = false;
        m_RequestViewportFocus = true;
    }

} // namespace Wheatear
