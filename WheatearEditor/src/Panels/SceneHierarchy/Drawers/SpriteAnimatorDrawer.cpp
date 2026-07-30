#include "SpriteAnimatorDrawer.h"
#include "../ComponentDrawers.h"
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
                    if (ImGui::SmallButton("Pause"))  c.IsPlaying = false;
                }
                else
                {
                    if (ImGui::SmallButton("Resume")) c.IsPlaying = true;
                }

                ImGui::Checkbox("Play On Start", &c.PlayOnStart);
                ImGui::SameLine();
                ImGui::Checkbox("Fire Events", &c.FireEvents);

                ImGui::Text("Default: %s",
                    c.DefaultClipName.empty() ? "(none)" : c.DefaultClipName.c_str());
                ImGui::Text("Time: %.3fs  Frame: %d  Finished: %s",
                    c.ElapsedTime,
                    c.CurrentFrameIndex,
                    c.IsFinished ? "true" : "false");

                ImGui::TextDisabled("%d clip(s) - edit in Animation Editor window",
                    (int)c.Clips.size());
            });
    }

} // namespace Wheatear
