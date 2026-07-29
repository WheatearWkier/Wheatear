#include "wtpch.h"
#include "AnimationEditorPanel.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <cstring>

#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Scene/Components.h"
#include "ContentBrowserPanel.h"   // GetEditorAssetPath

namespace Wheatear {

    // 轨道行高
    static constexpr float kTrackHeight = 36.0f;
    // 左侧标签列宽
    static constexpr float kLabelWidth = 150.0f;
    // 时间轴标尺高度
    static constexpr float kRulerHeight = 36.0f;

    // ════════════════════════════════════════════════════════
    //  插值模式下拉
    // ════════════════════════════════════════════════════════

    static void DrawInterpModeCombo(InterpolationMode& mode)
    {
        static const InterpolationMode kModes[] = {
            InterpolationMode::Linear, InterpolationMode::Step,
            InterpolationMode::EaseIn, InterpolationMode::EaseOut,
            InterpolationMode::EaseInOut,
        };
        ImGui::SetNextItemWidth(80.0f);
        if (ImGui::BeginCombo("##interp", InterpolationModeName(mode)))
        {
            for (auto m : kModes)
            {
                bool sel = (mode == m);
                if (ImGui::Selectable(InterpolationModeName(m), sel)) mode = m;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    // ════════════════════════════════════════════════════════
    //  SetEntity：从 Hierarchy 选中实体时调用
    // ════════════════════════════════════════════════════════

    void AnimationEditorPanel::SetEntity(Entity entity)
    {
        if (m_Entity == entity) return;

        StopPreview();

        m_Entity = entity;
        m_Animator = nullptr;
        m_PlaybackTime = 0.0f;
        m_IsPlaying = false;
        m_CurrentClipName = "";

        if (!entity) return;
        if (!entity.HasComponent<SpriteAnimatorComponent>()) return;

        m_Animator = &entity.GetComponent<SpriteAnimatorComponent>();
        m_CurrentClipName = m_Animator->CurrentClipName;

        if (m_Scene)
            m_Scene->SetAnimationEditorPreviewActive(true);
    }

    // ════════════════════════════════════════════════════════
    //  工具函数：取当前正在编辑的 Clip
    // ════════════════════════════════════════════════════════

    Ref<AnimationClip> AnimationEditorPanel::GetCurrentClip() const
    {
        if (!m_Animator) return nullptr;
        auto it = m_Animator->Clips.find(m_CurrentClipName);
        return it != m_Animator->Clips.end() ? it->second : nullptr;
    }

    // ════════════════════════════════════════════════════════
    //  采样工具函数（避免 time 变量被跨轨道污染）
    // ════════════════════════════════════════════════════════

    // BUG FIX #3: 将采样逻辑提取为独立函数，每条轨道使用独立的 localTime，
    // 防止原代码中 float 轨道的 looping fmod 修改共享的 time 变量，
    // 从而错误影响后续 vec4 轨道的采样。
    static float SampleFloat(
        const std::vector<Keyframe<float>>& kfs,
        float playbackTime,
        float duration,
        bool looping)
    {
        if (kfs.empty()) return 0.0f;

        float t = playbackTime;
        if (looping && duration > 0.0f)
            t = fmod(playbackTime, duration);

        if (kfs.size() == 1)   return kfs[0].Value;
        if (t <= kfs.front().Time) return kfs.front().Value;
        if (t >= kfs.back().Time)  return kfs.back().Value;

        int nextIdx = 1;
        while (nextIdx < (int)kfs.size() && kfs[nextIdx].Time < t)
            nextIdx++;

        const auto& prev = kfs[nextIdx - 1];
        const auto& next = kfs[nextIdx];
        float span = next.Time - prev.Time;
        float alpha = (span > 0.0f) ? (t - prev.Time) / span : 0.0f;

        switch (prev.Mode)
        {
        case InterpolationMode::Step:      alpha = 0.0f; break;
        case InterpolationMode::EaseIn:    alpha = alpha * alpha; break;
        case InterpolationMode::EaseOut:   alpha = alpha * (2.0f - alpha); break;
        case InterpolationMode::EaseInOut: alpha = alpha < 0.5f
            ? 2.0f * alpha * alpha : -1.0f + (4.0f - 2.0f * alpha) * alpha; break;
        default: break;
        }
        return prev.Value + (next.Value - prev.Value) * alpha;
    }

    static glm::vec4 SampleVec4(
        const std::vector<Keyframe<glm::vec4>>& kfs,
        float playbackTime,
        float duration,
        bool looping)
    {
        if (kfs.empty()) return glm::vec4(1.0f);

        float t = playbackTime;
        if (looping && duration > 0.0f)
            t = fmod(playbackTime, duration);

        if (kfs.size() == 1)   return kfs[0].Value;
        if (t <= kfs.front().Time) return kfs.front().Value;
        if (t >= kfs.back().Time)  return kfs.back().Value;

        int nextIdx = 1;
        while (nextIdx < (int)kfs.size() && kfs[nextIdx].Time < t)
            nextIdx++;

        const auto& prev = kfs[nextIdx - 1];
        const auto& next = kfs[nextIdx];
        float span = next.Time - prev.Time;
        float alpha = (span > 0.0f) ? (t - prev.Time) / span : 0.0f;

        switch (prev.Mode)
        {
        case InterpolationMode::Step:      alpha = 0.0f; break;
        case InterpolationMode::EaseIn:    alpha = alpha * alpha; break;
        case InterpolationMode::EaseOut:   alpha = alpha * (2.0f - alpha); break;
        case InterpolationMode::EaseInOut: alpha = alpha < 0.5f
            ? 2.0f * alpha * alpha : -1.0f + (4.0f - 2.0f * alpha) * alpha; break;
        default: break;
        }
        return prev.Value + (next.Value - prev.Value) * alpha;
    }

    // ════════════════════════════════════════════════════════
    //  SyncPreviewToEntity
    //  BUG FIX #1: 只在预览激活（m_IsPlaying 或用户主动拖动时间轴）时写回组件，
    //  且快照必须在第一次写回之前已经拍好，否则本函数不执行。
    //  这样可以保证：编辑器不在预览时绝不污染实体的序列化状态。
    // ════════════════════════════════════════════════════════

    void AnimationEditorPanel::SyncPreviewToEntity()
    {
        if (!m_Entity || !m_Animator) return;

        // BUG FIX #1 核心：没有快照就不写回，防止在非预览状态下污染组件数据。
        // 快照只在 Play 或主动 Scrub 时才拍，确保 Sync 不会在用户没有
        // 明确触发预览的情况下修改实体状态。
        if (!m_HasSnapshot) return;

        auto clip = GetCurrentClip();
        if (!clip) return;

        float duration = clip->GetTotalDuration();
        bool  looping = clip->IsLooping();

        // ── 序列帧写回 SpriteRenderer ────────────────────────────
        if (clip->GetFrameCount() > 0 &&
            m_Entity.HasComponent<SpriteRendererComponent>())
        {
            const auto& frames = clip->GetFrames();
            float elapsed = 0.0f;
            int frameIndex = (int)frames.size() - 1;

            float playT = m_PlaybackTime;
            if (looping && duration > 0.0f)
                playT = fmod(playT, duration);

            for (int i = 0; i < (int)frames.size(); i++)
            {
                if (playT >= elapsed && playT < elapsed + frames[i].Duration)
                {
                    frameIndex = i;
                    break;
                }
                elapsed += frames[i].Duration;
            }

            const auto& frame = frames[frameIndex];
            auto& sr = m_Entity.GetComponent<SpriteRendererComponent>();
            if (frame.Texture)
            {
                sr.Texture = frame.Texture;
                // BUG FIX #2: 同时写回 UV，防止 Atlas 帧只换纹理不换 UV 导致显示错误
                sr.UVMin = frame.TexCoordMin;
                sr.UVMax = frame.TexCoordMax;
            }
        }

        // ── 属性轨道写回 ─────────────────────────────────────────
        for (auto& trackBase : clip->GetPropertyTracks())
        {
            if (trackBase->GetDataType() == TrackDataType::Float)
            {
                auto track = std::static_pointer_cast<PropertyTrack<float>>(trackBase);
                // BUG FIX #3: 每条轨道用独立函数采样，不共享、不污染 time 变量
                float sampledValue = SampleFloat(
                    track->Keyframes, m_PlaybackTime, duration, looping);

                auto prop = trackBase->Property;

                if (m_Entity.HasComponent<SpriteRendererComponent>())
                {
                    auto& sr = m_Entity.GetComponent<SpriteRendererComponent>();
                    switch (prop)
                    {
                    case AnimatedProperty::SpriteColorR: sr.Color.r = sampledValue; break;
                    case AnimatedProperty::SpriteColorG: sr.Color.g = sampledValue; break;
                    case AnimatedProperty::SpriteColorB: sr.Color.b = sampledValue; break;
                    case AnimatedProperty::SpriteColorA: sr.Color.a = sampledValue; break;
                    default: break;
                    }
                }

                if (m_Entity.HasComponent<TransformComponent>())
                {
                    auto& tc = m_Entity.GetComponent<TransformComponent>();
                    switch (prop)
                    {
                    case AnimatedProperty::PositionX:    tc.Translation.x = sampledValue; break;
                    case AnimatedProperty::PositionY:    tc.Translation.y = sampledValue; break;
                    case AnimatedProperty::PositionZ:    tc.Translation.z = sampledValue; break;
                    case AnimatedProperty::RotationZ:    tc.Rotation.z = glm::radians(sampledValue); break;
                    case AnimatedProperty::ScaleX:       tc.Scale.x = sampledValue; break;
                    case AnimatedProperty::ScaleY:       tc.Scale.y = sampledValue; break;
                    case AnimatedProperty::ScaleUniform: tc.Scale.x = tc.Scale.y = sampledValue; break;
                    default: break;
                    }
                }
            }
            else if (trackBase->GetDataType() == TrackDataType::Vec4)
            {
                auto track = std::static_pointer_cast<PropertyTrack<glm::vec4>>(trackBase);
                glm::vec4 sampledValue = SampleVec4(
                    track->Keyframes, m_PlaybackTime, duration, looping);

                if (trackBase->Property == AnimatedProperty::SpriteColor &&
                    m_Entity.HasComponent<SpriteRendererComponent>())
                {
                    m_Entity.GetComponent<SpriteRendererComponent>().Color = sampledValue;
                }
            }
        }
    }

    // ════════════════════════════════════════════════════════
    //  TakeSnapshot / RestoreSnapshot
    // ════════════════════════════════════════════════════════

    void AnimationEditorPanel::TakeSnapshot()
    {
        if (!m_Entity) return;
        if (m_HasSnapshot) return;

        if (m_Entity.HasComponent<SpriteRendererComponent>())
        {
            auto& sr = m_Entity.GetComponent<SpriteRendererComponent>();
            m_SnapshotColor = sr.Color;
            // BUG FIX #2: 快照也需要保存 Texture、TexCoord，
            // 否则 Stop 后无法恢复序列帧播放前的纹理状态，
            // 第二次 Play 时 TakeSnapshot 会拍到上次播放残留的最后一帧纹理。
            m_SnapshotTexture = sr.Texture;
            m_SnapshotTexCoordMin = sr.UVMin;
            m_SnapshotTexCoordMax = sr.UVMax;
        }

        if (m_Entity.HasComponent<TransformComponent>())
        {
            auto& tc = m_Entity.GetComponent<TransformComponent>();
            m_SnapshotTranslation = tc.Translation;
            m_SnapshotRotation = tc.Rotation;
            m_SnapshotScale = tc.Scale;
        }

        m_HasSnapshot = true;
    }

    void AnimationEditorPanel::RestoreSnapshot()
    {
        if (!m_Entity || !m_HasSnapshot) return;

        if (m_Entity.HasComponent<SpriteRendererComponent>())
        {
            auto& sr = m_Entity.GetComponent<SpriteRendererComponent>();
            sr.Color = m_SnapshotColor;
            // BUG FIX #2: 恢复纹理和 UV，确保预览结束后实体回到原始外观
            sr.Texture = m_SnapshotTexture;
            sr.UVMin = m_SnapshotTexCoordMin;
            sr.UVMax = m_SnapshotTexCoordMax;
        }

        if (m_Entity.HasComponent<TransformComponent>())
        {
            auto& tc = m_Entity.GetComponent<TransformComponent>();
            tc.Translation = m_SnapshotTranslation;
            tc.Rotation = m_SnapshotRotation;
            tc.Scale = m_SnapshotScale;
        }

        m_HasSnapshot = false;
    }

    void AnimationEditorPanel::StopPreview()
    {
        m_IsPlaying = false;
        m_PlaybackTime = 0.0f;
        RestoreSnapshot();

        if (m_Scene)
            m_Scene->SetAnimationEditorPreviewActive(false);
    }

    // ════════════════════════════════════════════════════════
    //  Scrub（拖动时间轴）时主动触发快照 + Sync
    //  BUG FIX #1: 提供一个专用函数，让时间轴点击/拖动也走快照保护流程
    // ════════════════════════════════════════════════════════

    void AnimationEditorPanel::BeginScrub()
    {
        // 开始拖动时间轴时拍快照（如果还没有的话）
        TakeSnapshot();
        m_IsScrubbing = true;
        // 重新激活预览模式，防止 Scene::OnUpdateEditor 覆盖 UV
        if (m_Scene)
            m_Scene->SetAnimationEditorPreviewActive(true);
    }

    void AnimationEditorPanel::EndScrub()
    {
        // 松开鼠标后停留在当前时间，不自动恢复快照
        // （用户可能想查看某一帧的状态）
        // 快照将在下次 StopPreview / 切换 Clip / 切换 Entity 时恢复
        m_IsScrubbing = false;
    }

    // ════════════════════════════════════════════════════════
    //  主入口
    // ════════════════════════════════════════════════════════

    void AnimationEditorPanel::OnImGuiRender(Timestep ts)
    {
        ImGui::Begin("Animation Editor");

        if (!m_Entity || !m_Animator)
        {
            ImGui::TextDisabled("Select an entity with a Sprite Animator component.");
            ImGui::End();
            return;
        }

        // 预览播放推进时间
        if (m_IsPlaying)
        {
            auto clip = GetCurrentClip();
            if (clip)
            {
                float duration = clip->GetTotalDuration();
                if (duration > 0.0f)
                {
                    m_PlaybackTime += ts;
                    if (clip->IsLooping())
                        m_PlaybackTime = fmod(m_PlaybackTime, duration);
                    else
                        m_PlaybackTime = std::min(m_PlaybackTime, duration);
                }
            }
        }

        // BUG FIX #1: SyncPreviewToEntity 内部已做 m_HasSnapshot 守卫，
        // 只有在快照存在（即用户已触发预览/Scrub）时才写回组件。
        SyncPreviewToEntity();

        DrawToolbar();
        ImGui::Separator();
        DrawTimeline();
        ImGui::Separator();
        DrawAtlasSection();
        ImGui::Separator();
        DrawFramesSection();

        ImGui::End();
    }

    // ════════════════════════════════════════════════════════
    //  工具栏
    // ════════════════════════════════════════════════════════

    void AnimationEditorPanel::DrawToolbar()
    {
        if (!m_Animator) return;

        // ── 第一行：实体名 + Clip 管理 ────────────────────────
        ImGui::TextDisabled("Entity:");
        ImGui::SameLine();
        ImGui::Text("%s", m_Entity.GetName().c_str());

        ImGui::SameLine(0, 20);

        bool playOnStart = m_Animator->PlayOnStart;
        if (ImGui::Checkbox("Play On Start", &playOnStart))
            m_Animator->PlayOnStart = playOnStart;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Auto-play default clip when scene starts");

        ImGui::SameLine(0, 20);

        if (ImGui::Button("+ Add Clip"))
        {
            std::string newName = "NewClip";
            int suffix = 0;
            while (m_Animator->Clips.count(
                newName + (suffix ? std::to_string(suffix) : "")))
                suffix++;
            if (suffix) newName += std::to_string(suffix);
            m_Animator->AddClip(AnimationClip::Create(newName, true));
            m_CurrentClipName = newName;
        }

        ImGui::SameLine();

        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::BeginCombo("##clipselect",
            m_CurrentClipName.empty() ? "(none)" : m_CurrentClipName.c_str()))
        {
            for (auto& [name, clip] : m_Animator->Clips)
            {
                bool sel = (name == m_CurrentClipName);
                if (ImGui::Selectable(name.c_str(), sel))
                {
                    // 切换 Clip 时恢复快照，防止跨 Clip 污染
                    StopPreview();
                    m_CurrentClipName = name;
                    //m_Animator->CurrentClipName = name;
                    m_PlaybackTime = 0.0f;
                    m_IsPlaying = false;
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Clip 重命名
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        static char s_RenameBuffer[64] = {};
        static std::string s_LastClipName = "";
        if (s_LastClipName != m_CurrentClipName)
        {
            strncpy_s(s_RenameBuffer, m_CurrentClipName.c_str(), sizeof(s_RenameBuffer) - 1);
            s_LastClipName = m_CurrentClipName;
        }

        ImGui::PushID("##cliprename");
        if (ImGui::InputText("##rename", s_RenameBuffer, sizeof(s_RenameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue))
        {
            std::string newName = s_RenameBuffer;
            if (!newName.empty() && !m_Animator->Clips.count(newName)
                && newName != m_CurrentClipName)
            {
                auto clipNode = m_Animator->Clips.extract(m_CurrentClipName);
                clipNode.key() = newName;
                m_Animator->Clips.insert(std::move(clipNode));

                auto atlasNode = m_AtlasConfigs.extract(m_CurrentClipName);
                if (!atlasNode.empty())
                {
                    atlasNode.key() = newName;
                    m_AtlasConfigs.insert(std::move(atlasNode));
                }

                if (m_Animator->CurrentClipName == m_CurrentClipName)
                    m_Animator->CurrentClipName = newName;

                m_CurrentClipName = newName;
                s_LastClipName = newName;
                //m_Animator->CurrentClipName = newName;

                auto it = m_Animator->Clips.find(newName);
                if (it != m_Animator->Clips.end())
                    it->second->SetName(newName);
            }
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Press Enter to rename clip");

        if (!m_CurrentClipName.empty())
        {
            ImGui::SameLine();
            if (ImGui::Button("Delete Clip"))
            {
                StopPreview(); // BUG FIX #1: 删除 Clip 前先恢复快照
                m_AtlasConfigs.erase(m_CurrentClipName);
                m_Animator->Clips.erase(m_CurrentClipName);
                if (m_Animator->CurrentClipName == m_CurrentClipName)
                    m_Animator->CurrentClipName = "";
                m_CurrentClipName = m_Animator->Clips.empty()
                    ? "" : m_Animator->Clips.begin()->first;
                m_PlaybackTime = 0.0f;
                m_IsPlaying = false;
                return;
            }
        }

        auto clip = GetCurrentClip();
        if (!clip) return;

        // ── 第二行：播放控制 ──────────────────────────────────
        ImGui::Spacing();

        if (m_IsPlaying)
        {
            if (ImGui::Button("|| Pause"))  m_IsPlaying = false;
        }
        else
        {
            if (ImGui::Button(">  Play"))
            {
                TakeSnapshot(); // 没快照才拍，已有快照不覆盖

                m_IsPlaying = true;
                if (m_PlaybackTime >= clip->GetTotalDuration())
                    m_PlaybackTime = 0.0f;
                // 重新激活预览模式，防止 Scene::OnUpdateEditor 覆盖 UV
                if (m_Scene)
                    m_Scene->SetAnimationEditorPreviewActive(true);
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("[] Stop"))
        {
            StopPreview();
        }

        ImGui::SameLine(0, 20);
        bool looping = clip->IsLooping();
        if (ImGui::Checkbox("Loop", &looping))
            clip->SetLooping(looping);

        ImGui::SameLine(0, 20);
        if (ImGui::Button("Set Default"))
            m_Animator->DefaultClipName = m_CurrentClipName;

        if (!m_Animator->DefaultClipName.empty())
        {
            ImGui::SameLine();
            if (ImGui::Button("Clear Default"))
                m_Animator->DefaultClipName = "";
        }

        if (!m_Animator->DefaultClipName.empty() && m_Animator->DefaultClipName == m_CurrentClipName)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "(default)");
        }

        ImGui::SameLine(0, 20);
        ImGui::TextDisabled("Duration: %.2fs", clip->GetTotalDuration());

        ImGui::SameLine(0, 20);
        ImGui::TextDisabled("Zoom:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::SliderFloat("##zoom", &m_PixelsPerSecond, 40.0f, 400.0f, "%.0fpx/s");

        // ── 第三行：Add Track ─────────────────────────────────
        ImGui::Spacing();

        static const AnimatedProperty kAllProperties[] = {
            AnimatedProperty::SpriteColorA,
            AnimatedProperty::SpriteColor,
            AnimatedProperty::SpriteColorR,
            AnimatedProperty::SpriteColorG,
            AnimatedProperty::SpriteColorB,
            AnimatedProperty::PositionX,
            AnimatedProperty::PositionY,
            AnimatedProperty::PositionZ,
            AnimatedProperty::RotationZ,
            AnimatedProperty::ScaleX,
            AnimatedProperty::ScaleY,
            AnimatedProperty::ScaleUniform,
        };
        static int s_SelectedPropIdx = 0;

        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::BeginCombo("##propselect",
            AnimatedPropertyName(kAllProperties[s_SelectedPropIdx])))
        {
            for (int i = 0; i < IM_ARRAYSIZE(kAllProperties); i++)
            {
                bool sel = (s_SelectedPropIdx == i);
                if (ImGui::Selectable(AnimatedPropertyName(kAllProperties[i]), sel))
                    s_SelectedPropIdx = i;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Add Track"))
        {
            AnimatedProperty prop = kAllProperties[s_SelectedPropIdx];
            if (prop == AnimatedProperty::SpriteColor)
                clip->AddVec4Track(prop);
            else
                clip->AddFloatTrack(prop);
        }
    }

    // ════════════════════════════════════════════════════════
    //  时间轴主体
    // ════════════════════════════════════════════════════════

    void AnimationEditorPanel::DrawTimeline()
    {
        auto clip = GetCurrentClip();
        if (!clip)
        {
            ImGui::TextDisabled("No clip selected.");
            return;
        }

        const float duration = std::max(clip->GetTotalDuration(), 1.0f);
        const float totalWidth = duration * m_PixelsPerSecond + 200.0f;
        const int   trackCount = 1 + (int)clip->GetPropertyTracks().size();
        const float totalHeight = kRulerHeight + trackCount * kTrackHeight + 4.0f;

        // ── 左侧标签列 ──────────────────────────────────────
        ImGui::BeginChild("##labels",
            ImVec2(kLabelWidth, totalHeight + 20.0f), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            ImGui::Dummy(ImVec2(kLabelWidth, kRulerHeight));

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.85f, 0.5f, 1.0f));
            ImGui::Text("  Frames (%d)", clip->GetFrameCount());
            ImGui::PopStyleColor();

            auto& tracks = clip->GetPropertyTracks();
            for (int ti = 0; ti < (int)tracks.size(); ti++)
            {
                auto& trackBase = tracks[ti];
                ImGui::PushID(ti);

                if (ImGui::SmallButton("x"))
                {
                    clip->RemovePropertyTrack(ti);
                    ImGui::PopID();
                    break;
                }
                ImGui::SameLine();
                ImGui::Text("%s", AnimatedPropertyName(trackBase->Property));
                ImGui::PopID();
            }
        }
        ImGui::EndChild();

        ImGui::SameLine(0, 0);

        // ── 右侧时间轴（可横向滚动）──────────────────────────
        ImGui::BeginChild("##timeline_scroll",
            ImVec2(0, totalHeight + 20.0f), false,
            ImGuiWindowFlags_HorizontalScrollbar);
        {
            ImGui::Dummy(ImVec2(totalWidth, totalHeight));

            ImDrawList* dl = ImGui::GetWindowDrawList();

            ImVec2 origin;
            origin.x = ImGui::GetWindowPos().x - ImGui::GetScrollX();
            origin.y = ImGui::GetWindowPos().y;

            float trackY = origin.y;

            // ── 标尺背景 ────────────────────────────────────
            dl->AddRectFilled(
                ImVec2(origin.x, trackY),
                ImVec2(origin.x + totalWidth, trackY + kRulerHeight),
                IM_COL32(45, 45, 45, 255));

            float step = 0.1f;
            if (m_PixelsPerSecond < 60.0f) step = 0.5f;
            if (m_PixelsPerSecond < 25.0f) step = 1.0f;

            for (float t = 0.0f; t <= duration + step * 0.5f; t += step)
            {
                float x = origin.x + t * m_PixelsPerSecond;
                bool  isMajor = (fmod(t + 0.001f, 0.5f) < step * 0.5f);
                dl->AddLine(
                    ImVec2(x, trackY + (isMajor ? 4.0f : 10.0f)),
                    ImVec2(x, trackY + kRulerHeight),
                    IM_COL32(180, 180, 180, 200));
                if (isMajor)
                {
                    char buf[16]; snprintf(buf, sizeof(buf), "%.1fs", t);
                    dl->AddText(ImVec2(x + 2.0f, trackY + 3.0f),
                        IM_COL32(200, 200, 200, 255), buf);
                }
            }

            // 标尺点击/拖动跳转
            ImGui::SetCursorScreenPos(ImVec2(ImGui::GetWindowPos().x, origin.y));
            ImGui::InvisibleButton("##ruler_hit",
                ImVec2(ImGui::GetWindowWidth(), kRulerHeight));

            // BUG FIX #1: 拖动标尺时也需要走快照流程，
            // 否则用户 Scrub 时会直接污染组件数据
            if (ImGui::IsItemActivated())
                BeginScrub();

            if (ImGui::IsItemActive() && ImGui::IsMouseDown(0))
            {
                float clickX = ImGui::GetIO().MousePos.x
                    - ImGui::GetWindowPos().x
                    + ImGui::GetScrollX();
                m_PlaybackTime = std::clamp(clickX / m_PixelsPerSecond, 0.0f, duration);
                m_IsPlaying = false;
            }

            if (ImGui::IsItemDeactivated())
                EndScrub();

            // 滚轮缩放
            if (ImGui::IsWindowHovered())
            {
                float wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0.0f && ImGui::GetIO().KeyCtrl)
                    m_PixelsPerSecond = std::clamp(
                        m_PixelsPerSecond + wheel * 15.0f, 40.0f, 400.0f);
            }

            trackY += kRulerHeight;

            // ── Frames 轨道 ──────────────────────────────────
            DrawFrameTrack(ImVec2(origin.x, trackY), kTrackHeight);
            trackY += kTrackHeight;

            // ── 属性轨道 ────────────────────────────────────
            auto& tracks = clip->GetPropertyTracks();
            for (int ti = 0; ti < (int)tracks.size(); ti++)
            {
                DrawPropertyTrack(ImVec2(origin.x, trackY), kTrackHeight, ti);
                trackY += kTrackHeight;
            }

            // ── 时间指针红线 ──────────────────────────────────
            float cursorX = origin.x + m_PlaybackTime * m_PixelsPerSecond;
            dl->AddLine(
                ImVec2(cursorX, origin.y),
                ImVec2(cursorX, trackY),
                IM_COL32(255, 80, 80, 220), 2.0f);
            dl->AddTriangleFilled(
                ImVec2(cursorX - 5.0f, origin.y),
                ImVec2(cursorX + 5.0f, origin.y),
                ImVec2(cursorX, origin.y + 10.0f),
                IM_COL32(255, 80, 80, 255));
        }
        ImGui::EndChild();
    }

    // ════════════════════════════════════════════════════════
    //  帧序列轨道
    // ════════════════════════════════════════════════════════

    void AnimationEditorPanel::DrawFrameTrack(ImVec2 origin, float trackHeight)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        auto clip = GetCurrentClip();
        if (!clip) return;

        dl->AddRectFilled(
            origin,
            ImVec2(origin.x + clip->GetTotalDuration() * m_PixelsPerSecond + 60.0f,
                origin.y + trackHeight),
            IM_COL32(35, 35, 35, 255));

        auto& frames = clip->GetFrames();
        float x = origin.x;

        // 预先算出当前帧索引，避免在内层循环重复计算
        float elapsed = 0.0f;
        int currentFrameIndex = (int)frames.size() - 1;
        {
            auto clip2 = GetCurrentClip();
            float playT = m_PlaybackTime;
            if (clip2 && clip2->IsLooping() && clip2->GetTotalDuration() > 0.0f)
                playT = fmod(playT, clip2->GetTotalDuration());

            for (int j = 0; j < (int)frames.size(); j++)
            {
                if (playT >= elapsed && playT < elapsed + frames[j].Duration)
                {
                    currentFrameIndex = j;
                    break;
                }
                elapsed += frames[j].Duration;
            }
        }

        for (int i = 0; i < (int)frames.size(); i++)
        {
            const auto& frame = frames[i];
            float w = frame.Duration * m_PixelsPerSecond;
            float h = trackHeight - 2.0f;

            ImVec2 cellMin = ImVec2(x + 1.0f, origin.y + 1.0f);
            ImVec2 cellMax = ImVec2(x + w - 1.0f, origin.y + h);

            bool isCurrent = (i == currentFrameIndex);

            ImU32 bgColor = isCurrent
                ? IM_COL32(80, 160, 80, 200)
                : IM_COL32(60, 90, 60, 180);

            if (frame.Texture && w > 4.0f)
            {
                dl->AddImageRounded(
                    static_cast<ImTextureID>(static_cast<uintptr_t>(frame.Texture->GetRendererID())),
                    cellMin, cellMax,
                    // 注意垂直翻转显示图片
                    ImVec2(frame.TexCoordMin.x, frame.TexCoordMax.y),
                    ImVec2(frame.TexCoordMax.x, frame.TexCoordMin.y),
                    IM_COL32_WHITE, 2.0f);
            }
            else
            {
                dl->AddRectFilled(cellMin, cellMax, bgColor, 2.0f);
            }

            dl->AddRect(cellMin, cellMax,
                isCurrent ? IM_COL32(150, 255, 150, 255)
                : IM_COL32(100, 140, 100, 200),
                2.0f);

            if (w > 16.0f)
            {
                char buf[8]; snprintf(buf, sizeof(buf), "%d", i);
                dl->AddText(ImVec2(x + 3.0f, origin.y + 3.0f),
                    IM_COL32(230, 230, 230, 200), buf);
            }

            x += w;
        }
    }

    // ════════════════════════════════════════════════════════
    //  属性轨道
    // ════════════════════════════════════════════════════════

    void AnimationEditorPanel::DrawPropertyTrack(ImVec2 origin, float trackHeight, int trackIndex)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        auto clip = GetCurrentClip();
        if (!clip) return;

        auto& tracks = clip->GetPropertyTracks();
        if (trackIndex >= (int)tracks.size()) return;

        auto& trackBase = tracks[trackIndex];
        float totalDur = clip->GetTotalDuration();

        ImU32 bgColor = (trackIndex % 2 == 0)
            ? IM_COL32(38, 38, 38, 255)
            : IM_COL32(42, 42, 42, 255);
        dl->AddRectFilled(
            origin,
            ImVec2(origin.x + totalDur * m_PixelsPerSecond + 60.0f,
                origin.y + trackHeight),
            bgColor);

        float centerY = origin.y + trackHeight * 0.5f;

        if (trackBase->GetDataType() == TrackDataType::Float)
        {
            auto track = std::static_pointer_cast<PropertyTrack<float>>(trackBase);
            auto& kfs = track->Keyframes;

            for (int ki = 0; ki + 1 < (int)kfs.size(); ki++)
            {
                float x0 = origin.x + kfs[ki].Time * m_PixelsPerSecond;
                float x1 = origin.x + kfs[ki + 1].Time * m_PixelsPerSecond;
                dl->AddLine(ImVec2(x0, centerY), ImVec2(x1, centerY),
                    IM_COL32(150, 200, 255, 120), 1.5f);
            }

            // BUG FIX #4: 收集待删除的关键帧索引，在循环结束后再删除，
            // 防止在遍历时修改容器导致迭代器失效/越界崩溃。
            int kfToDelete = -1;

            for (int ki = 0; ki < (int)kfs.size(); ki++)
            {
                auto& kf = kfs[ki];
                float kx = origin.x + kf.Time * m_PixelsPerSecond;
                float ky = centerY;

                ImVec2 pts[4] = {
                    ImVec2(kx,        ky - 6.0f),
                    ImVec2(kx + 6.0f, ky),
                    ImVec2(kx,        ky + 6.0f),
                    ImVec2(kx - 6.0f, ky)
                };
                dl->AddConvexPolyFilled(pts, 4, IM_COL32(100, 180, 255, 220));
                dl->AddPolyline(pts, 4, IM_COL32(180, 220, 255, 255), true, 1.0f);

                ImGui::SetCursorScreenPos(ImVec2(kx - 6.0f, ky - 6.0f));
                ImGui::PushID(ki + trackIndex * 1000);
                ImGui::InvisibleButton("##kf", ImVec2(12.0f, 12.0f));

                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))
                {
                    kf.Time += ImGui::GetIO().MouseDelta.x / m_PixelsPerSecond;
                    kf.Time = std::max(0.0f, kf.Time);
                }

                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("t=%.3fs  v=%.3f\n%s\n[drag] move  [right-click] edit",
                        kf.Time, kf.Value, InterpolationModeName(kf.Mode));
                }

                if (ImGui::BeginPopupContextItem("##kf_ctx"))
                {
                    ImGui::Text("Keyframe %d", ki);
                    ImGui::Separator();
                    ImGui::SetNextItemWidth(80.0f);
                    ImGui::DragFloat("Time##kft", &kf.Time, 0.01f, 0.0f, 999.0f, "%.3fs");
                    ImGui::SetNextItemWidth(80.0f);
                    ImGui::DragFloat("Value##kfv", &kf.Value, 0.01f, -999.0f, 999.0f, "%.3f");
                    DrawInterpModeCombo(kf.Mode);
                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete"))
                        kfToDelete = ki; // BUG FIX #4: 延迟删除
                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }

            // BUG FIX #4: 循环结束后再执行删除
            if (kfToDelete >= 0)
                track->RemoveKeyframe(kfToDelete);

            // 双击空白处添加关键帧
            ImGui::SetCursorScreenPos(origin);
            ImGui::InvisibleButton(
                ("##track_bg" + std::to_string(trackIndex)).c_str(),
                ImVec2(totalDur * m_PixelsPerSecond + 60.0f, trackHeight));
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            {
                float clickTime = (ImGui::GetIO().MousePos.x - origin.x) / m_PixelsPerSecond;
                clickTime = std::max(0.0f, clickTime);
                track->AddKeyframe(clickTime, 0.0f);
            }
        }
        else
        {
            auto track = std::static_pointer_cast<PropertyTrack<glm::vec4>>(trackBase);
            auto& kfs = track->Keyframes;

            for (int ki = 0; ki + 1 < (int)kfs.size(); ki++)
            {
                float x0 = origin.x + kfs[ki].Time * m_PixelsPerSecond;
                float x1 = origin.x + kfs[ki + 1].Time * m_PixelsPerSecond;
                dl->AddLine(ImVec2(x0, centerY), ImVec2(x1, centerY),
                    IM_COL32(255, 200, 100, 120), 1.5f);
            }

            // BUG FIX #4: 同样延迟删除
            int kfToDelete = -1;

            for (int ki = 0; ki < (int)kfs.size(); ki++)
            {
                auto& kf = kfs[ki];
                float kx = origin.x + kf.Time * m_PixelsPerSecond;

                ImU32 kfColor = IM_COL32(
                    (int)(kf.Value.r * 255),
                    (int)(kf.Value.g * 255),
                    (int)(kf.Value.b * 255),
                    220);

                ImVec2 pts[4] = {
                    ImVec2(kx,        centerY - 6.0f),
                    ImVec2(kx + 6.0f, centerY),
                    ImVec2(kx,        centerY + 6.0f),
                    ImVec2(kx - 6.0f, centerY)
                };
                dl->AddConvexPolyFilled(pts, 4, kfColor);
                dl->AddPolyline(pts, 4, IM_COL32_WHITE, true, 1.0f);

                ImGui::SetCursorScreenPos(ImVec2(kx - 6.0f, centerY - 6.0f));
                ImGui::PushID(ki + trackIndex * 1000 + 10000);
                ImGui::InvisibleButton("##kf4", ImVec2(12.0f, 12.0f));

                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))
                {
                    kf.Time += ImGui::GetIO().MouseDelta.x / m_PixelsPerSecond;
                    kf.Time = std::max(0.0f, kf.Time);
                }

                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("t=%.3fs  rgba=(%.2f,%.2f,%.2f,%.2f)",
                        kf.Time,
                        kf.Value.r, kf.Value.g, kf.Value.b, kf.Value.a);

                if (ImGui::BeginPopupContextItem("##kf4_ctx"))
                {
                    ImGui::Text("Keyframe %d", ki);
                    ImGui::Separator();
                    ImGui::SetNextItemWidth(80.0f);
                    ImGui::DragFloat("Time##kft4", &kf.Time, 0.01f, 0.0f, 999.0f, "%.3fs");
                    ImGui::SetNextItemWidth(200.0f);
                    ImGui::ColorEdit4("Color##kfc", glm::value_ptr(kf.Value));
                    DrawInterpModeCombo(kf.Mode);
                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete"))
                        kfToDelete = ki; // BUG FIX #4: 延迟删除
                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }

            // BUG FIX #4: 循环结束后再执行删除
            if (kfToDelete >= 0)
                track->RemoveKeyframe(kfToDelete);

            // 双击添加
            ImGui::SetCursorScreenPos(origin);
            ImGui::InvisibleButton(
                ("##track_bg4_" + std::to_string(trackIndex)).c_str(),
                ImVec2(totalDur * m_PixelsPerSecond + 60.0f, trackHeight));
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            {
                float clickTime = (ImGui::GetIO().MousePos.x - origin.x) / m_PixelsPerSecond;
                track->AddKeyframe(std::max(0.0f, clickTime), glm::vec4(1.0f));
            }
        }
    }

