#include "wepch.h"
#include "PhysicsDrawers.h"

#include "../ComponentDrawers.h"
#include "Editor/EditorLocale.h"

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

                if (ImGui::BeginCombo(EditorLocale::Text("Body Type", "刚体类型"), currentStr))
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

                ImGui::Checkbox(EditorLocale::Text("Fixed Rotation", "锁定旋转"), &c.FixedRotation);
                ImGui::DragFloat(EditorLocale::Text("Gravity Scale", "重力缩放"), &c.GravityScale, 0.01f, -10.0f, 10.0f);
            });
    }


    void DrawBoxCollider2DComponent(Entity entity)
    {
        DrawComponent<BoxCollider2DComponent>("Box Collider 2D", entity, [](auto& c)
            {
                ImGui::DragFloat2(EditorLocale::Text("Offset", "偏移"), glm::value_ptr(c.Offset));
                ImGui::DragFloat2(EditorLocale::Text("Size", "大小"), glm::value_ptr(c.Size));
                ImGui::DragFloat(EditorLocale::Text("Density", "密度"), &c.Density, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat(EditorLocale::Text("Friction", "摩擦"), &c.Friction, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat(EditorLocale::Text("Restitution", "弹性"), &c.Restitution, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat(EditorLocale::Text("Restitution Threshold", "弹性阈值"), &c.RestitutionThreshold, 0.01f, 0.0f);
            });
    }


    void DrawCircleCollider2DComponent(Entity entity)
    {
        DrawComponent<CircleCollider2DComponent>("Circle Collider 2D", entity, [](auto& c)
            {
                ImGui::DragFloat2(EditorLocale::Text("Offset", "偏移"), glm::value_ptr(c.Offset));
                ImGui::DragFloat(EditorLocale::Text("Radius", "半径"), &c.Radius);
                ImGui::DragFloat(EditorLocale::Text("Density", "密度"), &c.Density, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat(EditorLocale::Text("Friction", "摩擦"), &c.Friction, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat(EditorLocale::Text("Restitution", "弹性"), &c.Restitution, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat(EditorLocale::Text("Restitution Threshold", "弹性阈值"), &c.RestitutionThreshold, 0.01f, 0.0f);
            });
    }

} // namespace Wheatear
