#include "wepch.h"
#include "AudioDrawer.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Audio/AudioEngine.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Assets/AssetPath.h"
#include <imgui/imgui.h>
#include <filesystem>

namespace Wheatear {

    void DrawAudioSourceComponent(Entity entity)
    {
        if (!entity.HasComponent<AudioSourceComponent>()) return;
        auto& asc = entity.GetComponent<AudioSourceComponent>();

        bool open = ImGui::TreeNodeEx("AudioSourceComponent",
            ImGuiTreeNodeFlags_DefaultOpen |
            ImGuiTreeNodeFlags_Framed      |
            ImGuiTreeNodeFlags_SpanAvailWidth);

        if (open)
        {
            ImGui::PushID((int)(uint32_t)entity);
            EditorWidgets::DrawAssetReferenceField("Audio File",
                asc.AudioFilePath,
                EditorWidgets::AssetReferenceKind::Audio,
                256);

            // Drag and drop audio assets.
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload =
                    ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                {
                    const wchar_t* wpath = (const wchar_t*)payload->Data;
                    std::filesystem::path dropped(wpath);
                    auto ext = dropped.extension().string();

                    if (ext == ".wav" || ext == ".mp3" ||
                        ext == ".ogg" || ext == ".flac")
                    {
                        asc.AudioFilePath = AssetPath::ToProjectRelative(
                            AssetPath::GetAssetRoot() / dropped).generic_string();
                        WT_CORE_INFO("AudioFilePath set to: {0}", asc.AudioFilePath);
                    }
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::PopID();

            if (asc.AudioFilePath.empty())
                ImGui::TextDisabled(EditorLocale::Text("Choose or drag an audio asset.", "选择或拖入音频资源。"));

            // Volume
            ImGui::SliderFloat(EditorLocale::Text("Volume", "音量"), &asc.Volume, 0.0f, 1.0f);

            // Loop and PlayOnStart
            ImGui::Checkbox(EditorLocale::Text("Loop", "循环"), &asc.Loop);
            ImGui::SameLine();
            ImGui::Checkbox(EditorLocale::Text("Play On Start", "开始时播放"), &asc.PlayOnStart);

            ImGui::TreePop();
        }
    }

} // namespace Wheatear
