#include "wtpch.h"
#include "EditorCamera.h"
#include "Wheatear/Input/Input.h"
#include "Wheatear/Input/KeyCodes.h"
#include "Wheatear/Input/MouseButtonCodes.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Wheatear {

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------

    EditorCamera::EditorCamera(float fovDegrees, float aspectRatio,
        float nearClip, float farClip)
        : Camera(glm::perspective(glm::radians(fovDegrees), aspectRatio, nearClip, farClip))
        , m_FOV(fovDegrees)
        , m_AspectRatio(aspectRatio)
        , m_NearClip(nearClip)
        , m_FarClip(farClip)
    {
        UpdateView();
    }

    void EditorCamera::SetViewportSize(float width, float height)
    {
        m_ViewportWidth = width;
        m_ViewportHeight = height;
        UpdateProjection();
    }

    void EditorCamera::SetViewTransform(const glm::vec3& position,
        const glm::vec3& rotation,
        float orbitDistance)
    {
        m_Mode = Mode::Orbit;
        m_Distance = std::max(orbitDistance, 1.0f);
        m_Pitch = -rotation.x;
        m_Yaw = -rotation.y;
        m_Position = position;
        m_FocalPoint = m_Position + GetForwardDirection() * m_Distance;
        UpdateProjection();
        UpdateView();
    }

    void EditorCamera::Frame(const glm::vec3& focalPoint, float orbitDistance)
    {
        Frame(focalPoint, glm::vec3(0.0f), orbitDistance);
    }

    void EditorCamera::Frame(const glm::vec3& focalPoint,
        const glm::vec3& rotation,
        float orbitDistance)
    {
        m_Mode = Mode::Orbit;
        m_Pitch = -rotation.x;
        m_Yaw = -rotation.y;
        m_FocalPoint = focalPoint;
        m_Distance = std::max(orbitDistance, 1.0f);
        UpdateProjection();
        UpdateView();
    }

    void EditorCamera::UpdateProjection()
    {
        m_AspectRatio = m_ViewportWidth / m_ViewportHeight;
        m_Projection = glm::perspective(glm::radians(m_FOV),
            m_AspectRatio,
            m_NearClip, m_FarClip);
    }

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------

    void EditorCamera::UpdateView()
    {
        if (m_Mode == Mode::Orbit)
            m_Position = CalculateOrbitPosition();

        const glm::quat orientation = GetOrientation();
        m_ViewMatrix = glm::inverse(
            glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation)
        );
    }

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------

    void EditorCamera::OnUpdate(Timestep ts)
    {
        const bool rightMouseHeld = Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_RIGHT);

        if (rightMouseHeld && m_Mode == Mode::Orbit)
        {
            m_Mode = Mode::Fly;
            m_FocalPoint = m_Position + GetForwardDirection() * m_Distance;
        }
        else if (!rightMouseHeld && m_Mode == Mode::Fly)
        {
            m_Mode = Mode::Orbit;
            m_FocalPoint = m_Position + GetForwardDirection() * m_Distance;
        }

        if (m_Mode == Mode::Fly)
        {
            const glm::vec2 delta = { Input::GetMouseDeltaX(), Input::GetMouseDeltaY() };
            const glm::vec2 scaledDelta = delta * 0.003f;

            const float yawSign = (GetUpDirection().y < 0.0f) ? -1.0f : 1.0f;
            m_Yaw += yawSign * scaledDelta.x * RotationSpeed();
            m_Pitch += scaledDelta.y * RotationSpeed();

            const float pitchLimit = glm::radians(89.0f);
            m_Pitch = glm::clamp(m_Pitch, -pitchLimit, pitchLimit);

            const float speed = m_FlySpeed * (float)ts;
            const float fastMult = Input::IsKeyPressed(WT_KEY_LEFT_SHIFT) ? 3.0f : 1.0f;
            const float slowMult = Input::IsKeyPressed(WT_KEY_LEFT_CONTROL) ? 0.25f : 1.0f;
            const float finalSpeed = speed * fastMult * slowMult;

            if (Input::IsKeyPressed(WT_KEY_W)) m_Position += GetForwardDirection() * finalSpeed;
            if (Input::IsKeyPressed(WT_KEY_S)) m_Position -= GetForwardDirection() * finalSpeed;
            if (Input::IsKeyPressed(WT_KEY_A)) m_Position -= GetRightDirection() * finalSpeed;
            if (Input::IsKeyPressed(WT_KEY_D)) m_Position += GetRightDirection() * finalSpeed;
            if (Input::IsKeyPressed(WT_KEY_Q)) m_Position -= GetUpDirection() * finalSpeed;
            if (Input::IsKeyPressed(WT_KEY_E)) m_Position += GetUpDirection() * finalSpeed;
        }

        if (m_Mode == Mode::Orbit && Input::IsKeyPressed(WT_KEY_LEFT_ALT))
        {
            const glm::vec2 delta = { Input::GetMouseDeltaX(), Input::GetMouseDeltaY() };
            const glm::vec2 scaledDelta = delta * 0.003f;

            if (Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_MIDDLE)) MousePan(scaledDelta);
            else if (Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_LEFT))   MouseRotate(scaledDelta);
            else if (Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_RIGHT))  MouseZoom(scaledDelta.y);
        }

        UpdateView();
    }

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------

    void EditorCamera::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<MouseScrolledEvent>(WT_BIND_EVENT_FN(EditorCamera::OnMouseScroll));
    }

    bool EditorCamera::OnMouseScroll(MouseScrolledEvent& e)
    {
        if (m_Mode == Mode::Orbit)
        {
            MouseZoom(e.GetYOffset() * 0.1f);
            UpdateView();
        }
        else
        {
            m_FlySpeed = glm::max(0.5f, m_FlySpeed + e.GetYOffset() * 0.5f);
        }
        return false;
    }

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------

    void EditorCamera::MousePan(const glm::vec2& delta)
    {
        auto [xSpeed, ySpeed] = PanSpeed();
        m_FocalPoint -= GetRightDirection() * delta.x * xSpeed * m_Distance;
        m_FocalPoint += GetUpDirection() * delta.y * ySpeed * m_Distance;
    }

    void EditorCamera::MouseRotate(const glm::vec2& delta)
    {
        const float yawSign = (GetUpDirection().y < 0.0f) ? -1.0f : 1.0f;
        m_Yaw += yawSign * delta.x * RotationSpeed();
        m_Pitch += delta.y * RotationSpeed();
    }

    void EditorCamera::MouseZoom(float delta)
    {
        m_Distance -= delta * ZoomSpeed();
        if (m_Distance < 1.0f)
        {
            m_FocalPoint += GetForwardDirection();
            m_Distance = 1.0f;
        }
    }

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------

    std::pair<float, float> EditorCamera::PanSpeed() const
    {
        const float x = std::min(m_ViewportWidth / 1000.0f, 2.4f);
        const float y = std::min(m_ViewportHeight / 1000.0f, 2.4f);
        auto factor = [](float v) {
            return 0.0366f * (v * v) - 0.1778f * v + 0.3021f;
            };
        return { factor(x), factor(y) };
    }

    float EditorCamera::RotationSpeed() const
    {
        return 0.8f;
    }

    float EditorCamera::ZoomSpeed() const
    {
        const float distance = std::max(m_Distance * 0.2f, 0.0f);
        return std::min(distance * distance, 100.0f);
    }

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------

    glm::quat EditorCamera::GetOrientation() const
    {
        return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
    }

    glm::vec3 EditorCamera::GetUpDirection() const
    {
        return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::vec3 EditorCamera::GetRightDirection() const
    {
        return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
    }

    glm::vec3 EditorCamera::GetForwardDirection() const
    {
        return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
    }

    glm::vec3 EditorCamera::CalculateOrbitPosition() const
    {
        return m_FocalPoint - GetForwardDirection() * m_Distance;
    }

} // namespace Wheatear
