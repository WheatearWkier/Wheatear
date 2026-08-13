#include "wepch.h"
#include "AnimationEditorPanel.h"

#include "ContentBrowserPanel.h"
#include "Editor/EditorCommands.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "SpriteSheetPickerPanel.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Scene/Components.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <cfloat>

namespace Wheatear {

    namespace {

        using EditorWidgets::InputString;

    } // namespace

    void AnimationEditorPanel::DrawAtlasSection()
    {
        auto clip = GetCurrentClip();
        if (!clip)
            return;

        if (!ImGui::CollapsingHeader(EditorLocale::Text("Spritesheet / Atlas Generator", "序列帧 / 图集生成器")))
            return;

        AtlasConfig& atlas = m_AtlasConfigs[m_CurrentClipName];
        ImGui::TextDisabled("Preferred runtime path: one spritesheet texture + UV frames for fewer texture swaps.");
        if (ImGui::Button(EditorLocale::Text("Open Sprite Sheet Picker", "打开序列帧选择器")))
            SpriteSheetPickerPanel::RequestOpen(m_Entity);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("Use the visual atlas picker for cell selection and frame sequence generation.");

        ImTextureID atlasThumb = atlas.Texture
            ? static_cast<ImTextureID>(static_cast<uintptr_t>(atlas.Texture->GetRendererID()))
            : static_cast<ImTextureID>(0);

        ImGui::Image(atlasThumb, ImVec2(60, 60), ImVec2(0, 1), ImVec2(1, 0));
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
            {
                const wchar_t* wpath = static_cast<const wchar_t*>(payload->Data);
                atlas.Texture = Texture2D::Create(AssetPath::ToProjectRelative(
                    GetEditorAssetPath() / wpath).generic_string());
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::SetNextItemWidth(60);
        ImGui::DragInt(EditorLocale::Text("Cols", "列数"), &atlas.Cols, 1, 1, 64);
        ImGui::SetNextItemWidth(60);
        ImGui::DragInt(EditorLocale::Text("Rows", "行数"), &atlas.Rows, 1, 1, 64);
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::SetNextItemWidth(60);
        ImGui::DragInt(EditorLocale::Text("Start Col", "起始列"), &atlas.StartCol, 1, 0, 63);
        ImGui::SetNextItemWidth(60);
        ImGui::DragInt(EditorLocale::Text("Start Row", "起始行"), &atlas.StartRow, 1, 0, 63);
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::SetNextItemWidth(60);
        ImGui::DragInt(EditorLocale::Text("Frame Count", "帧数"), &atlas.FrameCount, 1, 1, 256);
        ImGui::SetNextItemWidth(60);
        ImGui::DragFloat(EditorLocale::Text("Dur/frame", "时长/帧"), &atlas.Duration, 0.01f, 0.01f, 2.0f, "%.2fs");
        ImGui::EndGroup();

        const bool canGenerate = atlas.Texture && atlas.Cols > 0 && atlas.Rows > 0 && atlas.FrameCount > 0;
        if (!canGenerate)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }

        if (ImGui::Button(EditorLocale::Text("Generate Frames", "生成帧")))
        {
            const std::string clipName = m_CurrentClipName;
            const AtlasConfig generation = atlas; // copy; mutation runs against component data
            ApplyAnimatorEdit(m_Entity,
                [clipName, generation](SpriteAnimatorComponent& component)
            {
                auto it = component.Clips.find(clipName);
                if (it == component.Clips.end() || !generation.Texture)
                    return;
                auto clip = it->second;
                clip->ClearFrames();
                const float uStep = 1.0f / generation.Cols;
                const float vStep = 1.0f / generation.Rows;
                int col = generation.StartCol;
                int row = generation.StartRow;
                for (int i = 0; i < generation.FrameCount; i++)
                {
                    glm::vec2 uvMin = { col * uStep, row * vStep };
                    glm::vec2 uvMax = { (col + 1) * uStep, (row + 1) * vStep };
                    clip->AddFrame({ generation.Texture, uvMin, uvMax, generation.Duration });
                    if (++col >= generation.Cols)
                    {
                        col = 0;
                        row++;
                    }
                }
            });
        }

        if (!canGenerate)
        {
            ImGui::PopStyleVar();
            ImGui::PopItemFlag();
            ImGui::SameLine();
            ImGui::TextDisabled("(drop a texture first)");
        }
    }

