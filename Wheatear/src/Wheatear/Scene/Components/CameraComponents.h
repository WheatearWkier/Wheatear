#pragma once

// Camera component.

#include "Wheatear/Renderer/SceneCamera.h"

namespace Wheatear {

    struct CameraComponent
    {
        SceneCamera Camera;
        bool        Primary = true;
        bool        FixedAspectRatio = false;

        CameraComponent() = default;
        CameraComponent(const CameraComponent&) = default;
    };

} // namespace Wheatear
