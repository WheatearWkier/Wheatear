#include "ScriptDrawer.h"
#include "Wheatear/Core/EngineInfo.h"

#include "../ComponentDrawers.h"

#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Editor/EventScriptGraphPanel.h"
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

    namespace {

        struct ScriptSelectorState
        {
            std::array<char, 128> Search{};
        };

        static std::unordered_map<uint64_t, ScriptSelectorState> s_SelectorStates;
        using EditorWidgets::InputString;

        static std::string ToLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        static bool ContainsCaseInsensitive(const std::string& text, const char* filter)
        {
            if (!filter || filter[0] == '\0')
                return true;

            return ToLower(text).find(ToLower(filter)) != std::string::npos;
        }

        static const char* FieldTypeLabel(ScriptFieldType type)
        {
            switch (type)
            {
            case ScriptFieldType::Float:   return "float";
            case ScriptFieldType::Double:  return "double";
            case ScriptFieldType::Bool:    return "bool";
            case ScriptFieldType::Byte:    return "byte";
            case ScriptFieldType::Short:   return "short";
            case ScriptFieldType::Int:     return "int";
            case ScriptFieldType::Long:    return "long";
            case ScriptFieldType::Vector2: return "Vector2";
            case ScriptFieldType::Vector3: return "Vector3";
            case ScriptFieldType::Vector4: return "Vector4";
            case ScriptFieldType::String:  return "string";
            default:                       return "unsupported";
            }
        }

        static std::vector<std::string> SortedFieldNames(const std::unordered_map<std::string, ScriptField>& fields)
        {
            std::vector<std::string> names;
            names.reserve(fields.size());
            for (const auto& [name, field] : fields)
                names.push_back(name);
            std::sort(names.begin(), names.end());
            return names;
        }

        static void DrawUnsupportedField(const std::string& name, ScriptFieldType type)
        {
            ImGui::TextDisabled("%s (%s)", name.c_str(), FieldTypeLabel(type));
        }

        static void DrawRuntimeField(const std::string& name, const ScriptField& field, const Ref<ScriptInstance>& instance)
        {
            switch (field.Type)
            {
            case ScriptFieldType::Float:
            {
                float value = instance->GetFieldValue<float>(name);
                if (ImGui::DragFloat(name.c_str(), &value, 0.05f))
                    instance->SetFieldValue(name, value);
                break;
            }
            case ScriptFieldType::Double:
            {
                double value = instance->GetFieldValue<double>(name);
                if (ImGui::DragScalar(name.c_str(), ImGuiDataType_Double, &value, 0.05f))
                    instance->SetFieldValue(name, value);
                break;
            }
            case ScriptFieldType::Bool:
            {
                bool value = instance->GetFieldValue<bool>(name);
                if (ImGui::Checkbox(name.c_str(), &value))
                    instance->SetFieldValue(name, value);
                break;
            }
            case ScriptFieldType::Byte:
            {
                int value = instance->GetFieldValue<uint8_t>(name);
                if (ImGui::DragInt(name.c_str(), &value, 1.0f, 0, 255))
                    instance->SetFieldValue(name, static_cast<uint8_t>(std::clamp(value, 0, 255)));
                break;
            }
            case ScriptFieldType::Short:
            {
                int value = instance->GetFieldValue<int16_t>(name);
                if (ImGui::DragInt(name.c_str(), &value))
                    instance->SetFieldValue(name, static_cast<int16_t>(std::clamp(value, -32768, 32767)));
                break;
            }
            case ScriptFieldType::Int:
            {
                int value = instance->GetFieldValue<int32_t>(name);
                if (ImGui::DragInt(name.c_str(), &value))
                    instance->SetFieldValue(name, value);
                break;
            }
            case ScriptFieldType::Long:
            {
                int64_t value = instance->GetFieldValue<int64_t>(name);
                if (ImGui::DragScalar(name.c_str(), ImGuiDataType_S64, &value, 1.0f))
                    instance->SetFieldValue(name, value);
                break;
            }
            case ScriptFieldType::Vector2:
            {
                glm::vec2 value = instance->GetFieldValue<glm::vec2>(name);
                if (ImGui::DragFloat2(name.c_str(), glm::value_ptr(value), 0.05f))
                    instance->SetFieldValue(name, value);
                break;
            }
            case ScriptFieldType::Vector3:
            {
                glm::vec3 value = instance->GetFieldValue<glm::vec3>(name);
                if (ImGui::DragFloat3(name.c_str(), glm::value_ptr(value), 0.05f))
                    instance->SetFieldValue(name, value);
                break;
            }
            case ScriptFieldType::Vector4:
            {
                glm::vec4 value = instance->GetFieldValue<glm::vec4>(name);
                if (ImGui::DragFloat4(name.c_str(), glm::value_ptr(value), 0.05f))
                    instance->SetFieldValue(name, value);
                break;
            }
            case ScriptFieldType::String:
            {
                char buffer[512] = {};
                strncpy_s(buffer, sizeof(buffer), instance->GetStringFieldValue(name).c_str(), _TRUNCATE);
                if (ImGui::InputText(name.c_str(), buffer, sizeof(buffer)))
                    instance->SetStringFieldValue(name, buffer);
                break;
            }
            default:
                DrawUnsupportedField(name, field.Type);
                break;
            }
        }

        static void DrawEditField(const std::string& name, const ScriptField& field, ScriptFieldInstance& fieldInst)
        {
            fieldInst.Field = field;

            switch (field.Type)
            {
            case ScriptFieldType::Float:
            {
                float value = fieldInst.GetValue<float>();
                if (ImGui::DragFloat(name.c_str(), &value, 0.05f))
                    fieldInst.SetValue(value);
                break;
            }
            case ScriptFieldType::Double:
            {
                double value = fieldInst.GetValue<double>();
                if (ImGui::DragScalar(name.c_str(), ImGuiDataType_Double, &value, 0.05f))
                    fieldInst.SetValue(value);
                break;
            }
            case ScriptFieldType::Bool:
            {
                bool value = fieldInst.GetValue<bool>();
                if (ImGui::Checkbox(name.c_str(), &value))
                    fieldInst.SetValue(value);
                break;
            }
            case ScriptFieldType::Byte:
            {
                int value = fieldInst.GetValue<uint8_t>();
                if (ImGui::DragInt(name.c_str(), &value, 1.0f, 0, 255))
                    fieldInst.SetValue(static_cast<uint8_t>(std::clamp(value, 0, 255)));
                break;
            }
            case ScriptFieldType::Short:
            {
                int value = fieldInst.GetValue<int16_t>();
                if (ImGui::DragInt(name.c_str(), &value))
                    fieldInst.SetValue(static_cast<int16_t>(std::clamp(value, -32768, 32767)));
                break;
            }
            case ScriptFieldType::Int:
            {
                int value = fieldInst.GetValue<int32_t>();
                if (ImGui::DragInt(name.c_str(), &value))
                    fieldInst.SetValue(value);
                break;
            }
            case ScriptFieldType::Long:
            {
                int64_t value = fieldInst.GetValue<int64_t>();
                if (ImGui::DragScalar(name.c_str(), ImGuiDataType_S64, &value, 1.0f))
                    fieldInst.SetValue(value);
                break;
            }
            case ScriptFieldType::Vector2:
            {
                glm::vec2 value = fieldInst.GetValue<glm::vec2>();
                if (ImGui::DragFloat2(name.c_str(), glm::value_ptr(value), 0.05f))
                    fieldInst.SetValue(value);
                break;
            }
            case ScriptFieldType::Vector3:
            {
                glm::vec3 value = fieldInst.GetValue<glm::vec3>();
                if (ImGui::DragFloat3(name.c_str(), glm::value_ptr(value), 0.05f))
                    fieldInst.SetValue(value);
                break;
            }
            case ScriptFieldType::Vector4:
            {
                glm::vec4 value = fieldInst.GetValue<glm::vec4>();
                if (ImGui::DragFloat4(name.c_str(), glm::value_ptr(value), 0.05f))
                    fieldInst.SetValue(value);
                break;
            }
            case ScriptFieldType::String:
            {
                char buffer[512] = {};
                strncpy_s(buffer, sizeof(buffer), fieldInst.GetStringValue().c_str(), _TRUNCATE);
                if (ImGui::InputText(name.c_str(), buffer, sizeof(buffer)))
                    fieldInst.SetStringValue(buffer);
                break;
            }
            default:
                DrawUnsupportedField(name, field.Type);
                break;
            }
        }

        static void DrawScriptClassSelector(Entity entity, ScriptComponent& component)
        {
            const uint64_t entityID = static_cast<uint64_t>(entity.GetUUID());
            ScriptSelectorState& state = s_SelectorStates[entityID];

            const std::vector<std::string> classNames = ScriptEngine::GetEntityClassNames();
            const bool classMissing = !component.ClassName.empty() && !ScriptEngine::EntityClassExists(component.ClassName);
            const char* preview = component.ClassName.empty() ? "None" : component.ClassName.c_str();

            if (classMissing)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.28f, 0.22f, 1.0f));

            if (ImGui::BeginCombo("Class", preview))
            {
                ImGui::InputTextWithHint("##ScriptSearch", "Search script class...", state.Search.data(), state.Search.size());
                ImGui::Separator();

                if (ImGui::Selectable("None", component.ClassName.empty()))
                {
                    component.ClassName.clear();
                    ScriptEngine::ClearScriptFieldMap(entity);
                }

                bool anyVisible = false;
                for (size_t i = 0; i < classNames.size(); ++i)
                {
                    const std::string& className = classNames[i];
                    if (!ContainsCaseInsensitive(className, state.Search.data()))
                        continue;

                    anyVisible = true;
                    const bool selected = component.ClassName == className;
                    const std::string label = EditorWidgets::LabelWithId(
                        className,
                        "script_class:" + std::to_string(i));
                    if (ImGui::Selectable(label.c_str(), selected))
                    {
                        component.ClassName = className;
                        ScriptEngine::ClearScriptFieldMap(entity);
                        ScriptEngine::InitializeScriptFieldMap(entity);
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }

                if (!anyVisible && classNames.empty())
                    ImGui::TextDisabled(EditorLocale::Text("No script classes loaded.", "未加载脚本类。"));
                else if (!anyVisible)
                    ImGui::TextDisabled(EditorLocale::Text("No matches.", "无匹配。"));

                ImGui::EndCombo();
            }

            if (classMissing)
            {
                ImGui::PopStyleColor();
                ImGui::TextDisabled((std::string("Class not found in ") + EngineInfo::ScriptCoreAssemblyName + ".dll").c_str());
            }

            if (!component.ClassName.empty())
            {
                ImGui::SameLine();
                if (ImGui::SmallButton(EditorLocale::Text("Clear", "清空")))
                {
                    component.ClassName.clear();
                    ScriptEngine::ClearScriptFieldMap(entity);
                }
            }
        }

    } // namespace

    void DrawScriptComponent(Entity entity)
    {
        DrawComponent<ScriptComponent>("Script", entity, [entity](auto& component)
        {
            ImGui::PushID(static_cast<int>(static_cast<uint32_t>(entity.GetUUID())));

            DrawScriptClassSelector(entity, component);

            if (component.ClassName.empty())
            {
                ImGui::PopID();
                return;
            }

            if (!ScriptEngine::EntityClassExists(component.ClassName))
            {
                ImGui::PopID();
                return;
            }

            Ref<ScriptClass> scriptClass = ScriptEngine::GetEntityClass(component.ClassName);
            if (!scriptClass)
            {
                ImGui::PopID();
                return;
            }

            const auto& fields = scriptClass->GetFields();
            if (fields.empty())
            {
                ImGui::TextDisabled(EditorLocale::Text("No public editable fields.", "无公开可编辑字段。"));
                ImGui::PopID();
                return;
            }

            ScriptEngine::InitializeScriptFieldMap(entity);
            auto& entityFields = ScriptEngine::GetScriptFieldMap(entity);
            Ref<ScriptInstance> scriptInstance = ScriptEngine::GetEntityScriptInstance(entity.GetUUID());

            ImGui::Spacing();
            ImGui::TextDisabled(scriptInstance ? "Runtime Fields" : "Serialized Fields");
            ImGui::Separator();

            for (const std::string& name : SortedFieldNames(fields))
            {
                const ScriptField& field = fields.at(name);
                if (scriptInstance)
                    DrawRuntimeField(name, field, scriptInstance);
                else
                    DrawEditField(name, field, entityFields[name]);
            }

            ImGui::PopID();
        });
    }

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

            ImGui::PopID();
        });
    }

} // namespace Wheatear
