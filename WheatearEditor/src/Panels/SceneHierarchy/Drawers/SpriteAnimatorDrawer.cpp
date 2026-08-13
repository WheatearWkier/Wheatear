#include "SpriteAnimatorDrawer.h"
#include "../ComponentDrawers.h"
#include "Editor/EditorLocale.h"
#include <imgui/imgui.h>
#include "Wheatear/Scene/Components.h"

namespace Wheatear {

    void DrawSpriteAnimatorComponent(Entity entity)
    {
        DrawComponent<SpriteAnimatorComponent>("Sprite Animator", entity, [](auto& c)
            {
                ImGui::Text("Clip: %s",
                    c.CurrentClipName.empty() ? "(none)" : c.CurrentClipName.c_str());

                ImGui::SameLine();
                if (c.IsPlaying)
                {
                    if (ImGui::SmallButton(EditorLocale::Text("Pause", "暂停")))  c.IsPlaying = false;
                }
                else
                {
                    if (ImGui::SmallButton(EditorLocale::Text("Resume", "继续"))) c.IsPlaying = true;
                }

                ImGui::Checkbox("Play On Start", &c.PlayOnStart);
                ImGui::SameLine();
                ImGui::Checkbox(EditorLocale::Text("Fire Events", "触发事件"), &c.FireEvents);
                ImGui::DragFloat(EditorLocale::Text("Playback Speed", "播放速度"), &c.PlaybackSpeed, 0.01f, 0.0f, 8.0f);

                ImGui::Text(EditorLocale::Text("Default: %s", "默认: %s"),
                    c.DefaultClipName.empty() ? "(none)" : c.DefaultClipName.c_str());
                ImGui::Text(EditorLocale::Text("Time: %.3fs  Frame: %d  Finished: %s", "时间: %.3fs  帧: %d  结束: %s"),
                    c.ElapsedTime,
                    c.CurrentFrameIndex,
                    c.IsFinished ? "true" : "false");

                ImGui::TextDisabled("%d clip(s) - edit in Animation Editor window",
                    (int)c.Clips.size());

                if (!c.ExternalClipAssets.empty())
                {
                    ImGui::Separator();
                    ImGui::TextDisabled(EditorLocale::Text("External .wtanim assets", "外部 .wtanim 动画资产"));
                    for (const auto& [name, path] : c.ExternalClipAssets)
                        ImGui::BulletText("%s -> %s", name.c_str(), path.c_str());
                }
            });
    }

} // namespace Wheatear
