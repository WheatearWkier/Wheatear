#include "wepch.h"
#include "Wheatear/Utils/StringUtils.h"
#include "ScriptDrawer.h"
#include "Wheatear/Core/EngineInfo.h"

#include "../ComponentDrawers.h"

#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Panels/EventScriptGraphPanel.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Scripting/EventScript.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scripting/ScriptEngine.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear {

    using EditorWidgets::InputString;

    void DrawEventScriptComponent(Entity entity)
    {
        DrawComponent<EventScriptComponent>("Event Script", entity, [entity](auto& component)
        {
            ImGui::PushID(static_cast<int>(static_cast<uint32_t>(entity.GetUUID())));

            InputString(EditorLocale::Text("Script Path", "脚本路径"), component.ScriptPath, 320);
            InputString(EditorLocale::Text("Start Event", "起始事件"), component.StartEvent, 128);
            ImGui::Checkbox(EditorLocale::Text("Enabled", "启用"), &component.Enabled);
            ImGui::Checkbox(EditorLocale::Text("Run On Start", "开始时运行"), &component.RunOnStart);
            ImGui::Checkbox(EditorLocale::Text("Run Once", "仅运行一次"), &component.RunOnce);
            if (ImGui::Button(EditorLocale::Text("Open Event Script Editor", "打开事件脚本编辑器")))
                EventScriptGraphRequests::RequestOpenScript(component.ScriptPath, component.StartEvent);
            ImGui::SameLine();
            EditorWidgets::StatusBadge("Edits Asset", EditorWidgets::StatusKind::Info);
            ImGui::SameLine();
            EditorWidgets::StatusBadge("Edits Scene", EditorWidgets::StatusKind::Success);

            ImGui::Separator();
            ImGui::TextDisabled(EditorLocale::Text("Runtime", "运行时"));
            ImGui::TextDisabled(EditorLocale::Text("Active: %s", "激活: %s"), component.RuntimeActive ? "true" : "false");
            ImGui::TextDisabled(EditorLocale::Text("Started: %s", "已开始: %s"), component.RuntimeStarted ? "true" : "false");
            ImGui::TextDisabled(EditorLocale::Text("Completed: %s", "已完成: %s"), component.RuntimeCompleted ? "true" : "false");
            ImGui::TextDisabled(EditorLocale::Text("Current Event: %s", "当前事件: %s"),
                component.RuntimeEventName.empty() ? "(none)" : component.RuntimeEventName.c_str());
            ImGui::TextDisabled(EditorLocale::Text("Instruction: %zu / Wait: %.2fs", "指令: %zu / 等待: %.2fs"),
                component.RuntimeInstructionIndex,
                component.RuntimeWaitRemaining);

            // Live "current instruction" preview: what the event script is
            // executing right now while playing.
            if (component.RuntimeActive && !component.ScriptPath.empty())
            {
                EventScript script = EventScript::FromFile(AssetPath::ResolveRuntimeData(component.ScriptPath));
                if (const EventScriptBlock* block = script.FindEvent(component.RuntimeEventName))
                {
                    if (component.RuntimeInstructionIndex < block->Instructions.size())
                    {
                        const auto& instruction = block->Instructions[component.RuntimeInstructionIndex];
                        std::string preview;
                        switch (instruction.Type)
                        {
                        case EventScriptInstructionType::Command:
                            preview = instruction.Text;
                            break;
                        case EventScriptInstructionType::Wait:
                            preview = "wait " + std::to_string(instruction.Seconds) + "s";
                            break;
                        case EventScriptInstructionType::If:
                            preview = "if " + instruction.Text;
                            break;
                        case EventScriptInstructionType::EndIf:
                            preview = "endif";
                            break;
                        }
                        if (!preview.empty())
                        {
                            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark),
                                "%s %s", EditorLocale::Text("Now:", "正在执行:"), preview.c_str());
                        }
                    }
                }
            }

            ImGui::PopID();
        });
    }

} // namespace Wheatear
