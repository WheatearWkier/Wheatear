#include "TransformDrawer.h"

#include "../ComponentDrawers.h"
#include "../WidgetHelpers.h"

#include <glm/gtc/type_ptr.hpp>

#include "Wheatear/Scene/Components.h"

namespace Wheatear {

    void DrawTransformComponent(Entity entity)
    {
        DrawComponent<TransformComponent>("Transform", entity, [](auto& c)
            {
                UI::DrawVec3Control("Translation", c.Translation);

                glm::vec3 rotation = glm::degrees(c.Rotation);
                UI::DrawVec3Control("Rotation", rotation);
                c.Rotation = glm::radians(rotation);

                UI::DrawVec3Control("Scale", c.Scale, 1.0f);
            });
    }

} // namespace Wheatear