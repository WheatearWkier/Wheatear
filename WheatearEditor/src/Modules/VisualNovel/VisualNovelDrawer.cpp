#include "VisualNovelDrawer.h"

#include "Editor/EditorContentPickers.h"
#include "Editor/EditorWidgets.h"
#include "Modules/VisualNovel/VisualNovelScriptEditorPanel.h"
#include "Panels/SceneHierarchy/ComponentDrawers.h"
#include "Wheatear/Scene/Components.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <vector>

namespace Wheatear {

    namespace {

        static bool StartsWith(const std::string& value, const std::string& prefix)
        {
            return value.rfind(prefix, 0) == 0;
        }

        static void SetVNEditorPreview(Entity controller,
            const std::vector<std::string>& visiblePrefixes)
        {
            Scene* scene = controller.GetScene();
            if (!scene)
                return;

            auto& registry = scene->GetRegistry();
            for (auto entityID : registry.view<TagComponent, UIWidgetComponent>())
            {
                auto& tag = registry.get<TagComponent>(entityID).Tag;
                if (!StartsWith(tag, "VN_"))
                    continue;

                bool visible = visiblePrefixes.empty();
                for (const std::string& prefix : visiblePrefixes)
                {
                    if (StartsWith(tag, prefix))
                    {
                        visible = true;
                        break;
                    }
                }
                registry.get<UIWidgetComponent>(entityID).EditorVisible = visible;
            }
        }

    }

    void DrawVisualNovelComponent(Entity entity)
    {
        DrawComponent<VisualNovelComponent>("Visual Novel", entity, [entity](auto& component)
            {
                EditorContentPickers::DrawAssetField("Script Path",
                    component.ScriptPath,
                    EditorWidgets::AssetReferenceKind::Script);
                if (ImGui::Button("Open VN Script Editor"))
                    VisualNovelEditorRequests::RequestOpenScript(component.ScriptPath);
                ImGui::SameLine();
                EditorWidgets::StatusBadge("Edits Asset", EditorWidgets::StatusKind::Info);
                ImGui::DragFloat("Characters / Second", &component.CharactersPerSecond, 1.0f, 1.0f, 240.0f);
                ImGui::Checkbox("Play On Start", &component.PlayOnStart);
                ImGui::Checkbox("Restart On Finish", &component.RestartOnFinish);

                ImGui::Separator();
                ImGui::TextDisabled("Scene Bindings");
                EditorContentPickers::DrawSceneEntityField("Speaker Text", entity, component.SpeakerTextEntityName);
                EditorContentPickers::DrawSceneEntityField("Body Text", entity, component.BodyTextEntityName);
                EditorContentPickers::DrawSceneEntityField("Advance Hint", entity, component.AdvanceHintEntityName);
                EditorContentPickers::DrawSceneEntityField("Background", entity, component.BackgroundEntityName);
                EditorContentPickers::DrawSceneEntityField("Floor", entity, component.FloorEntityName);
                EditorWidgets::InputString("Character Prefix", component.CharacterEntityPrefix);
                EditorWidgets::InputString("Choice Prefix", component.ChoiceEntityPrefix);

                int maxChoices = static_cast<int>(component.MaxVisibleChoices);
                if (ImGui::DragInt("Max Choices", &maxChoices, 1.0f, 1, 9))
                    component.MaxVisibleChoices = static_cast<uint32_t>(maxChoices);

                ImGui::Separator();
                ImGui::TextDisabled("Runtime Controls");
                ImGui::Checkbox("Auto Play On Start", &component.AutoPlayOnStart);
                ImGui::DragFloat("Auto Play Delay", &component.AutoPlayDelay, 0.05f, 0.2f, 10.0f);
                EditorContentPickers::DrawSceneEntityField("History Text", entity, component.HistoryTextEntityName);
                EditorContentPickers::DrawSceneEntityField("Auto Play Indicator", entity, component.AutoPlayIndicatorEntityName);
                EditorContentPickers::DrawSceneEntityField("Command Bar", entity, component.CommandBarEntityName);
                EditorContentPickers::DrawSceneEntityField("Command Tooltip", entity, component.CommandTooltipEntityName);
                ImGui::Checkbox("Command Tooltip Follow Mouse", &component.CommandTooltipFollowMouse);
                ImGui::DragFloat2("Command Tooltip Mouse Offset", &component.CommandTooltipMouseOffset.x, 0.001f, -1.0f, 1.0f, "%.3f");
                EditorContentPickers::DrawSceneEntityField("History Panel", entity, component.HistoryPanelEntityName);
                EditorContentPickers::DrawSceneEntityField("History Scroll", entity, component.HistoryScrollEntityName);
                EditorContentPickers::DrawSceneEntityField("Settings Panel", entity, component.SettingsPanelEntityName);
                EditorContentPickers::DrawSceneEntityField("Settings Text", entity, component.SettingsTextEntityName);
                EditorContentPickers::DrawSceneEntityField("Save Load Panel", entity, component.SaveLoadPanelEntityName);
                EditorContentPickers::DrawSceneEntityField("Save Load Text", entity, component.SaveLoadTextEntityName);
                EditorContentPickers::DrawSceneEntityField("System Message", entity, component.SystemMessageEntityName);
                EditorContentPickers::DrawSceneEntityField("Music Notice Panel", entity, component.MusicNoticePanelEntityName);
                EditorContentPickers::DrawSceneEntityField("Music Notice Text", entity, component.MusicNoticeTextEntityName);
                EditorWidgets::InputString("Save Directory", component.SaveDirectory);
                ImGui::DragInt("Auto Load Slot", &component.AutoLoadSlot, 1.0f, 0, 9);

                ImGui::Separator();
                ImGui::TextDisabled("Editor UI Preview");
                if (ImGui::Button("Show All VN UI"))
                    SetVNEditorPreview(entity, {});
                ImGui::SameLine();
                if (ImGui::Button("Hide Auxiliary Pages"))
                {
                    SetVNEditorPreview(entity, {
                        "VN_DialoguePanel",
                        "VN_SpeakerText",
                        "VN_BodyText",
                        "VN_AdvanceHint",
                        "VN_Choice",
                        "VN_Command",
                        "VN_AutoPlayIndicator",
                        "VN_SystemMessage"
                    });
                }

                if (ImGui::Button("Show History Page"))
                    SetVNEditorPreview(entity, { "VN_History" });
                ImGui::SameLine();
                if (ImGui::Button("Show Settings Page"))
                    SetVNEditorPreview(entity, { "VN_Settings" });
                ImGui::SameLine();
                if (ImGui::Button("Show Save/Load Page"))
                    SetVNEditorPreview(entity, { "VN_SaveLoad" });
                if (ImGui::Button("Show Music Notice"))
                    SetVNEditorPreview(entity, { "VN_MusicNotice" });
            });
    }

} // namespace Wheatear