    void AnimationEditorPanel::DrawEventsSection()
    {
        auto clip = GetCurrentClip();
        if (!clip)
            return;

        if (!ImGui::CollapsingHeader(EditorLocale::Text("Events", "事件"), ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto& events = clip->GetEvents();
        if (ImGui::Button(EditorLocale::Text("+ Event At Cursor", "+ 在光标处添加事件")))
        {
            const std::string clipName = m_CurrentClipName;
            const float eventTime = std::max(0.0f, m_PlaybackTime);
            ApplyAnimatorEdit(m_Entity,
                [clipName, eventTime](SpriteAnimatorComponent& component)
            {
                auto it = component.Clips.find(clipName);
                if (it == component.Clips.end())
                    return;
                AnimationEvent event;
                event.Time = eventTime;
                event.Name = "event";
                event.Command = "";
                it->second->AddEvent(event);
            });
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Use commands like anim:play:@UUID:attack, event:foo, or event:@UUID:foo");

        int eventToDelete = -1;
        bool sortEvents = false;
        if (ImGui::BeginTable("##animation_events", 5,
            ImGuiTableFlags_BordersInnerV
            | ImGuiTableFlags_RowBg
            | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 28.0f);
            ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 92.0f);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.75f);
            ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch, 1.7f);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 34.0f);
            ImGui::TableHeadersRow();

            const float duration = std::max(clip->GetTotalDuration(), 1.0f);
            for (int i = 0; i < static_cast<int>(events.size()); ++i)
            {
                auto& event = events[i];
                ImGui::PushID(60000 + i);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", i + 1);

                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::DragFloat("##time", &event.Time, 0.01f, 0.0f, duration, "%.3fs"))
                    sortEvents = true;

                ImGui::TableSetColumnIndex(2);
                ImGui::SetNextItemWidth(-FLT_MIN);
                InputString("##name", event.Name, 128);

                ImGui::TableSetColumnIndex(3);
                ImGui::SetNextItemWidth(-FLT_MIN);
                InputString("##command", event.Command, 320);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(EditorLocale::Text("Placeholders: {entity} {clip} {event}", "占位符: {entity} {clip} {event}"));

                ImGui::TableSetColumnIndex(4);
                if (ImGui::SmallButton("X"))
                    eventToDelete = i;

                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        if (eventToDelete >= 0)
        {
            const int deleteIndex = eventToDelete;
            const std::string clipName = m_CurrentClipName;
            ApplyAnimatorEdit(m_Entity,
                [clipName, deleteIndex](SpriteAnimatorComponent& component)
            {
                auto it = component.Clips.find(clipName);
                if (it != component.Clips.end())
                    it->second->RemoveEvent(deleteIndex);
            });
        }
        else if (sortEvents)
            clip->SortEvents();
    }

    void AnimationEditorPanel::DrawFramesSection()
    {
        auto clip = GetCurrentClip();
        if (!clip)
            return;

        if (!ImGui::CollapsingHeader(EditorLocale::Text("Frames", "帧")))
            return;

        auto& frames = clip->GetFrames();
        int frameToDelete = -1;

        for (int i = 0; i < static_cast<int>(frames.size()); i++)
        {
            ImGui::PushID(i);
            auto& frame = frames[i];

            ImTextureID thumbID = frame.Texture
                ? static_cast<ImTextureID>(static_cast<uintptr_t>(frame.Texture->GetRendererID()))
                : static_cast<ImTextureID>(0);

            ImGui::Image(thumbID, ImVec2(48, 48),
                ImVec2(frame.TexCoordMin.x, frame.TexCoordMax.y),
                ImVec2(frame.TexCoordMax.x, frame.TexCoordMin.y));

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                {
                    const wchar_t* wpath = static_cast<const wchar_t*>(payload->Data);
                    frame.Texture = Texture2D::Create(AssetPath::ToProjectRelative(
                        GetEditorAssetPath() / wpath).generic_string());
                    frame.TexCoordMin = { 0.0f, 0.0f };
                    frame.TexCoordMax = { 1.0f, 1.0f };
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(EditorLocale::Text("Drop texture here", "拖入纹理"));

            ImGui::SameLine();
            ImGui::BeginGroup();

            ImGui::TextDisabled(EditorLocale::Text("Frame %d", "帧 %d"), i);
            ImGui::SetNextItemWidth(80.0f);
            ImGui::DragFloat(EditorLocale::Text("Duration##dur", "时长##dur"), &frame.Duration, 0.01f, 0.01f, 5.0f, "%.2fs");

            if (ImGui::TreeNode("UV##uv"))
            {
                ImGui::SetNextItemWidth(120.0f);
                ImGui::DragFloat2("Min##uvmin", glm::value_ptr(frame.TexCoordMin), 0.005f, 0.0f, 1.0f, "%.3f");
                ImGui::SetNextItemWidth(120.0f);
                ImGui::DragFloat2("Max##uvmax", glm::value_ptr(frame.TexCoordMax), 0.005f, 0.0f, 1.0f, "%.3f");
                ImGui::TreePop();
            }

            ImGui::EndGroup();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
            if (ImGui::Button("X##delframe"))
                frameToDelete = i;
            ImGui::PopStyleColor();

            ImGui::Separator();
            ImGui::PopID();
        }

        if (frameToDelete >= 0)
        {
            const int deleteIndex = frameToDelete;
            const std::string clipName = m_CurrentClipName;
            ApplyAnimatorEdit(m_Entity,
                [clipName, deleteIndex](SpriteAnimatorComponent& component)
            {
                auto it = component.Clips.find(clipName);
                if (it != component.Clips.end())
                {
                    auto& frameList = it->second->GetFrames();
                    if (deleteIndex >= 0 && deleteIndex < (int)frameList.size())
                        frameList.erase(frameList.begin() + deleteIndex);
                }
            });
        }

        if (ImGui::Button(EditorLocale::Text("+ Add Frame", "+ 添加帧")))
        {
            const std::string clipName = m_CurrentClipName;
            ApplyAnimatorEdit(m_Entity,
                [clipName](SpriteAnimatorComponent& component)
            {
                auto it = component.Clips.find(clipName);
                if (it != component.Clips.end())
                    it->second->AddFrame(AnimationFrame{});
            });
        }

        ImGui::SameLine();
        ImGui::TextDisabled("%d frame(s)", static_cast<int>(frames.size()));
    }

} // namespace Wheatear
