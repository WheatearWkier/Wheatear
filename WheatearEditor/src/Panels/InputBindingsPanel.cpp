#include "wepch.h"
#include "InputBindingsPanel.h"

#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Wheatear/Config/UserSettings.h"
#include "Wheatear/Input/Input.h"
#include "Wheatear/Input/InputActionCatalog.h"
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

        // Default keys for an action: data catalog first, C++ fallback.
        std::vector<int> DefaultKeysFor(const std::string& actionId)
        {
            if (const std::vector<int>* catalogKeys = InputActionCatalog::GetDefaultKeys(actionId))
                return *catalogKeys;

            const UserSettingsData defaults = UserSettings::Defaults();
            auto it = defaults.KeyBindings.find(actionId);
            return (it != defaults.KeyBindings.end()) ? it->second : std::vector<int>{};
        }

    } // namespace

    void InputBindingsPanel::OnImGuiRender()
    {
        if (!m_Open)
            return;

        if (!EditorFloatingWindow::Begin("Input Bindings", &m_Open, 0, { 560.0f, 600.0f }))
        {
            EditorFloatingWindow::End();
            return;
        }
        EditorFloatingWindow::DrawToggleButton("Input Bindings");

        ImGui::TextDisabled("%s", EditorLocale::Text(
            "Click a binding and press a key / mouse button to remap. Right-click for reset / clear.",
            "点击绑定后按下新键 / 鼠标键即可重映射。右键可重置 / 清空。"));
        ImGui::TextDisabled("%s", EditorLocale::Text(
            "Actions are defined in assets/input/action_bindings.yaml; new actions are saved to the project.",
            "动作定义在 assets/input/action_bindings.yaml；新增动作会保存到项目。"));

        if (!m_WaitingAction.empty())
        {
            ImGui::Separator();
            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark),
                "%s %s", m_WaitingAction.c_str(),
                EditorLocale::Text("... press key(s) (Esc to finish)", "… 依次按下按键（Esc 结束）"));

            const int pressed = PollPressedInput();
            if (pressed != 0)
                CommitBinding(m_WaitingAction, pressed);
        }
        ImGui::Separator();

        DrawAddActionBar();
        ImGui::Separator();

        // Group actions by prefix, preserving catalog declaration order
        // (catalog actions first, then C++ defaults not present in the file).
        const std::vector<std::string> actionIds = InputActionCatalog::GetAllActionIds();
        std::string currentGroup;
        for (const std::string& actionId : actionIds)
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

    void InputBindingsPanel::DrawAddActionBar()
    {
        ImGui::TextUnformatted(EditorLocale::Text("New Action:", "新动作:"));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.0f);
        EditorWidgets::InputString("##NewActionId", m_NewActionId, 64);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("Add", "添加")))
        {
            std::string id = m_NewActionId;
            // Trim whitespace and reject ids without the prefix.group shape.
            const size_t first = id.find_first_not_of(" \t\r\n");
            const size_t last = id.find_last_not_of(" \t\r\n");
            if (first != std::string::npos)
                id = id.substr(first, last - first + 1);

            if (id.empty())
            {
                ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),
                    " %s", EditorLocale::Text("(enter an action id like side.dash)", "（输入动作 ID，如 side.dash）"));
            }
            else if (InputActionCatalog::FindDefinition(id))
            {
                // Re-adding a disabled built-in re-enables it.
                InputActionDefinition definition = *InputActionCatalog::FindDefinition(id);
                definition.Disabled = false;
                InputActionCatalog::AddDefinition(definition);
                m_NewActionId.clear();
            }
            else
            {
                InputActionDefinition definition;
                definition.Id = id;
                definition.Label = id;
                definition.DefaultKeys = {};
                InputActionCatalog::AddDefinition(definition);
                m_NewActionId.clear();
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", EditorLocale::Text(
                "Create a new input action. Give it default keys by clicking its binding row, then save.",
                "创建新的输入动作。点击其绑定行给它默认键，然后保存。"));
    }

    void InputBindingsPanel::DrawActionRow(const std::string& actionId)
    {
        ImGui::PushID(actionId.c_str());

        const InputActionDefinition* definition = InputActionCatalog::FindDefinition(actionId);
        const bool disabled = definition && definition->Disabled;

        const std::string displayName = InputActionCatalog::GetDisplayLabel(actionId);
        const std::string label = InputBindingService::GetBindingLabel(actionId);
        const bool waiting = (m_WaitingAction == actionId);

        if (disabled)
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextUnformatted(displayName.c_str());
        if (disabled)
            ImGui::PopStyleColor();
        ImGui::SameLine(230.0f);
        ImGui::SetNextItemWidth(170.0f);
        if (disabled)
        {
            ImGui::TextDisabled("%s", EditorLocale::Text("(disabled)", "（已停用）"));
        }
        else if (waiting)
        {
            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark),
                "%s", EditorLocale::Text("Press a key...", "按下按键..."));
        }
        else
        {
            const std::string buttonLabel = label.empty() ? "-" : label;
            if (ImGui::Button(buttonLabel.c_str(), ImVec2(170.0f, 0.0f)))
                m_WaitingAction = actionId;
        }

        // Right-click menu: reset to default / clear / disable / remove /
        // edit display label.
        if (!waiting && ImGui::BeginPopupContextItem("##bind_ctx"))
        {
            if (!disabled)
            {
                if (ImGui::MenuItem(EditorLocale::Text("Reset to Default", "重置为默认")))
                    InputBindingService::SetKeys(actionId, DefaultKeysFor(actionId));
                if (ImGui::MenuItem(EditorLocale::Text("Clear", "清空")))
                    InputBindingService::SetKeys(actionId, {});
                ImGui::Separator();
                if (ImGui::MenuItem(EditorLocale::Text("Edit Label...", "编辑显示名...")))
                {
                    m_LabelEditAction = actionId;
                    const std::string currentLabel = InputActionCatalog::GetDisplayLabel(actionId);
                    std::strncpy(m_LabelEditBuffer, currentLabel.c_str(), sizeof(m_LabelEditBuffer) - 1);
                    m_LabelEditBuffer[sizeof(m_LabelEditBuffer) - 1] = '\0';
                    m_ShowLabelEdit = true;
                }
            }
            if (definition)
            {
                if (disabled)
                {
                    if (ImGui::MenuItem(EditorLocale::Text("Restore", "恢复")))
                    {
                        InputActionDefinition restored = *definition;
                        restored.Disabled = false;
                        InputActionCatalog::AddDefinition(restored);
                    }
                }
                else if (ImGui::MenuItem(EditorLocale::Text("Disable", "停用")))
                {
                    InputActionCatalog::RemoveDefinition(actionId);
                }
            }
            ImGui::EndPopup();
        }

        // Display-label editing modal (persisted with the catalog file).
        if (m_ShowLabelEdit && !m_LabelEditAction.empty())
        {
            ImGui::OpenPopup("##bind_label_edit");
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal("##bind_label_edit", nullptr,
                    ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("%s", EditorLocale::Text(
                    "Display label for '%s'", "'%s' 的显示名"),
                    m_LabelEditAction.c_str());
                ImGui::SetNextItemWidth(260.0f);
                ImGui::InputText("##label_input", m_LabelEditBuffer, sizeof(m_LabelEditBuffer));
                ImGui::Separator();
                ImGui::Spacing();
                if (ImGui::Button(EditorLocale::Text("Apply", "应用"), ImVec2(120.0f, 0.0f)))
                {
                    if (const InputActionDefinition* existing =
                            InputActionCatalog::FindDefinition(m_LabelEditAction))
                    {
                        InputActionDefinition updated = *existing;
                        updated.Label = m_LabelEditBuffer;
                        InputActionCatalog::AddDefinition(updated);
                    }
                    m_ShowLabelEdit = false;
                    m_LabelEditAction.clear();
                }
                ImGui::SameLine();
                if (ImGui::Button(EditorLocale::Text("Cancel", "取消"), ImVec2(100.0f, 0.0f)))
                {
                    m_ShowLabelEdit = false;
                    m_LabelEditAction.clear();
                }
                ImGui::EndPopup();
            }
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
            m_WaitingAction.clear();   // Esc finishes/cancels
            return;
        }

        // Multi-key bindings: keep waiting and append each pressed key to the
        // existing set (duplicates are ignored). Esc finishes the sequence.
        std::vector<int> keys = InputBindingService::GetKeys(actionId);
        if (std::find(keys.begin(), keys.end(), binding) == keys.end())
        {
            keys.push_back(binding);
            InputBindingService::SetKeys(actionId, keys);
        }
    }

} // namespace Wheatear
