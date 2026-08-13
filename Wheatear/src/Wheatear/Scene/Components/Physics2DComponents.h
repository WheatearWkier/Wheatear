#pragma once

// Box2D-backed 2D physics components.

#ifndef GLM_ENABLE_EXPERIMENTAL
	#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>

namespace Wheatear {

    struct Rigidbody2DComponent
    {
        enum class BodyType { Static = 0, Dynamic, Kinematic };

        BodyType Type = BodyType::Static;
        bool     FixedRotation = false;
        float    GravityScale = 1.0f;
        void*    RuntimeBody = nullptr;

        Rigidbody2DComponent() = default;
        Rigidbody2DComponent(const Rigidbody2DComponent&) = default;
    };

    struct BoxCollider2DComponent
    {
        glm::vec2 Offset = { 0.0f, 0.0f };
        glm::vec2 Size = { 0.5f, 0.5f };
        float     Density = 1.0f;
        float     Friction = 0.5f;
        float     Restitution = 0.0f;
        float     RestitutionThreshold = 0.5f;
        void* RuntimeFixture = nullptr;

        BoxCollider2DComponent() = default;
        BoxCollider2DComponent(const BoxCollider2DComponent&) = default;
    };

    struct CircleCollider2DComponent
    {
        glm::vec2 Offset = { 0.0f, 0.0f };
        float     Radius = 0.5f;
        float     Density = 1.0f;
        float     Friction = 0.5f;
        float     Restitution = 0.0f;
        float     RestitutionThreshold = 0.5f;
        void* RuntimeFixture = nullptr;

        CircleCollider2DComponent() = default;
        CircleCollider2DComponent(const CircleCollider2DComponent&) = default;
    };

} // namespace Wheatear
