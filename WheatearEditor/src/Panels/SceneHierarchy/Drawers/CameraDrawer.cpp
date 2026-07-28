#include "CameraDrawer.h"

#include "../ComponentDrawers.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include "Wheatear/Scene/Components.h"

namespace Wheatear {

    void DrawCameraComponent(Entity entity)
    {
        DrawComponent<CameraComponent>("Camera", entity, [](auto& c)
            {
                auto& camera = c.Camera;
                ImGui::Checkbox("Primary", &c.Primary);

                const char* projTypeStrings[] = { "Perspective", "Orthographic" };
                const char* currentProjStr = projTypeStrings[static_cast<int>(camera.GetProjectionType())];

                if (ImGui::BeginCombo("Projection", currentProjStr))
                {
                    for (int i = 0; i < 2; i++)
                    {
                        const bool isSelected = (currentProjStr == projTypeStrings[i]);
                        if (ImGui::Selectable(projTypeStrings[i], isSelected))
                            camera.SetProjectionType(static_cast<SceneCamera::ProjectionType>(i));
                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
                {
                    float fov = glm::degrees(camera.GetPerspectiveVerticalFOV());
                    if (ImGui::DragFloat("Vertical FOV", &fov))
                        camera.SetPerspectiveVerticalFOV(glm::radians(fov));

                    float nearClip = camera.GetPerspectiveNearClip();
                    if (ImGui::DragFloat("Near Clip", &nearClip))
                        camera.SetPerspectiveNearClip(nearClip);

                    float farClip = camera.GetPerspectiveFarClip();
                    if (ImGui::DragFloat("Far Clip", &farClip))
                        camera.SetPerspectiveFarClip(farClip);
                }

                if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
                {
                    float size = camera.GetOrthographicSize();
                    if (ImGui::DragFloat("Size", &size))
                        camera.SetOrthographicSize(size);

                    float nearClip = camera.GetOrthographicNearClip();
                    if (ImGui::DragFloat("Near Clip", &nearClip))
                        camera.SetOrthographicNearClip(nearClip);

                    float farClip = camera.GetOrthographicFarClip();
                    if (ImGui::DragFloat("Far Clip", &farClip))
                        camera.SetOrthographicFarClip(farClip);

                    ImGui::Checkbox("Fixed Aspect Ratio", &c.FixedAspectRatio);
                }
            });
    }

} // namespace Wheatear