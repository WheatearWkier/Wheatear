#include "wepch.h"
#include "CircleRendererDrawer.h"

#include "../ComponentDrawers.h"
#include "Editor/EditorLocale.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include "Wheatear/Scene/Components.h"

namespace Wheatear {

    void DrawCircleRendererComponent(Entity entity)
    {
        DrawComponent<CircleRendererComponent>("Circle Renderer", entity, [](auto& c)
            {
                ImGui::ColorEdit4("Color", glm::value_ptr(c.Color));
                ImGui::DragFloat(EditorLocale::Text("Thickness", "厚度"), &c.Thickness, 0.025f, 0.0f, 1.0f);
                ImGui::DragFloat(EditorLocale::Text("Fade", "淡出"), &c.Fade, 0.00025f, 0.0f, 1.0f);
            });
    }

} // namespace Wheatear