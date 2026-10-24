#include "wtpch.h"
#include "RTSCameraSystem.h"

#include "RTSComponents.h"
#include "RTSUtilities.h"

#include "Wheatear/Input/Input.h"
#include "Wheatear/Input/KeyCodes.h"
#include "Wheatear/Input/MouseButtonCodes.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace Wheatear {

    void RTSCameraSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        RTS::CameraView camera;
        if (!RTS::FindCamera(scene, camera))
            return;

        RTSCameraComponent& rts = *camera.RTS;
        const float dt = ts.GetSeconds();

        // Ground-plane forward/right derived from yaw. The camera orbits the
        // target, so its ground forward points from the camera toward target.
        const glm::vec3 forward(-std::sin(rts.Yaw), 0.0f, -std::cos(rts.Yaw));
        const glm::vec3 right(std::cos(rts.Yaw), 0.0f, -std::sin(rts.Yaw));

        glm::vec3 pan(0.0f);
        if (Input::IsKeyPressed(WT_KEY_W) || Input::IsKeyPressed(WT_KEY_UP))
            pan += forward;
        if (Input::IsKeyPressed(WT_KEY_S) || Input::IsKeyPressed(WT_KEY_DOWN))
            pan -= forward;
        if (Input::IsKeyPressed(WT_KEY_D) || Input::IsKeyPressed(WT_KEY_RIGHT))
            pan += right;
        if (Input::IsKeyPressed(WT_KEY_A) || Input::IsKeyPressed(WT_KEY_LEFT))
            pan -= right;

        if (glm::length(pan) > 0.0f)
        {
            pan = glm::normalize(pan);
            const float speed = rts.PanSpeed * (rts.Distance / 40.0f);
            rts.Target += pan * speed * dt;
        }

        // Middle-drag rotates. Input::GetMouseDelta* is frame-consistent.
        if (Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_MIDDLE))
        {
            rts.Yaw -= Input::GetMouseDeltaX() * rts.RotateSpeed;
            rts.Pitch -= Input::GetMouseDeltaY() * rts.RotateSpeed;
        }

        rts.Pitch = glm::clamp(rts.Pitch, glm::radians(18.0f), glm::radians(82.0f));
        rts.Distance = glm::clamp(
            rts.Distance - rts.ZoomDelta * rts.ZoomStep,
            rts.MinDistance, rts.MaxDistance);
        rts.ZoomDelta = 0.0f;

        rts.Target.x = glm::clamp(rts.Target.x, rts.BoundsMin.x, rts.BoundsMax.x);
        rts.Target.z = glm::clamp(rts.Target.z, rts.BoundsMin.z, rts.BoundsMax.z);
        rts.Target.y = 0.0f;

        const glm::vec3 offset(
            std::sin(rts.Yaw) * std::cos(rts.Pitch),
            std::sin(rts.Pitch),
            std::cos(rts.Yaw) * std::cos(rts.Pitch));
        const glm::vec3 eye = rts.Target + offset * rts.Distance;

        const glm::mat4 view = glm::lookAt(eye, rts.Target, glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::quat rotation = glm::quat_cast(glm::inverse(view));

        camera.Transform->Translation = eye;
        camera.Transform->Rotation = glm::eulerAngles(rotation);
        camera.Transform->Scale = glm::vec3(1.0f);
    }

} // namespace Wheatear
