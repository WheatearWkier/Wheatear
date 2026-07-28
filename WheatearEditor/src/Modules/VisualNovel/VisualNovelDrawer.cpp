#include "VisualNovelDrawer.h"

#include "Panels/SceneHierarchy/ComponentDrawers.h"
#include "Wheatear/Scene/Components.h"

#include <imgui/imgui.h>

#include <cstring>
#include <vector>

namespace Wheatear {

    namespace {

        static bool InputString(const char* label, std::string& value, size_t capacity = 256)
        {
            std::vector<char> buffer(capacity, 0);
            strncpy_s(buffer.data(), buffer.size(), value.c_str(), _TRUNCATE);
            if (ImGui::InputText(label, buffer.data(), buffer.size()))
            {
                value = buffer.data();
                return true;
            }
            return false;
        }

    }

    void DrawVisualNovelComponent(Entity entity)
    {
        DrawComponent<VisualNovelComponent>("Visual Novel", entity, [](auto& component)
            {
                InputString("Script Path", component.ScriptPath);
                ImGui::DragFloat("Characters / Second", &component.CharactersPerSecond, 1.0f, 1.0f, 240.0f);
                ImGui::Checkbox("Play On Start", &component.PlayOnStart);
                ImGui::Checkbox("Restart On Finish", &component.RestartOnFinish);

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
                InputString("System Message", component.SystemMessageEntityName);
                InputString("Save Directory", component.SaveDirectory);
                ImGui::DragInt("Auto Load Slot", &component.AutoLoadSlot, 1.0f, 0, 9);
            });
    }

} // namespace Wheatear
