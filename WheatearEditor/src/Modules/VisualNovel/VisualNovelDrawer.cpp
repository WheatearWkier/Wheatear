#include "VisualNovelDrawer.h"

#include "Editor/EditorWidgets.h"
#include "Editor/TextAssetEditor.h"
#include "Modules/VisualNovel/VisualNovelScriptEditorPanel.h"
#include "Panels/SceneHierarchy/ComponentDrawers.h"
#include "Wheatear/Scene/Components.h"

#include <imgui/imgui.h>

#include <unordered_map>
#include <vector>

namespace Wheatear {

    namespace {

        using EditorWidgets::InputString;

        static std::unordered_map<std::string, EditorUI::TextAssetEditorState> s_ScriptEditors;

    }

    void DrawVisualNovelComponent(Entity entity)
    {
        DrawComponent<VisualNovelComponent>("Visual Novel", entity, [](auto& component)
            {
                InputString("Script Path", component.ScriptPath);
                if (ImGui::Button("Open VN Script Editor"))
                    VisualNovelEditorRequests::RequestOpenScript(component.ScriptPath);
                ImGui::DragFloat("Characters / Second", &component.CharactersPerSecond, 1.0f, 1.0f, 240.0f);
                ImGui::Checkbox("Play On Start", &component.PlayOnStart);
                ImGui::Checkbox("Restart On Finish", &component.RestartOnFinish);
                EditorUI::DrawTextAssetEditor("VN Script Editor", "VisualNovelScriptEditor", component.ScriptPath, s_ScriptEditors, 512 * 1024);

                ImGui::Separator();
                ImGui::TextDisabled("Scene Bindings");
                InputString("Speaker Text", component.SpeakerTextEntityName);
                InputString("Body Text", component.BodyTextEntityName);
                InputString("Advance Hint", component.AdvanceHintEntityName);
                InputString("Background", component.BackgroundEntityName);
                InputString("Floor", component.FloorEntityName);
                InputString("Character Prefix", component.CharacterEntityPrefix);
                InputString("Choice Prefix", component.ChoiceEntityPrefix);

                int maxChoices = static_cast<int>(component.MaxVisibleChoices);
                if (ImGui::DragInt("Max Choices", &maxChoices, 1.0f, 1, 9))
                    component.MaxVisibleChoices = static_cast<uint32_t>(maxChoices);

                ImGui::Separator();
                ImGui::TextDisabled("Runtime Controls");
                ImGui::Checkbox("Auto Play On Start", &component.AutoPlayOnStart);
                ImGui::DragFloat("Auto Play Delay", &component.AutoPlayDelay, 0.05f, 0.2f, 10.0f);
                InputString("History Text", component.HistoryTextEntityName);
                InputString("Auto Play Indicator", component.AutoPlayIndicatorEntityName);
                InputString("Command Bar", component.CommandBarEntityName);
                InputString("History Panel", component.HistoryPanelEntityName);
                InputString("Settings Panel", component.SettingsPanelEntityName);
                InputString("Settings Text", component.SettingsTextEntityName);
                InputString("Save Load Panel", component.SaveLoadPanelEntityName);
                InputString("Save Load Text", component.SaveLoadTextEntityName);
                InputString("System Message", component.SystemMessageEntityName);
                InputString("Music Notice Panel", component.MusicNoticePanelEntityName);
                InputString("Music Notice Text", component.MusicNoticeTextEntityName);
                InputString("Save Directory", component.SaveDirectory);
                ImGui::DragInt("Auto Load Slot", &component.AutoLoadSlot, 1.0f, 0, 9);
            });
    }

} // namespace Wheatear
