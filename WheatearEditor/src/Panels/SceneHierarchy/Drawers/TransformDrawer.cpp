#include "wepch.h"
#include "TransformDrawer.h"

#include "../ComponentDrawers.h"
#include "../WidgetHelpers.h"
#include "Editor/EditorLocale.h"

#include <glm/gtc/type_ptr.hpp>

#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

namespace Wheatear {

    void DrawTransformComponent(Entity entity)
    {
        DrawComponent<TransformComponent>(EditorLocale::Text("Transform", "变换"), entity, [entity](auto& c)
            {
                UI::DrawVec3Control(EditorLocale::Text("Translation", "位移"), c.Translation);

                glm::vec3 rotation = glm::degrees(c.Rotation);
                UI::DrawVec3Control(EditorLocale::Text("Rotation", "旋转"), rotation);
                c.Rotation = glm::radians(rotation);

                UI::DrawVec3Control(EditorLocale::Text("Scale", "缩放"), c.Scale, 1.0f);

                uint64_t parentId = static_cast<uint64_t>(c.Parent);
                if (ImGui::InputScalar(
                        EditorLocale::Text("Parent UUID", "父实体 UUID"),
                        ImGuiDataType_U64,
                        &parentId))
                {
                    if (parentId == 0)
                    {
                        c.Parent = 0;
                    }
                    else
                    {
                        Scene* scene = entity.GetScene();
                        const UUID candidate(parentId);
                        if (scene && scene->FindEntityByUUID(candidate))
                            c.Parent = candidate;
                    }
                }

                if (c.Parent != 0)
                {
                    Scene* scene = entity.GetScene();
                    Entity parent = scene ? scene->FindEntityByUUID(c.Parent) : Entity{};
                    if (parent)
                    {
                        ImGui::TextDisabled("%s: %s",
                            EditorLocale::Text("Parent", "父实体"),
                            parent.GetName().c_str());
                    }
                    else
                    {
                        ImGui::TextColored({ 1.0f, 0.45f, 0.35f, 1.0f }, "%s",
                            EditorLocale::Text("Parent entity is missing.", "父实体不存在。"));
                    }

                    ImGui::SameLine();
                    if (ImGui::SmallButton(EditorLocale::Text("Clear Parent", "清除父实体")))
                        c.Parent = 0;
                }
            });
    }

} // namespace Wheatear