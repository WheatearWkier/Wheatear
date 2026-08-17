#include "wepch.h"
#include "Wheatear/Utils/StringUtils.h"
#include "VisualNovelDrawer.h"
#include "Wheatear/Modules/VisualNovel/VisualNovelComponents.h"

#include "Editor/EditorContentPickers.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Modules/VisualNovel/VisualNovelScriptEditorPanel.h"
#include "Panels/SceneHierarchy/ComponentDrawers.h"
#include "Wheatear/Gameplay/SystemBindingRegistry.h"
#include "Wheatear/Scene/Components.h"

#include <imgui/imgui.h>

#include <vector>

namespace Wheatear {

    namespace {


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
                if (!StringUtils::StartsWith(tag, SystemBindings::VisualNovel::RootPrefix))
                    continue;

                bool visible = visiblePrefixes.empty();
                for (const std::string& prefix : visiblePrefixes)
                {
                    if (StringUtils::StartsWith(tag, prefix))
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
                if (ImGui::Button(EditorLocale::Text("Open VN Script Editor", "打开视觉小说脚本编辑器")))
                    VisualNovelEditorRequests::RequestOpenScript(component.ScriptPath);
                ImGui::SameLine();
                EditorWidgets::StatusBadge("Edits Asset", EditorWidgets::StatusKind::Info);
                ImGui::DragFloat("Characters / Second", &component.CharactersPerSecond, 1.0f, 1.0f, 240.0f);
                ImGui::Checkbox(EditorLocale::Text("Play On Start", "开始时播放"), &component.PlayOnStart);
                ImGui::Checkbox(EditorLocale::Text("Restart On Finish", "结束后重播"), &component.RestartOnFinish);

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
                ImGui::DragFloat(EditorLocale::Text("Auto Play Delay", "自动播放延迟"), &component.AutoPlayDelay, 0.05f, 0.2f, 10.0f);
                EditorContentPickers::DrawSceneEntityField("History Text", entity, component.HistoryTextEntityName);
                EditorContentPickers::DrawSceneEntityField("Auto Play Indicator", entity, component.AutoPlayIndicatorEntityName);
                EditorContentPickers::DrawSceneEntityField("Command Bar", entity, component.CommandBarEntityName);
                EditorContentPickers::DrawSceneEntityField("Command Tooltip", entity, component.CommandTooltipEntityName);
                ImGui::Checkbox(EditorLocale::Text("Command Tooltip Follow Mouse", "命令提示跟随鼠标"), &component.CommandTooltipFollowMouse);
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

                ImGui::Separator();
                ImGui::TextDisabled("Editor UI Preview");
                if (ImGui::Button(EditorLocale::Text("Show All VN UI", "显示全部 VN 界面")))
                    SetVNEditorPreview(entity, {});
                ImGui::SameLine();
                if (ImGui::Button(EditorLocale::Text("Hide Auxiliary Pages", "隐藏辅助页")))
                {
                    SetVNEditorPreview(entity, {
                        SystemBindings::VisualNovel::DialoguePanel,
                        SystemBindings::VisualNovel::SpeakerText,
                        SystemBindings::VisualNovel::BodyText,
                        SystemBindings::VisualNovel::AdvanceHint,
                        SystemBindings::VisualNovel::ChoicePrefix,
                        SystemBindings::VisualNovel::CommandPrefix,
                        SystemBindings::VisualNovel::AutoPlayIndicator,
                        SystemBindings::VisualNovel::SystemMessage
                    });
                }

                if (ImGui::Button(EditorLocale::Text("Show History Page", "显示历史页")))
                    SetVNEditorPreview(entity, { SystemBindings::VisualNovel::HistoryPrefix });
                ImGui::SameLine();
                if (ImGui::Button(EditorLocale::Text("Show Settings Page", "显示设置页")))
                    SetVNEditorPreview(entity, { SystemBindings::VisualNovel::SettingsPrefix });
                ImGui::SameLine();
                if (ImGui::Button(EditorLocale::Text("Show Save/Load Page", "显示存档页")))
                    SetVNEditorPreview(entity, { SystemBindings::VisualNovel::SaveLoadPrefix });
                if (ImGui::Button(EditorLocale::Text("Show Music Notice", "显示音乐提示")))
                    SetVNEditorPreview(entity, { SystemBindings::VisualNovel::MusicNoticePrefix });
            });
    }

} // namespace Wheatear
