#include "wtpch.h"
#include "AnimationEditorPanel.h"

#include "Editor/EditorContentPickers.h"
#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorWidgets.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
#include <cctype>
#include <cstring>
#include <cfloat>
#include <vector>

#include "Wheatear/Animation/AnimationClipSerializer.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/EngineInfo.h"
#include "Wheatear/Scene/Components.h"

namespace Wheatear {

    static constexpr float kTrackHeight = 36.0f;
    static constexpr float kLabelWidth = 150.0f;
    static constexpr float kRulerHeight = 36.0f;

    static std::string MakeSafeClipName(const std::string& name)
    {
        std::string result;
        result.reserve(name.size());
        for (char c : name)
        {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')
                result += c;
            else
                result += '_';
        }
        if (result.empty())
            result = "clip";
        return result;
    }

    using EditorWidgets::InputString;

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


    Ref<AnimationClip> AnimationEditorPanel::GetCurrentClip() const
    {
        if (!m_Animator) return nullptr;
        auto it = m_Animator->Clips.find(m_CurrentClipName);
        return it != m_Animator->Clips.end() ? it->second : nullptr;
    }


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

    //  SyncPreviewToEntity

    void AnimationEditorPanel::SyncPreviewToEntity()
    {
        if (!m_Entity || !m_Animator) return;

        if (!m_HasSnapshot) return;

        auto clip = GetCurrentClip();
        if (!clip) return;

        float duration = clip->GetTotalDuration();
        bool  looping = clip->IsLooping();

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
                sr.UVMin = frame.TexCoordMin;
                sr.UVMax = frame.TexCoordMax;
            }
        }

        for (auto& trackBase : clip->GetPropertyTracks())
        {
            if (trackBase->GetDataType() == TrackDataType::Float)
            {
                auto track = std::static_pointer_cast<PropertyTrack<float>>(trackBase);
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

    //  TakeSnapshot / RestoreSnapshot

    void AnimationEditorPanel::TakeSnapshot()
    {
        if (!m_Entity) return;
        if (m_HasSnapshot) return;

        if (m_Entity.HasComponent<SpriteRendererComponent>())
        {
            auto& sr = m_Entity.GetComponent<SpriteRendererComponent>();
            m_SnapshotColor = sr.Color;
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


    void AnimationEditorPanel::BeginScrub()
    {
        TakeSnapshot();
        m_IsScrubbing = true;
        if (m_Scene)
            m_Scene->SetAnimationEditorPreviewActive(true);
    }

    void AnimationEditorPanel::EndScrub()
    {
        m_IsScrubbing = false;
    }


    void AnimationEditorPanel::OnImGuiRender(Timestep ts)
    {
        EditorFloatingWindow::Begin("Animation Editor", nullptr, 0, { 1180.0f, 720.0f });
        EditorFloatingWindow::DrawToggleButton("Animation Editor");

        if (!m_Entity || !m_Animator)
        {
            ImGui::TextDisabled("Select an entity with a Sprite Animator component.");
            EditorFloatingWindow::End();
            return;
        }

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

        SyncPreviewToEntity();

        DrawToolbar();
        ImGui::Separator();
        DrawTimeline();
        ImGui::Separator();
        DrawAtlasSection();
        ImGui::Separator();
        DrawEventsSection();
        ImGui::Separator();
        DrawFramesSection();

        EditorFloatingWindow::End();
    }


    void AnimationEditorPanel::DrawToolbar()
    {
        if (!m_Animator) return;

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
        bool fireEvents = m_Animator->FireEvents;
        if (ImGui::Checkbox("Fire Events", &fireEvents))
            m_Animator->FireEvents = fireEvents;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Animation events execute CommandBus commands at runtime");

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
            size_t clipIndex = 0;
            for (auto& [name, clip] : m_Animator->Clips)
            {
                bool sel = (name == m_CurrentClipName);
                const std::string label = EditorWidgets::LabelWithId(
                    name,
                    "animation_clip:" + std::to_string(clipIndex++));
                if (ImGui::Selectable(label.c_str(), sel))
                {
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
                StopPreview();
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

        ImGui::Spacing();

        if (m_IsPlaying)
        {
            if (ImGui::Button("|| Pause"))  m_IsPlaying = false;
        }
        else
        {
            if (ImGui::Button(">  Play"))
            {
                TakeSnapshot();

                m_IsPlaying = true;
                if (m_PlaybackTime >= clip->GetTotalDuration())
                    m_PlaybackTime = 0.0f;
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

        ImGui::SameLine(0, 20);
        if (ImGui::Button("Save Clip As..."))
        {
            std::string path = m_ClipAssetPath;
            if (path.empty())
                path = "assets/animations/" + MakeSafeClipName(clip->GetName()) + AssetFileType::AnimationClipExtension;
            if (AnimationClipSerializer::Save(clip, AssetPath::Resolve(path)))
            {
                m_ClipAssetPath = path;
                m_Animator->BindExternalClipAsset(clip->GetName(), path);
                WT_CORE_INFO("Saved animation clip asset: {}", path);
            }
            else
            {
                WT_CORE_ERROR("Failed to save animation clip asset: {}", path);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Load .wtanim..."))
        {
            if (EditorContentPickers::DrawAssetField("Clip Asset", m_ClipAssetPath,
                    EditorWidgets::AssetReferenceKind::AnimationClip, 512))
            {
                Ref<AnimationClip> loaded = AnimationClipSerializer::Load(AssetPath::Resolve(m_ClipAssetPath));
                if (loaded)
                {
                    m_Animator->AddClip(loaded);
                    m_Animator->BindExternalClipAsset(loaded->GetName(), m_ClipAssetPath);
                    m_CurrentClipName = loaded->GetName();
                    WT_CORE_INFO("Loaded animation clip asset: {}", m_ClipAssetPath);
                }
                else
                {
                    WT_CORE_ERROR("Failed to load animation clip asset: {}", m_ClipAssetPath);
                }
            }
        }
        EditorWidgets::HelpTooltip("Saves the current clip as a reusable .wtanim asset and binds it to this entity. "
            "Other entities can bind the same asset; runtime loads it automatically.");

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

        ImGui::SameLine(0, 20);
        if (ImGui::Button("+ Add Event"))
        {
            AnimationEvent event;
            event.Time = std::max(0.0f, m_PlaybackTime);
            event.Name = "event";
            event.Command = "";
            clip->AddEvent(event);
        }
    }


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
        const int   trackCount = 2 + (int)clip->GetPropertyTracks().size();
        const float totalHeight = kRulerHeight + trackCount * kTrackHeight + 4.0f;

        ImGui::BeginChild("##labels",
            ImVec2(kLabelWidth, totalHeight + 20.0f), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            ImGui::Dummy(ImVec2(kLabelWidth, kRulerHeight));

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.85f, 0.5f, 1.0f));
            ImGui::Text("  Frames (%d)", clip->GetFrameCount());
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.68f, 1.0f, 1.0f));
            ImGui::Text("  Events (%d)", (int)clip->GetEvents().size());
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

            ImGui::SetCursorScreenPos(ImVec2(ImGui::GetWindowPos().x, origin.y));
            ImGui::InvisibleButton("##ruler_hit",
                ImVec2(ImGui::GetWindowWidth(), kRulerHeight));

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

            if (ImGui::IsWindowHovered())
            {
                float wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0.0f && ImGui::GetIO().KeyCtrl)
                    m_PixelsPerSecond = std::clamp(
                        m_PixelsPerSecond + wheel * 15.0f, 40.0f, 400.0f);
            }

            trackY += kRulerHeight;

            DrawFrameTrack(ImVec2(origin.x, trackY), kTrackHeight);
            trackY += kTrackHeight;

            DrawEventTrack(ImVec2(origin.x, trackY), kTrackHeight);
            trackY += kTrackHeight;

            auto& tracks = clip->GetPropertyTracks();
            for (int ti = 0; ti < (int)tracks.size(); ti++)
            {
                DrawPropertyTrack(ImVec2(origin.x, trackY), kTrackHeight, ti);
                trackY += kTrackHeight;
            }

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


    void AnimationEditorPanel::DrawEventTrack(ImVec2 origin, float trackHeight)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        auto clip = GetCurrentClip();
        if (!clip) return;

        const float duration = std::max(clip->GetTotalDuration(), 1.0f);
        const float width = duration * m_PixelsPerSecond + 60.0f;
        dl->AddRectFilled(origin,
            ImVec2(origin.x + width, origin.y + trackHeight),
            IM_COL32(34, 30, 48, 255));

        ImGui::SetCursorScreenPos(origin);
        ImGui::InvisibleButton("##event_track_bg", ImVec2(width, trackHeight));
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
        {
            float clickTime = (ImGui::GetIO().MousePos.x - origin.x) / m_PixelsPerSecond;
            AnimationEvent event;
            event.Time = std::clamp(clickTime, 0.0f, duration);
            event.Name = "event";
            event.Command = "";
            clip->AddEvent(event);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Double-click to add an animation event.");

        auto& events = clip->GetEvents();
        int eventToDelete = -1;
        bool sortEvents = false;

        for (int i = 0; i < (int)events.size(); ++i)
        {
            auto& event = events[i];
            const float x = origin.x + event.Time * m_PixelsPerSecond;
            const float centerY = origin.y + trackHeight * 0.5f;

            ImVec2 marker[3] = {
                ImVec2(x, centerY - 9.0f),
                ImVec2(x + 8.0f, centerY + 7.0f),
                ImVec2(x - 8.0f, centerY + 7.0f)
            };
            dl->AddConvexPolyFilled(marker, 3, IM_COL32(170, 120, 255, 235));
            dl->AddPolyline(marker, 3, IM_COL32(230, 220, 255, 255), true, 1.2f);

            ImGui::SetCursorScreenPos(ImVec2(x - 8.0f, centerY - 9.0f));
            ImGui::PushID(50000 + i);
            ImGui::InvisibleButton("##anim_event", ImVec2(16.0f, 18.0f));
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))
            {
                event.Time += ImGui::GetIO().MouseDelta.x / m_PixelsPerSecond;
                event.Time = std::clamp(event.Time, 0.0f, duration);
                sortEvents = true;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s\n%.3fs\n%s",
                    event.Name.empty() ? "(unnamed)" : event.Name.c_str(),
                    event.Time,
                    event.Command.empty() ? "(no command)" : event.Command.c_str());

            if (ImGui::BeginPopupContextItem("##anim_event_ctx"))
            {
                ImGui::Text("Animation Event");
                ImGui::Separator();
                ImGui::SetNextItemWidth(90.0f);
                if (ImGui::DragFloat("Time", &event.Time, 0.01f, 0.0f, duration, "%.3fs"))
                    sortEvents = true;
                InputString("Name", event.Name, 128);
                InputString("Command", event.Command, 320);
                ImGui::TextDisabled("Placeholders: {entity} {clip} {event}");
                ImGui::Separator();
                if (ImGui::MenuItem("Delete"))
                    eventToDelete = i;
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }

        if (eventToDelete >= 0)
            clip->RemoveEvent(eventToDelete);
        else if (sortEvents)
            clip->SortEvents();
    }

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
                        kfToDelete = ki;
                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }

            if (kfToDelete >= 0)
                track->RemoveKeyframe(kfToDelete);

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
                        kfToDelete = ki;
                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }

            if (kfToDelete >= 0)
                track->RemoveKeyframe(kfToDelete);

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

} // namespace Wheatear
