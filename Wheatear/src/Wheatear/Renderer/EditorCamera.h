#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include "Wheatear/Core/Timestep.h"
#include "Wheatear/Events/Event.h"
#include "Wheatear/Events/MouseEvent.h"
#include "Wheatear/Renderer/Camera.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Wheatear {

    class EditorCamera : public Camera
    {
    public:
        enum class Mode
        {
            Orbit,
            Fly
        };

    public:
        EditorCamera() = default;
        EditorCamera(float fovDegrees, float aspectRatio, float nearClip, float farClip);

        void OnUpdate(Timestep ts);
        void OnEvent(Event& e);

        void SetViewportSize(float width, float height);
        void SetViewTransform(const glm::vec3& position,
            const glm::vec3& rotation,
            float orbitDistance = 10.0f);

        // Getters
        float GetDistance() const { return m_Distance; }
        void  SetDistance(float distance) { m_Distance = distance; }

        float GetPitch() const { return m_Pitch; }
        float GetYaw()   const { return m_Yaw; }

        Mode  GetMode()  const { return m_Mode; }

        const glm::mat4& GetViewMatrix()     const { return m_ViewMatrix; }
        glm::mat4        GetViewProjection() const { return m_Projection * m_ViewMatrix; }

        const glm::vec3& GetPosition()         const { return m_Position; }
        glm::quat        GetOrientation()      const;
        glm::vec3        GetUpDirection()      const;
        glm::vec3        GetRightDirection()   const;
        glm::vec3        GetForwardDirection() const;

    private:
        void UpdateProjection();
        void UpdateView();

        bool OnMouseScroll(MouseScrolledEvent& e);

        void MousePan(const glm::vec2& delta);
        void MouseRotate(const glm::vec2& delta);
        void MouseZoom(float delta);

        glm::vec3 CalculateOrbitPosition() const;
        std::pair<float, float> PanSpeed()    const;
        float                   RotationSpeed() const;
        float                   ZoomSpeed()     const;

    private:
        float m_FOV = 45.0f;
        float m_AspectRatio = 1.778f;
        float m_NearClip = 0.1f;
        float m_FarClip = 1000.0f;

        glm::mat4 m_ViewMatrix = glm::mat4(1.0f);

        glm::vec3 m_Position = { 0.0f, 8.0f, 15.0f };
        float     m_Pitch = 0.0f;
        float     m_Yaw = 0.0f;

        glm::vec3 m_FocalPoint = { 0.0f, 0.0f, 0.0f };
        float     m_Distance = 10.0f;

        float m_FlySpeed = 5.0f;

        Mode m_Mode = Mode::Orbit;

        glm::vec2 m_InitialMousePosition = { 0.0f, 0.0f };

        float m_ViewportWidth = 1920.0f;
        float m_ViewportHeight = 1080.0f;
    };

} // namespace Wheatear
