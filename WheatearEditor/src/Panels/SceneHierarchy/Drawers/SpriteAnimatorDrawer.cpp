#include "SpriteAnimatorDrawer.h"
#include "../ComponentDrawers.h"
#include <imgui/imgui.h>
#include "Wheatear/Scene/Components.h"

namespace Wheatear {

    void DrawSpriteAnimatorComponent(Entity entity)
    {
        DrawComponent<SpriteAnimatorComponent>("Sprite Animator", entity, [](auto& c)
            {
                // 当前播放状态
                ImGui::Text("Clip: %s",
                    c.CurrentClipName.empty() ? "(none)" : c.CurrentClipName.c_str());

                ImGui::SameLine();
                if (c.IsPlaying)
                {
                    if (ImGui::SmallButton("Pause"))  c.IsPlaying = false;
                }
                else
                {
                    if (ImGui::SmallButton("Resume")) c.IsPlaying = true;
                }

                // Clip 数量提示
                ImGui::TextDisabled("%d clip(s) — edit in Animation Editor window",
                    (int)c.Clips.size());
            });
    }

} // namespace Wheatear