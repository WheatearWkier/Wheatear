#include "wepch.h"
#include "InputBindingsPanel.h"

#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorLocale.h"
#include "Wheatear/Config/UserSettings.h"
#include "Wheatear/Input/Input.h"
#include "Wheatear/Input/InputBindingService.h"
#include "Wheatear/Input/KeyCodes.h"
#include "Wheatear/Input/MouseButtonCodes.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace Wheatear {

    namespace {

        // Candidate keys scanned while waiting for input (letters, digits,
        // function keys, navigation, punctuation + mouse buttons).
        const std::vector<int>& CandidateKeys()
        {
            static const std::vector<int> keys = []()
            {
                std::vector<int> result;
                for (int key = WT_KEY_A; key <= WT_KEY_Z; ++key)
                    result.push_back(key);
                for (int key = WT_KEY_0; key <= WT_KEY_9; ++key)
                    result.push_back(key);
                for (int key = WT_KEY_F1; key <= WT_KEY_F12; ++key)
                    result.push_back(key);
                const int extras[] = {
                    WT_KEY_SPACE, WT_KEY_ENTER, WT_KEY_ESCAPE, WT_KEY_TAB,
                    WT_KEY_BACKSPACE, WT_KEY_DELETE, WT_KEY_HOME, WT_KEY_END,
                    WT_KEY_LEFT, WT_KEY_RIGHT, WT_KEY_UP, WT_KEY_DOWN,
                    WT_KEY_LEFT_SHIFT, WT_KEY_RIGHT_SHIFT,
                    WT_KEY_LEFT_CONTROL, WT_KEY_RIGHT_CONTROL,
                    WT_KEY_LEFT_ALT, WT_KEY_RIGHT_ALT,
                    WT_KEY_SEMICOLON, WT_KEY_APOSTROPHE, WT_KEY_COMMA,
                    WT_KEY_PERIOD, WT_KEY_SLASH, WT_KEY_BACKSLASH,
                    WT_KEY_LEFT_BRACKET, WT_KEY_RIGHT_BRACKET,
                    WT_KEY_MINUS, WT_KEY_EQUAL, WT_KEY_GRAVE_ACCENT
                };
                for (int key : extras)
                    result.push_back(key);
                return result;
            }();
            return keys;
        }

    } // namespace

    void InputBindingsPanel::OnImGuiRender()
    {
        if (!m_Open)
            return;

        if (!EditorFloatingWindow::Begin("Input Bindings", &m_Open, 0, { 520.0f, 560.0f }))
        {
            EditorFloatingWindow::End();
            return;
        }
        EditorFloatingWindow::DrawToggleButton("Input Bindings");

        ImGui::TextDisabled("%s", EditorLocale::Text(
            "Click a binding and press a key / mouse button to remap. Right-click for reset / clear.",
            "点击绑定后按下新键 / 鼠标键即可重映射。右键可重置 / 清空。"));

        if (!m_WaitingAction.empty())
        {
            ImGui::Separator();
            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark),
                "%s %s", m_WaitingAction.c_str(),
                EditorLocale::Text("... press a key (Esc to cancel)", "… 等待按键（Esc 取消）"));

            const int pressed = PollPressedInput();
            if (pressed != 0)
                CommitBinding(m_WaitingAction, pressed);
        }
        ImGui::Separator();

        // Group actions by prefix, preserving default declaration order.
        const auto& defaults = UserSettings::Defaults().KeyBindings;
        std::string currentGroup;
        for (const auto& [actionId, keys] : defaults)
        {
            const size_t dot = actionId.find('.');
            const std::string group = (dot != std::string::npos)
                ? actionId.substr(0, dot) : actionId;

            if (group != currentGroup)
            {
                if (!currentGroup.empty())
                    ImGui::Spacing();
                currentGroup = group;
                ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),
                    "%s", group.c_str());
            }

            DrawActionRow(actionId);
        }

        EditorFloatingWindow::End();
    }

    void InputBindingsPanel::DrawActionRow(const std::string& actionId)
    {
        ImGui::PushID(actionId.c_str());

        const std::string label = InputBindingService::GetBindingLabel(actionId);
        const bool waiting = (m_WaitingAction == actionId);

        ImGui::TextUnformatted(actionId.c_str());
        ImGui::SameLine(220.0f);
        ImGui::SetNextItemWidth(180.0f);
        if (waiting)
        {
            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark),
                "%s", EditorLocale::Text("Press a key...", "按下按键..."));
        }
        else
        {
            const std::string buttonLabel = label.empty() ? "-" : label;
            if (ImGui::Button(buttonLabel.c_str(), ImVec2(180.0f, 0.0f)))
                m_WaitingAction = actionId;
        }

        // Right-click menu: reset to default / clear bindings.
        if (!waiting && ImGui::BeginPopupContextItem("##bind_ctx"))
        {
            if (ImGui::MenuItem(EditorLocale::Text("Reset to Default", "重置为默认")))
                InputBindingService::SetKeys(actionId, UserSettings::Defaults().KeyBindings[actionId]);
            if (ImGui::MenuItem(EditorLocale::Text("Clear", "清空")))
                InputBindingService::SetKeys(actionId, {});
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    int InputBindingsPanel::PollPressedInput() const
    {
        // Mouse buttons first (bindings are stored as negative values).
        for (int button = 0; button <= WT_MOUSE_BUTTON_8; ++button)
        {
            if (Input::IsMouseButtonPressed(button))
                return InputBindingService::ButtonToMouseBinding(button);
        }

        for (int key : CandidateKeys())
        {
            if (Input::IsKeyPressed(key))
                return key;
        }
        return 0;
    }

    void InputBindingsPanel::CommitBinding(const std::string& actionId, int binding)
    {
        if (binding == WT_KEY_ESCAPE)
        {
            m_WaitingAction.clear();   // Esc cancels
            return;
        }

        // Replace the whole binding with the new key (a single-key binding is
        // the common case; multi-key defaults can be re-added via Reset).
        InputBindingService::SetKeys(actionId, { binding });
        m_WaitingAction.clear();
    }

} // namespace Wheatear
