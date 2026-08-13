#include "wepch.h"
#include "AnimationEditorPanel.h"

#include "Editor/EditorCommands.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Wheatear/Animation/AnimationClip.h"
#include "Wheatear/Scene/Components.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace Wheatear {

    namespace {

        static constexpr float kTrackHeight = 36.0f;
        static constexpr float kLabelWidth = 150.0f;
        static constexpr float kRulerHeight = 36.0f;

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

    } // namespace

    void AnimationEditorPanel::DrawTimeline()
    {
        auto clip = GetCurrentClip();
        if (!clip)
        {
            ImGui::TextDisabled(EditorLocale::Text("No clip selected.", "未选择片段。"));
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
                    const int trackIndex = ti;
                    const std::string clipName = m_CurrentClipName;
                    ApplyAnimatorEdit(m_Entity,
                        [clipName, trackIndex](SpriteAnimatorComponent& component)
                    {
                        auto it = component.Clips.find(clipName);
                        if (it != component.Clips.end())
                            it->second->RemovePropertyTrack(trackIndex);
                    });
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
            const float clickTime = (ImGui::GetIO().MousePos.x - origin.x) / m_PixelsPerSecond;
            const float eventTime = std::clamp(clickTime, 0.0f, duration);
            const std::string clipName = m_CurrentClipName;
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

            ImGui::SetCursorScreenPos(ImVec2(x - 10.0f, centerY - 11.0f));
            ImGui::PushID(50000 + i);
            ImGui::InvisibleButton("##anim_event", ImVec2(20.0f, 22.0f));
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
                ImGui::TextDisabled(EditorLocale::Text("Placeholders: {entity} {clip} {event}", "占位符: {entity} {clip} {event}"));
                ImGui::Separator();
                if (ImGui::MenuItem("Delete"))
                    eventToDelete = i;
                ImGui::EndPopup();
            }
            ImGui::PopID();
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

                ImGui::SetCursorScreenPos(ImVec2(kx - 8.0f, ky - 8.0f));
                ImGui::PushID(ki + trackIndex * 1000);
                ImGui::InvisibleButton("##kf", ImVec2(16.0f, 16.0f));

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
                    ImGui::Text(EditorLocale::Text("Keyframe %d", "关键帧 %d"), ki);
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
            {
                const int deleteIndex = kfToDelete;
                const std::string clipName = m_CurrentClipName;
                const int trackIdx = trackIndex;
                ApplyAnimatorEdit(m_Entity,
                    [clipName, trackIdx, deleteIndex](SpriteAnimatorComponent& component)
                {
                    auto it = component.Clips.find(clipName);
                    if (it == component.Clips.end())
                        return;
                    auto& tracks = it->second->GetPropertyTracks();
                    if (trackIdx < 0 || trackIdx >= (int)tracks.size())
                        return;
                    auto track = std::static_pointer_cast<PropertyTrack<float>>(tracks[trackIdx]);
                    track->RemoveKeyframe(deleteIndex);
                });
            }

            ImGui::SetCursorScreenPos(origin);
            ImGui::InvisibleButton(
                ("##track_bg" + std::to_string(trackIndex)).c_str(),
                ImVec2(totalDur * m_PixelsPerSecond + 60.0f, trackHeight));
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            {
                float clickTime = (ImGui::GetIO().MousePos.x - origin.x) / m_PixelsPerSecond;
                clickTime = std::max(0.0f, clickTime);
                const std::string clipName = m_CurrentClipName;
                const int trackIdx = trackIndex;
                const float kfTime = clickTime;
                ApplyAnimatorEdit(m_Entity,
                    [clipName, trackIdx, kfTime](SpriteAnimatorComponent& component)
                {
                    auto it = component.Clips.find(clipName);
                    if (it == component.Clips.end())
                        return;
                    auto& tracks = it->second->GetPropertyTracks();
                    if (trackIdx < 0 || trackIdx >= (int)tracks.size())
                        return;
                    auto track = std::static_pointer_cast<PropertyTrack<float>>(tracks[trackIdx]);
                    track->AddKeyframe(kfTime, 0.0f);
                });
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
                    ImGui::Text(EditorLocale::Text("Keyframe %d", "关键帧 %d"), ki);
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
            {
                const int deleteIndex = kfToDelete;
                const std::string clipName = m_CurrentClipName;
                const int trackIdx = trackIndex;
                ApplyAnimatorEdit(m_Entity,
                    [clipName, trackIdx, deleteIndex](SpriteAnimatorComponent& component)
                {
                    auto it = component.Clips.find(clipName);
                    if (it == component.Clips.end())
                        return;
                    auto& tracks = it->second->GetPropertyTracks();
                    if (trackIdx < 0 || trackIdx >= (int)tracks.size())
                        return;
                    auto track = std::static_pointer_cast<PropertyTrack<glm::vec4>>(tracks[trackIdx]);
                    track->RemoveKeyframe(deleteIndex);
                });
            }

            ImGui::SetCursorScreenPos(origin);
            ImGui::InvisibleButton(
                ("##track_bg4_" + std::to_string(trackIndex)).c_str(),
                ImVec2(totalDur * m_PixelsPerSecond + 60.0f, trackHeight));
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            {
                float clickTime = (ImGui::GetIO().MousePos.x - origin.x) / m_PixelsPerSecond;
                const std::string clipName = m_CurrentClipName;
                const int trackIdx = trackIndex;
                const float kfTime = std::max(0.0f, clickTime);
                ApplyAnimatorEdit(m_Entity,
                    [clipName, trackIdx, kfTime](SpriteAnimatorComponent& component)
                {
                    auto it = component.Clips.find(clipName);
                    if (it == component.Clips.end())
                        return;
                    auto& tracks = it->second->GetPropertyTracks();
                    if (trackIdx < 0 || trackIdx >= (int)tracks.size())
                        return;
                    auto track = std::static_pointer_cast<PropertyTrack<glm::vec4>>(tracks[trackIdx]);
                    track->AddKeyframe(kfTime, glm::vec4(1.0f));
                });
            }
        }
    }
} // namespace Wheatear
