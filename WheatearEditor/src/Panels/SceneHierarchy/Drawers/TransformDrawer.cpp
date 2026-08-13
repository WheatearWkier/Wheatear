#include "TransformDrawer.h"

#include "../ComponentDrawers.h"
#include "../WidgetHelpers.h"
#include "Editor/EditorLocale.h"

#include <glm/gtc/type_ptr.hpp>

#include "Wheatear/Scene/Components.h"

namespace Wheatear {

    void DrawTransformComponent(Entity entity)
    {
        DrawComponent<TransformComponent>(EditorLocale::Text("Transform", "变换"), entity, [](auto& c)
            {
                UI::DrawVec3Control(EditorLocale::Text("Translation", "位移"), c.Translation);

                glm::vec3 rotation = glm::degrees(c.Rotation);
                UI::DrawVec3Control(EditorLocale::Text("Rotation", "旋转"), rotation);
                c.Rotation = glm::radians(rotation);

                UI::DrawVec3Control(EditorLocale::Text("Scale", "缩放"), c.Scale, 1.0f);
            });
    }

} // namespace Wheatear