    // ════════════════════════════════════════════════════════
    //  Atlas Generator
    // ════════════════════════════════════════════════════════

    void AnimationEditorPanel::DrawAtlasSection()
    {
        auto clip = GetCurrentClip();
        if (!clip) return;

        if (!ImGui::CollapsingHeader("Atlas Generator"))
            return;

        AtlasConfig& atlas = m_AtlasConfigs[m_CurrentClipName];

        ImTextureID atlasThumb = atlas.Texture
            ? static_cast<ImTextureID>(static_cast<uintptr_t>(atlas.Texture->GetRendererID()))
            : static_cast<ImTextureID>(0);

        // 注意颠倒图片显示
        ImGui::Image(atlasThumb, ImVec2(60, 60), ImVec2(0, 1), ImVec2(1, 0));
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
            {
                const wchar_t* wpath = static_cast<const wchar_t*>(payload->Data);
                atlas.Texture = Texture2D::Create(AssetPath::ToProjectRelative(
                    GetEditorAssetPath() / wpath).generic_string());
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::SetNextItemWidth(60); ImGui::DragInt("Cols", &atlas.Cols, 1, 1, 64);
        ImGui::SetNextItemWidth(60); ImGui::DragInt("Rows", &atlas.Rows, 1, 1, 64);
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::SetNextItemWidth(60); ImGui::DragInt("Start Col", &atlas.StartCol, 1, 0, 63);
        ImGui::SetNextItemWidth(60); ImGui::DragInt("Start Row", &atlas.StartRow, 1, 0, 63);
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::SetNextItemWidth(60); ImGui::DragInt("Frame Count", &atlas.FrameCount, 1, 1, 256);
        ImGui::SetNextItemWidth(60); ImGui::DragFloat("Dur/frame", &atlas.Duration, 0.01f, 0.01f, 2.0f, "%.2fs");
        ImGui::EndGroup();

        const bool canGenerate = atlas.Texture
            && atlas.Cols > 0 && atlas.Rows > 0 && atlas.FrameCount > 0;

        if (!canGenerate)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }

        if (ImGui::Button("Generate Frames"))
        {
            clip->ClearFrames();
            const float uStep = 1.0f / atlas.Cols;
            const float vStep = 1.0f / atlas.Rows;
            int col = atlas.StartCol, row = atlas.StartRow;
            for (int i = 0; i < atlas.FrameCount; i++)
            {
                glm::vec2 uvMin = { col * uStep,       row * vStep };
                glm::vec2 uvMax = { (col + 1) * uStep, (row + 1) * vStep };
                clip->AddFrame({ atlas.Texture, uvMin, uvMax, atlas.Duration });
                if (++col >= atlas.Cols) { col = 0; row++; }
            }
        }

        if (!canGenerate)
        {
            ImGui::PopStyleVar();
            ImGui::PopItemFlag();
            ImGui::SameLine();
            ImGui::TextDisabled("(drop a texture first)");
        }
    }

