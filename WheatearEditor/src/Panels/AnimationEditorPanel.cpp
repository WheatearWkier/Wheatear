#include "wepch.h"
#include "AnimationEditorPanel.h"

#include "Editor/EditorContentPickers.h"
#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Editor/EditorCommands.h"

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
        if (ImGui::Checkbox(EditorLocale::Text("Play On Start", "开始时播放"), &playOnStart))
            m_Animator->PlayOnStart = playOnStart;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Auto-play default clip when scene starts");

        ImGui::SameLine(0, 20);
        bool fireEvents = m_Animator->FireEvents;
        if (ImGui::Checkbox(EditorLocale::Text("Fire Events", "触发事件"), &fireEvents))
            m_Animator->FireEvents = fireEvents;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Animation events execute CommandBus commands at runtime");

        ImGui::SameLine(0, 20);

        if (ImGui::Button(EditorLocale::Text("+ Add Clip", "+ 添加片段")))
        {
            std::string newName = "NewClip";
            int suffix = 0;
            while (m_Animator->Clips.count(
                newName + (suffix ? std::to_string(suffix) : "")))
                suffix++;
            if (suffix) newName += std::to_string(suffix);
            const std::string createdName = newName;
            ApplyAnimatorEdit(m_Entity, [createdName](SpriteAnimatorComponent& component)
            {
                component.AddClip(AnimationClip::Create(createdName, true));
            });
            m_CurrentClipName = createdName;
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
            const std::string oldName = m_CurrentClipName;
            if (!newName.empty() && !m_Animator->Clips.count(newName)
                && newName != oldName)
            {
                ApplyAnimatorEdit(m_Entity,
                    [oldName, newName](SpriteAnimatorComponent& component)
                {
                    auto clipNode = component.Clips.extract(oldName);
                    clipNode.key() = newName;
                    component.Clips.insert(std::move(clipNode));
                    if (component.CurrentClipName == oldName)
                        component.CurrentClipName = newName;
                    auto it = component.Clips.find(newName);
                    if (it != component.Clips.end())
                        it->second->SetName(newName);
                });

                // Panel-local atlas config follows the clip name (component-side
                // undo does not touch it; keep it in sync with the new name).
                auto atlasNode = m_AtlasConfigs.extract(oldName);
                if (!atlasNode.empty())
                {
                    atlasNode.key() = newName;
                    m_AtlasConfigs.insert(std::move(atlasNode));
                }

                m_CurrentClipName = newName;
                s_LastClipName = newName;
            }
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(EditorLocale::Text("Press Enter to rename clip", "按回车重命名片段"));

        if (!m_CurrentClipName.empty())
        {
            ImGui::SameLine();
            if (ImGui::Button(EditorLocale::Text("Delete Clip", "删除片段")))
            {
                StopPreview();
                const std::string deletedName = m_CurrentClipName;
                ApplyAnimatorEdit(m_Entity,
                    [deletedName](SpriteAnimatorComponent& component)
                {
                    component.Clips.erase(deletedName);
                    if (component.CurrentClipName == deletedName)
                        component.CurrentClipName = "";
                });
                m_AtlasConfigs.erase(deletedName);
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
        if (ImGui::Checkbox(EditorLocale::Text("Loop", "循环"), &looping))
        {
            const std::string clipName = m_CurrentClipName;
            ApplyAnimatorEdit(m_Entity,
                [clipName, looping](SpriteAnimatorComponent& component)
            {
                auto it = component.Clips.find(clipName);
                if (it != component.Clips.end())
                    it->second->SetLooping(looping);
            });
        }

        ImGui::SameLine(0, 20);
        if (ImGui::Button(EditorLocale::Text("Set Default", "设为默认")))
        {
            const std::string defaultName = m_CurrentClipName;
            ApplyAnimatorEdit(m_Entity,
                [defaultName](SpriteAnimatorComponent& component)
            {
                component.DefaultClipName = defaultName;
            });
        }

        if (!m_Animator->DefaultClipName.empty())
        {
            ImGui::SameLine();
            if (ImGui::Button(EditorLocale::Text("Clear Default", "清除默认")))
            {
                ApplyAnimatorEdit(m_Entity,
                    [](SpriteAnimatorComponent& component)
                {
                    component.DefaultClipName.clear();
                });
            }
        }

        ImGui::SameLine(0, 20);
        if (ImGui::Button(EditorLocale::Text("Save Clip As...", "另存片段为...")))
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
        if (ImGui::Button(EditorLocale::Text("Load .wtanim...", "加载 .wtanim...")))
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
        ImGui::TextDisabled(EditorLocale::Text("Duration: %.2fs", "时长: %.2fs"), clip->GetTotalDuration());

        ImGui::SameLine(0, 20);
        ImGui::TextDisabled(EditorLocale::Text("Zoom:", "缩放:"));
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
        if (ImGui::Button(EditorLocale::Text("+ Add Track", "+ 添加轨道")))
        {
            const AnimatedProperty prop = kAllProperties[s_SelectedPropIdx];
            const std::string clipName = m_CurrentClipName;
            ApplyAnimatorEdit(m_Entity,
                [clipName, prop](SpriteAnimatorComponent& component)
            {
                auto it = component.Clips.find(clipName);
                if (it == component.Clips.end())
                    return;
                if (prop == AnimatedProperty::SpriteColor)
                    it->second->AddVec4Track(prop);
                else
                    it->second->AddFloatTrack(prop);
            });
        }

        ImGui::SameLine(0, 20);
        if (ImGui::Button(EditorLocale::Text("+ Add Event", "+ 添加事件")))
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
    }








} // namespace Wheatear
