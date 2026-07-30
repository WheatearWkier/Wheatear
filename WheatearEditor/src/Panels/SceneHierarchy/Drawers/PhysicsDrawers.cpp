#include "PhysicsDrawers.h"

#include "../ComponentDrawers.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include "Wheatear/Scene/Components.h"

namespace Wheatear {


    void DrawRigidbody2DComponent(Entity entity)
    {
        DrawComponent<Rigidbody2DComponent>("Rigidbody 2D", entity, [](auto& c)
            {
                const char* bodyTypeStrings[] = { "Static", "Dynamic", "Kinematic" };
                const char* currentStr = bodyTypeStrings[static_cast<int>(c.Type)];

                if (ImGui::BeginCombo("Body Type", currentStr))
                {
                    for (int i = 0; i < 3; i++)
                    {
                        const bool isSelected = (static_cast<int>(c.Type) == i);
                        if (ImGui::Selectable(bodyTypeStrings[i], isSelected))
                            c.Type = static_cast<Rigidbody2DComponent::BodyType>(i);
                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                ImGui::Checkbox("Fixed Rotation", &c.FixedRotation);
            });
    }


    void DrawBoxCollider2DComponent(Entity entity)
    {
        DrawComponent<BoxCollider2DComponent>("Box Collider 2D", entity, [](auto& c)
            {
                ImGui::DragFloat2("Offset", glm::value_ptr(c.Offset));
                ImGui::DragFloat2("Size", glm::value_ptr(c.Size));
                ImGui::DragFloat("Density", &c.Density, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Friction", &c.Friction, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Restitution", &c.Restitution, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Restitution Threshold", &c.RestitutionThreshold, 0.01f, 0.0f);
            });
    }


    void DrawCircleCollider2DComponent(Entity entity)
    {
        DrawComponent<CircleCollider2DComponent>("Circle Collider 2D", entity, [](auto& c)
            {
                ImGui::DragFloat2("Offset", glm::value_ptr(c.Offset));
                ImGui::DragFloat("Radius", &c.Radius);
                ImGui::DragFloat("Density", &c.Density, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Friction", &c.Friction, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Restitution", &c.Restitution, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Restitution Threshold", &c.RestitutionThreshold, 0.01f, 0.0f);
            });
    }

} // namespace Wheatear
