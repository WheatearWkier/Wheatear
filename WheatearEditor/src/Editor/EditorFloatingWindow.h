#pragma once

#include <imgui/imgui.h>

#include <algorithm>
#include <string>
#include <unordered_map>

namespace Wheatear {

    class EditorFloatingWindow
    {
    public:
        // Tools that are not part of the permanent workspace layout open as
        // tabs in the central document area by default.
        static void SetDefaultDockNode(ImGuiID node)
        {
            s_DefaultDockNode = node;
        }

        static ImGuiID GetDefaultDockNode()
        {
            return s_DefaultDockNode;
        }

        static bool Begin(const char* title,
            bool* open = nullptr,
            ImGuiWindowFlags flags = 0,
            ImVec2 preferredSize = { 1100.0f, 720.0f })
        {
            State& state = GetState(title);
            if (state.Floating)
            {
                if (state.RequestFloat || state.RequestPlacement)
                {
                    const ImGuiViewport* viewport = ImGui::GetMainViewport();
                    ImVec2 size = preferredSize;
                    const float maxWidth = std::max(420.0f, viewport->WorkSize.x * 0.92f);
                    const float maxHeight = std::max(320.0f, viewport->WorkSize.y * 0.88f);
                    size.x = std::min(std::max(size.x, 420.0f), maxWidth);
                    size.y = std::min(std::max(size.y, 320.0f), maxHeight);
                    const ImVec2 position = {
                        viewport->WorkPos.x + (viewport->WorkSize.x - size.x) * 0.5f,
                        viewport->WorkPos.y + (viewport->WorkSize.y - size.y) * 0.5f
                    };

                    ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
                    ImGui::SetNextWindowPos(position, ImGuiCond_Always);
                    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
                    state.RequestPlacement = false;
                    state.RequestFloat = false;
                }

                ImGuiWindowClass windowClass;
                windowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
                windowClass.ViewportFlagsOverrideClear =
                    ImGuiViewportFlags_NoDecoration | ImGuiViewportFlags_NoTaskBarIcon;
                ImGui::SetNextWindowClass(&windowClass);
            }
            else if (s_DefaultDockNode != 0)
            {
                if (state.RequestDock)
                {
                    ImGui::SetNextWindowDockID(s_DefaultDockNode, ImGuiCond_Always);
                    state.RequestDock = false;
                }
                else
                {
                    // FirstUseEver keeps a user's manual dock position but
                    // gives newly opened tools a sensible home tab.
                    ImGui::SetNextWindowDockID(s_DefaultDockNode, ImGuiCond_FirstUseEver);
                }
            }

            // Keep one stable window ID for both docked and floating states.
            // Appending "##Floating" used to create a second ImGui settings
            // entry, which made panels come back as orphan windows.
            return ImGui::Begin(title, open, flags);
        }

        static void End()
        {
            ImGui::End();
        }

        static void DrawToggleButton(const char* title)
        {
            State& state = GetState(title);
            ImGui::PushID(title);
            if (ImGui::SmallButton(state.Floating ? "Dock" : "Float"))
            {
                state.Floating = !state.Floating;
                state.RequestPlacement = state.Floating;
                state.RequestFloat = state.Floating;
                state.RequestDock = !state.Floating;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", state.Floating
                    ? "Dock this window back into the workspace"
                    : "Open this window as a separate OS window");
            ImGui::PopID();
        }

        static bool DrawFloatingMenuItem(const char* title)
        {
            State& state = GetState(title);
            std::string label = std::string("Float ") + title;
            if (state.Floating)
                label = std::string("Dock ") + title;

            if (!ImGui::MenuItem(label.c_str()))
                return false;

            state.Floating = !state.Floating;
            state.RequestPlacement = state.Floating;
            state.RequestFloat = state.Floating;
            state.RequestDock = !state.Floating;
            return true;
        }

        static void OpenFloating(const char* title)
        {
            State& state = GetState(title);
            state.Floating = true;
            state.RequestPlacement = true;
            state.RequestFloat = true;
        }

        static void Dock(const char* title)
        {
            State& state = GetState(title);
            if (state.Floating)
                state.RequestDock = true;
            state.Floating = false;
            state.RequestPlacement = false;
            state.RequestFloat = false;
        }

        static bool IsFloating(const char* title)
        {
            return GetState(title).Floating;
        }

    private:
        struct State
        {
            bool Floating = false;
            bool RequestPlacement = false;
            bool RequestFloat = false;
            bool RequestDock = false;
        };

        static State& GetState(const char* title)
        {
            static std::unordered_map<std::string, State> states;
            return states[title ? title : ""];
        }

        static inline ImGuiID s_DefaultDockNode = 0;
    };

} // namespace Wheatear
