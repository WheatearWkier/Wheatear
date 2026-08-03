#pragma once

#include <imgui/imgui.h>

#include <algorithm>
#include <string>
#include <unordered_map>

namespace Wheatear {

    class EditorFloatingWindow
    {
    public:
        static bool Begin(const char* title,
            bool* open = nullptr,
            ImGuiWindowFlags flags = 0,
            ImVec2 preferredSize = { 1100.0f, 720.0f })
        {
            State& state = GetState(title);
            if (state.Floating && state.RequestPlacement)
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
            }

            if (state.Floating)
            {
                ImGuiWindowClass windowClass;
                windowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
                windowClass.ViewportFlagsOverrideClear =
                    ImGuiViewportFlags_NoDecoration | ImGuiViewportFlags_NoTaskBarIcon;
                ImGui::SetNextWindowClass(&windowClass);
            }

            std::string windowTitle = title;
            if (state.Floating)
                windowTitle += "##Floating";

            return ImGui::Begin(windowTitle.c_str(), open, flags);
        }

        static void End()
        {
            ImGui::End();
        }

        static void DrawToggleButton(const char* title)
        {
            State& state = GetState(title);
            ImGui::PushID(title);
            if (ImGui::Button(state.Floating ? "Dock" : "Pop Out"))
            {
                state.Floating = !state.Floating;
                state.RequestPlacement = state.Floating;
            }
            ImGui::PopID();
        }

        static bool DrawFloatingMenuItem(const char* title)
        {
            State& state = GetState(title);
            std::string label = std::string("Pop Out ") + title;
            if (state.Floating)
                label = std::string("Dock ") + title;

            if (!ImGui::MenuItem(label.c_str()))
                return false;

            state.Floating = !state.Floating;
            state.RequestPlacement = state.Floating;
            return true;
        }

        static void OpenFloating(const char* title)
        {
            State& state = GetState(title);
            state.Floating = true;
            state.RequestPlacement = true;
        }

        static void Dock(const char* title)
        {
            State& state = GetState(title);
            state.Floating = false;
            state.RequestPlacement = false;
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
        };

        static State& GetState(const char* title)
        {
            static std::unordered_map<std::string, State> states;
            return states[title ? title : ""];
        }
    };

} // namespace Wheatear
