#include "wepch.h"
#include "SpriteSheetPickerPanel.h"

#include "ContentBrowserPanel.h"
#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorLocale.h"
#include "Wheatear/Animation/AnimationClip.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Components.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace Wheatear {

    namespace {

        bool s_HasOpenRequest = false;
        Entity s_RequestedEntity;

        ImTextureID ToImGuiTextureID(const Ref<Texture2D>& texture)
        {
            return texture
                ? static_cast<ImTextureID>(static_cast<uintptr_t>(texture->GetRendererID()))
                : static_cast<ImTextureID>(0);
        }

        bool ButtonDisabled(const char* label, bool disabled, const ImVec2& size = ImVec2(0.0f, 0.0f))
        {
            if (disabled)
            {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
            }

            const bool clicked = ImGui::Button(label, size) && !disabled;

            if (disabled)
            {
                ImGui::PopStyleVar();
                ImGui::PopItemFlag();
            }

            return clicked;
        }

        void HelpMarker(const char* text)
        {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26.0f);
                ImGui::TextUnformatted(text);
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        }

        const char* SequenceModeName(SpriteSheetPickerPanel::SequenceMode mode)
        {
            switch (mode)
            {
            case SpriteSheetPickerPanel::SequenceMode::Horizontal: return "Horizontal Strip";
            case SpriteSheetPickerPanel::SequenceMode::RowMajor: return "Row Major";
            case SpriteSheetPickerPanel::SequenceMode::Vertical: return "Vertical Strip";
            }
            return "Horizontal Strip";
        }

    } // namespace

    void SpriteSheetPickerPanel::RequestOpen(Entity entity)
    {
        s_RequestedEntity = entity;
        s_HasOpenRequest = true;
    }

    void SpriteSheetPickerPanel::SetEntity(Entity entity)
    {
        m_Entity = entity;
    }

    void SpriteSheetPickerPanel::OpenForEntity(Entity entity)
    {
        m_Entity = entity;
        m_Open = true;
        SyncTextureFromEntity();
    }

    void SpriteSheetPickerPanel::Open()
    {
        m_Open = true;
        SyncTextureFromEntity();
    }

    void SpriteSheetPickerPanel::ConsumeOpenRequest()
    {
        if (!s_HasOpenRequest)
            return;

        OpenForEntity(s_RequestedEntity);
        s_HasOpenRequest = false;
    }

    void SpriteSheetPickerPanel::OnImGuiRender()
    {
        ConsumeOpenRequest();

        if (!m_Open)
            return;

        ImGui::SetNextWindowSize(ImVec2(860.0f, 620.0f), ImGuiCond_FirstUseEver);
        if (!EditorFloatingWindow::Begin("Sprite Sheet Picker", &m_Open, 0, { 980.0f, 700.0f }))
        {
            EditorFloatingWindow::End();
            return;
        }

        EditorFloatingWindow::DrawToggleButton("Sprite Sheet Picker");
        ImGui::Separator();
        DrawTargetSummary();
        ImGui::Separator();

        ImGui::Columns(2, "##sprite_sheet_picker_columns", true);
        ImGui::SetColumnWidth(0, 560.0f);
        DrawTextureDropZone();
        DrawPreview();

        ImGui::NextColumn();
        DrawGridControls();
        ImGui::Spacing();
        DrawSelectedSpritePreview();
        ImGui::Spacing();
        DrawApplyActions();

        ImGui::Columns(1);

        if (!m_LastAction.empty())
        {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.42f, 0.88f, 0.72f, 1.0f), "%s", m_LastAction.c_str());
        }

        // Confirmation before replacing a clip's existing frames.
        if (m_ShowReplaceConfirm)
        {
            ImGui::OpenPopup(EditorLocale::Text("Replace Clip", "替换动画片段"));
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal(EditorLocale::Text("Replace Clip", "替换动画片段"),
                    nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("%s", EditorLocale::Text(
                    "This will clear the existing frames of clip '%s' (%d frame(s)). Continue?",
                    "这将清空动画片段 '%s' 的现有 %d 帧，继续吗？"),
                    m_ReplaceConfirmClipName.c_str(), m_ReplaceConfirmFrameCount);
                ImGui::Separator();
                ImGui::Spacing();
                if (ImGui::Button(EditorLocale::Text("Replace", "替换"), ImVec2(120.0f, 0.0f)))
                {
                    Ref<AnimationClip> clip = GetOrCreateTargetClip();
                    if (clip)
                        ApplySequenceToClip(clip);
                    m_ShowReplaceConfirm = false;
                }
                ImGui::SameLine();
                if (ImGui::Button(EditorLocale::Text("Cancel", "取消"), ImVec2(100.0f, 0.0f)))
                    m_ShowReplaceConfirm = false;
                ImGui::EndPopup();
            }
        }

        EditorFloatingWindow::End();
    }

    void SpriteSheetPickerPanel::ApplySequenceToClip(const Ref<AnimationClip>& clip)
    {
        if (!clip)
            return;

        if (!m_AppendFrames)
            clip->ClearFrames();

        int added = 0;
        for (int i = 0; i < m_FrameCount; ++i)
        {
            auto [col, row] = GetSequenceCell(i);
            if (!IsCellValid(col, row))
                break;

            clip->AddFrame({ m_Texture, GetCellUVMin(col, row), GetCellUVMax(col, row), m_FrameDuration });
            ++added;
        }

        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "%s %d frame(s) into clip '%s'.",
            m_AppendFrames ? "Appended" : "Generated",
            added,
            clip->GetName().c_str());
        m_LastAction = buffer;
    }

    void SpriteSheetPickerPanel::SetTexture(const Ref<Texture2D>& texture, const std::string& texturePath)
    {
        m_Texture = texture;
        m_TexturePath = texturePath;

        if (!m_Texture)
            return;

        const float longestSide = static_cast<float>(std::max(m_Texture->GetWidth(), m_Texture->GetHeight()));
        if (longestSide > 1.0f)
            m_Zoom = std::clamp(520.0f / longestSide, 0.25f, 4.0f);
    }

    void SpriteSheetPickerPanel::SyncTextureFromEntity()
    {
        if (!m_Entity)
            return;

        if (m_Entity.HasComponent<UIImageComponent>())
        {
            const auto& image = m_Entity.GetComponent<UIImageComponent>();
            if (image.Texture)
            {
                SetTexture(image.Texture, image.Texture->GetPath());
                return;
            }
        }

        if (m_Entity.HasComponent<SpriteRendererComponent>())
        {
            const auto& sprite = m_Entity.GetComponent<SpriteRendererComponent>();
            if (sprite.Texture)
            {
                SetTexture(sprite.Texture, sprite.Texture->GetPath());
                return;
            }
        }

        if (m_Entity.HasComponent<SpriteAnimatorComponent>())
        {
            const auto& animator = m_Entity.GetComponent<SpriteAnimatorComponent>();
            Ref<AnimationClip> clip = animator.GetCurrentClip();
            if (!clip && !animator.DefaultClipName.empty())
            {
                auto it = animator.Clips.find(animator.DefaultClipName);
                if (it != animator.Clips.end())
                    clip = it->second;
            }
            if (!clip && !animator.Clips.empty())
                clip = animator.Clips.begin()->second;
            if (clip && !clip->GetFrames().empty() && clip->GetFrames().front().Texture)
                SetTexture(clip->GetFrames().front().Texture, clip->GetFrames().front().Texture->GetPath());
        }
    }

    bool SpriteSheetPickerPanel::AcceptTextureDrop()
    {
        bool accepted = false;
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
            {
                const wchar_t* wpath = static_cast<const wchar_t*>(payload->Data);
                const std::filesystem::path texturePath =
                    AssetPath::ToProjectRelative(GetEditorAssetPath() / wpath);
                SetTexture(Texture2D::Create(texturePath.generic_string()), texturePath.generic_string());
                m_LastAction = "Loaded spritesheet: " + texturePath.generic_string();
                accepted = true;
            }
            ImGui::EndDragDropTarget();
        }
        return accepted;
    }

    void SpriteSheetPickerPanel::DrawTargetSummary()
    {
        ImGui::TextUnformatted(EditorLocale::Text("Target", "目标"));
        ImGui::SameLine();
        if (m_Entity)
            ImGui::TextColored(ImVec4(0.75f, 0.92f, 1.0f, 1.0f), "%s", m_Entity.GetName().c_str());
        else
            ImGui::TextDisabled("(none)");

        ImGui::SameLine();
        if (m_Entity && m_Entity.HasComponent<SpriteRendererComponent>())
            ImGui::TextColored(ImVec4(0.60f, 0.85f, 1.0f, 1.0f), "[Sprite]");
        ImGui::SameLine();
        if (m_Entity && m_Entity.HasComponent<UIImageComponent>())
            ImGui::TextColored(ImVec4(0.72f, 0.94f, 0.62f, 1.0f), "[UI Image]");
        ImGui::SameLine();
        if (m_Entity && m_Entity.HasComponent<SpriteAnimatorComponent>())
            ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.46f, 1.0f), "[Animator]");
    }

    void SpriteSheetPickerPanel::DrawTextureDropZone()
    {
        const ImVec2 dropSize(0.0f, 74.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.13f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.13f, 0.20f, 0.23f, 1.0f));
        ImGui::Button(m_Texture ? "Drop another texture here" : "Drop spritesheet texture here", dropSize);
        ImGui::PopStyleColor(2);
        AcceptTextureDrop();

        if (m_Texture)
        {
            ImGui::TextDisabled("%s  (%u x %u)",
                m_TexturePath.empty() ? m_Texture->GetPath().c_str() : m_TexturePath.c_str(),
                m_Texture->GetWidth(),
                m_Texture->GetHeight());
        }
        else
        {
            ImGui::TextDisabled(EditorLocale::Text("Drag a texture from Content Browser, then click a cell or generate a frame sequence.", "从资源浏览器拖入纹理，然后点击单元格或生成帧序列。"));
        }
    }

    void SpriteSheetPickerPanel::DrawGridControls()
    {
        ImGui::TextUnformatted(EditorLocale::Text("Grid", "网格"));

        ImGui::SetNextItemWidth(92.0f);
        if (ImGui::DragInt(EditorLocale::Text("Columns", "列数"), &m_Cols, 1, 1, 256))
            m_SelectedCol = std::clamp(m_SelectedCol, 0, m_Cols - 1);
        ImGui::SetNextItemWidth(92.0f);
        if (ImGui::DragInt(EditorLocale::Text("Rows", "行数"), &m_Rows, 1, 1, 256))
            m_SelectedRow = std::clamp(m_SelectedRow, 0, m_Rows - 1);

        ImGui::SetNextItemWidth(92.0f);
        ImGui::DragFloat(EditorLocale::Text("Zoom", "缩放"), &m_Zoom, 0.02f, 0.1f, 8.0f, "%.2fx");
        HelpMarker("Zoom only changes the editor preview, not the actual sprite data.");

        ImGui::Checkbox(EditorLocale::Text("Top-origin rows", "顶部起始行"), &m_RowOriginTop);
        HelpMarker("When enabled, Row 0 means the top visual row of the atlas. This is the usual UI-artist workflow.");

        ImGui::Separator();
        ImGui::TextUnformatted(EditorLocale::Text("Selection", "选择"));
        ImGui::SetNextItemWidth(92.0f);
        ImGui::DragInt("Column", &m_SelectedCol, 1, 0, std::max(0, m_Cols - 1));
        ImGui::SetNextItemWidth(92.0f);
        ImGui::DragInt("Row", &m_SelectedRow, 1, 0, std::max(0, m_Rows - 1));
        m_SelectedCol = std::clamp(m_SelectedCol, 0, std::max(0, m_Cols - 1));
        m_SelectedRow = std::clamp(m_SelectedRow, 0, std::max(0, m_Rows - 1));

        const glm::vec2 uvMin = GetCellUVMin(m_SelectedCol, m_SelectedRow);
        const glm::vec2 uvMax = GetCellUVMax(m_SelectedCol, m_SelectedRow);
        ImGui::TextDisabled("UV %.3f %.3f -> %.3f %.3f", uvMin.x, uvMin.y, uvMax.x, uvMax.y);

        ImGui::Separator();
        ImGui::TextUnformatted(EditorLocale::Text("Sequence", "序列"));
        int modeIndex = static_cast<int>(m_SequenceMode);
        const char* modes[] = { "Horizontal Strip", "Row Major", "Vertical Strip" };
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::Combo(EditorLocale::Text("Mode", "模式"), &modeIndex, modes, IM_ARRAYSIZE(modes)))
            m_SequenceMode = static_cast<SequenceMode>(modeIndex);
        HelpMarker("Horizontal is best for animation frames laid out left-to-right in one row. Row Major wraps to the next row.");

        ImGui::SetNextItemWidth(92.0f);
        ImGui::DragInt(EditorLocale::Text("Frame Count", "帧数"), &m_FrameCount, 1, 1, 512);
        ImGui::SetNextItemWidth(92.0f);
        ImGui::DragInt(EditorLocale::Text("Frame Step", "帧步进"), &m_FrameStep, 1, 1, 64);
        ImGui::SetNextItemWidth(92.0f);
        ImGui::DragFloat(EditorLocale::Text("Duration", "时长"), &m_FrameDuration, 0.005f, 0.01f, 2.0f, "%.3fs");
        ImGui::Checkbox(EditorLocale::Text("Append frames", "追加帧"), &m_AppendFrames);
    }

    bool SpriteSheetPickerPanel::IsCellValid(int col, int row) const
    {
        return col >= 0 && row >= 0 && col < m_Cols && row < m_Rows;
    }

    glm::vec2 SpriteSheetPickerPanel::GetCellUVMin(int col, int row) const
    {
        const float uStep = 1.0f / static_cast<float>(std::max(1, m_Cols));
        const float vStep = 1.0f / static_cast<float>(std::max(1, m_Rows));
        const float uMin = static_cast<float>(col) * uStep;
        const float vMin = m_RowOriginTop
            ? 1.0f - static_cast<float>(row + 1) * vStep
            : static_cast<float>(row) * vStep;
        return { uMin, vMin };
    }

    glm::vec2 SpriteSheetPickerPanel::GetCellUVMax(int col, int row) const
    {
        const float uStep = 1.0f / static_cast<float>(std::max(1, m_Cols));
        const float vStep = 1.0f / static_cast<float>(std::max(1, m_Rows));
        const float uMax = static_cast<float>(col + 1) * uStep;
        const float vMax = m_RowOriginTop
            ? 1.0f - static_cast<float>(row) * vStep
            : static_cast<float>(row + 1) * vStep;
        return { uMax, vMax };
    }

    std::pair<int, int> SpriteSheetPickerPanel::GetSequenceCell(int frameIndex) const
    {
        const int step = std::max(1, m_FrameStep);
        const int offset = frameIndex * step;

        switch (m_SequenceMode)
        {
        case SequenceMode::Horizontal:
            return { m_SelectedCol + offset, m_SelectedRow };
        case SequenceMode::Vertical:
            return { m_SelectedCol, m_SelectedRow + offset };
        case SequenceMode::RowMajor:
        default:
        {
            const int start = m_SelectedRow * std::max(1, m_Cols) + m_SelectedCol;
            const int index = start + offset;
            return { index % std::max(1, m_Cols), index / std::max(1, m_Cols) };
        }
        }
    }

    void SpriteSheetPickerPanel::DrawPreview()
    {
        ImGui::BeginChild("##sprite_sheet_preview", ImVec2(0.0f, 390.0f), true, ImGuiWindowFlags_HorizontalScrollbar);

        if (!m_Texture)
        {
            ImGui::TextDisabled(EditorLocale::Text("No spritesheet loaded.", "未加载序列帧图。"));
            ImGui::EndChild();
            return;
        }

        const float imageWidth = std::max(1.0f, static_cast<float>(m_Texture->GetWidth()) * m_Zoom);
        const float imageHeight = std::max(1.0f, static_cast<float>(m_Texture->GetHeight()) * m_Zoom);
        const ImVec2 imageSize(imageWidth, imageHeight);

        ImGui::Image(ToImGuiTextureID(m_Texture), imageSize, ImVec2(0, 1), ImVec2(1, 0));
        AcceptTextureDrop();

        const ImVec2 imageMin = ImGui::GetItemRectMin();
        const ImVec2 imageMax = ImGui::GetItemRectMax();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(imageMin, imageMax, true);

        const ImU32 gridColor = IM_COL32(110, 180, 190, 120);
        const ImU32 selectedColor = IM_COL32(255, 220, 95, 255);
        const ImU32 sequenceColor = IM_COL32(75, 220, 255, 210);
        const float cellWidth = imageSize.x / static_cast<float>(std::max(1, m_Cols));
        const float cellHeight = imageSize.y / static_cast<float>(std::max(1, m_Rows));

        for (int col = 0; col <= m_Cols; ++col)
        {
            const float x = imageMin.x + cellWidth * static_cast<float>(col);
            drawList->AddLine(ImVec2(x, imageMin.y), ImVec2(x, imageMax.y), gridColor);
        }
        for (int row = 0; row <= m_Rows; ++row)
        {
            const float y = imageMin.y + cellHeight * static_cast<float>(row);
            drawList->AddLine(ImVec2(imageMin.x, y), ImVec2(imageMax.x, y), gridColor);
        }

        for (int i = 0; i < m_FrameCount; ++i)
        {
            auto [col, row] = GetSequenceCell(i);
            if (!IsCellValid(col, row))
                continue;

            const ImVec2 min(imageMin.x + col * cellWidth, imageMin.y + row * cellHeight);
            const ImVec2 max(min.x + cellWidth, min.y + cellHeight);
            drawList->AddRect(min, max, sequenceColor, 0.0f, 0, 2.0f);
        }

        const ImVec2 selectedMin(imageMin.x + m_SelectedCol * cellWidth, imageMin.y + m_SelectedRow * cellHeight);
        const ImVec2 selectedMax(selectedMin.x + cellWidth, selectedMin.y + cellHeight);
        drawList->AddRect(selectedMin, selectedMax, selectedColor, 0.0f, 0, 3.0f);
        drawList->PopClipRect();

        const bool hovered = ImGui::IsItemHovered();
        if (hovered)
        {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const int hoverCol = std::clamp(static_cast<int>((mouse.x - imageMin.x) / cellWidth), 0, std::max(0, m_Cols - 1));
            const int hoverRow = std::clamp(static_cast<int>((mouse.y - imageMin.y) / cellHeight), 0, std::max(0, m_Rows - 1));

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                m_SelectedCol = hoverCol;
                m_SelectedRow = hoverRow;
            }

            const glm::vec2 uvMin = GetCellUVMin(hoverCol, hoverRow);
            const glm::vec2 uvMax = GetCellUVMax(hoverCol, hoverRow);
            ImGui::BeginTooltip();
            ImGui::Text(EditorLocale::Text("Cell %d, %d", "单元格 %d, %d"), hoverCol, hoverRow);
            ImGui::Text("UV %.3f %.3f -> %.3f %.3f", uvMin.x, uvMin.y, uvMax.x, uvMax.y);
            ImGui::EndTooltip();
        }

        ImGui::EndChild();
    }

    void SpriteSheetPickerPanel::DrawSelectedSpritePreview()
    {
        ImGui::TextUnformatted("Selected Sprite");

        if (!m_Texture)
        {
            ImGui::TextDisabled("(drop texture first)");
            return;
        }

        const glm::vec2 uvMin = GetCellUVMin(m_SelectedCol, m_SelectedRow);
        const glm::vec2 uvMax = GetCellUVMax(m_SelectedCol, m_SelectedRow);
        const ImVec2 previewSize(96.0f, 96.0f);
        ImGui::Image(ToImGuiTextureID(m_Texture), previewSize,
            ImVec2(uvMin.x, uvMax.y),
            ImVec2(uvMax.x, uvMin.y));
    }

    Ref<AnimationClip> SpriteSheetPickerPanel::GetOrCreateTargetClip()
    {
        if (!m_Entity || !m_Entity.HasComponent<SpriteAnimatorComponent>())
            return nullptr;

        auto& animator = m_Entity.GetComponent<SpriteAnimatorComponent>();
        std::string clipName = animator.CurrentClipName;
        if (clipName.empty())
            clipName = animator.DefaultClipName;
        if (clipName.empty() && !animator.Clips.empty())
            clipName = animator.Clips.begin()->first;
        if (clipName.empty())
            clipName = "atlas_clip";

        auto it = animator.Clips.find(clipName);
        Ref<AnimationClip> clip;
        if (it != animator.Clips.end())
        {
            clip = it->second;
        }
        else
        {
            clip = AnimationClip::Create(clipName, true);
            animator.AddClip(clip);
        }

        if (animator.DefaultClipName.empty())
            animator.DefaultClipName = clipName;
        if (animator.CurrentClipName.empty())
            animator.Play(clipName);

        return clip;
    }

    void SpriteSheetPickerPanel::DrawApplyActions()
    {
        const bool hasTexture = static_cast<bool>(m_Texture);
        const bool canSprite = hasTexture && m_Entity && m_Entity.HasComponent<SpriteRendererComponent>();
        const bool canUIImage = hasTexture && m_Entity && m_Entity.HasComponent<UIImageComponent>();
        const bool canAnimator = hasTexture && m_Entity && m_Entity.HasComponent<SpriteAnimatorComponent>();

        ImGui::TextUnformatted(EditorLocale::Text("Apply", "应用"));
        const ImVec2 wideButton(-1.0f, 0.0f);

        if (ButtonDisabled(EditorLocale::Text("Apply To SpriteRenderer", "应用到精灵渲染器"), !canSprite, wideButton))
        {
            auto& sprite = m_Entity.GetComponent<SpriteRendererComponent>();
            sprite.Texture = m_Texture;
            sprite.UVMin = GetCellUVMin(m_SelectedCol, m_SelectedRow);
            sprite.UVMax = GetCellUVMax(m_SelectedCol, m_SelectedRow);
            m_LastAction = "Applied selected cell to SpriteRenderer.";
        }
        HelpMarker("Writes texture + UV to the selected SpriteRenderer component.");

        if (ButtonDisabled(EditorLocale::Text("Apply To UI Image", "应用到 UI 图片"), !canUIImage, wideButton))
        {
            auto& image = m_Entity.GetComponent<UIImageComponent>();
            image.Texture = m_Texture;
            image.UVMin = GetCellUVMin(m_SelectedCol, m_SelectedRow);
            image.UVMax = GetCellUVMax(m_SelectedCol, m_SelectedRow);
            m_LastAction = "Applied selected cell to UI Image.";
        }
        HelpMarker("Writes texture + UV to the selected UIImage component. Good for icon atlases.");

        if (ButtonDisabled(m_AppendFrames ? "Append Sequence To Clip" : "Replace Clip With Sequence", !canAnimator, wideButton))
        {
            Ref<AnimationClip> clip = GetOrCreateTargetClip();
            if (clip)
            {
                const bool willClear = !m_AppendFrames && clip->GetFrameCount() > 0;
                if (willClear)
                {
                    // Replacing wipes the clip's existing frames — ask first.
                    m_ReplaceConfirmClipName = clip->GetName();
                    m_ReplaceConfirmFrameCount = clip->GetFrameCount();
                    m_ShowReplaceConfirm = true;
                }
                else
                {
                    ApplySequenceToClip(clip);
                }
            }
        }
        HelpMarker("Creates frame entries from the selected cell and sequence settings. The animation uses one texture and per-frame UVs.");

        if (ButtonDisabled(EditorLocale::Text("Sync Texture From Target", "从目标同步纹理"), !m_Entity, wideButton))
        {
            SyncTextureFromEntity();
            m_LastAction = "Synced texture from selected entity.";
        }

        ImGui::Separator();
        ImGui::TextDisabled("Sequence mode: %s", SequenceModeName(m_SequenceMode));
    }

} // namespace Wheatear