    // ════════════════════════════════════════════════════════
    //  Frames 列表
    // ════════════════════════════════════════════════════════

    void AnimationEditorPanel::DrawFramesSection()
    {
        auto clip = GetCurrentClip();
        if (!clip) return;

        if (!ImGui::CollapsingHeader("Frames"))
            return;

        auto& frames = clip->GetFrames();
        int frameToDelete = -1;

        for (int i = 0; i < (int)frames.size(); i++)
        {
            ImGui::PushID(i);
            auto& frame = frames[i];

            ImTextureID thumbID = frame.Texture
                ? static_cast<ImTextureID>(
                    static_cast<uintptr_t>(frame.Texture->GetRendererID()))
                : static_cast<ImTextureID>(0);

            // 注意颠倒图片显示
            ImGui::Image(thumbID, ImVec2(48, 48),
                ImVec2(frame.TexCoordMin.x, frame.TexCoordMax.y), // 这里改用 Max.y
                ImVec2(frame.TexCoordMax.x, frame.TexCoordMin.y)  // 这里改用 Min.y
            );

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload =
                    ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
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
                ImGui::SetTooltip("Drop texture here");

            ImGui::SameLine();
            ImGui::BeginGroup();

            ImGui::TextDisabled("Frame %d", i);

            ImGui::SetNextItemWidth(80.0f);
            ImGui::DragFloat("Duration##dur", &frame.Duration,
                0.01f, 0.01f, 5.0f, "%.2fs");

            if (ImGui::TreeNode("UV##uv"))
            {
                ImGui::SetNextItemWidth(120.0f);
                ImGui::DragFloat2("Min##uvmin", glm::value_ptr(frame.TexCoordMin),
                    0.005f, 0.0f, 1.0f, "%.3f");
                ImGui::SetNextItemWidth(120.0f);
                ImGui::DragFloat2("Max##uvmax", glm::value_ptr(frame.TexCoordMax),
                    0.005f, 0.0f, 1.0f, "%.3f");
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
            frames.erase(frames.begin() + frameToDelete);

        if (ImGui::Button("+ Add Frame"))
            clip->AddFrame(AnimationFrame{});

        ImGui::SameLine();
        ImGui::TextDisabled("%d frame(s)", (int)frames.size());
    }

} // namespace Wheatear
