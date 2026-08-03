#pragma once
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/Components.h"

namespace Wheatear {

    class UIInputSystem
    {
    public:
        static void OnUpdate(Scene* scene,
            float mouseX, float mouseY,
            uint32_t viewportWidth,
            uint32_t viewportHeight);

        static void OnMousePressed(Scene* scene);
        static void OnMouseReleased(Scene* scene);
        static bool OnMouseScrolled(Scene* scene,
            float yOffset,
            float mouseX,
            float mouseY,
            uint32_t viewportWidth,
            uint32_t viewportHeight);
        static void Reset();

    private:
        static bool HitTest(const UIWidgetComponent& widget,
            float normMouseX, float normMouseY);

        static void FireOnClick(Scene* scene, entt::entity entity);

        static bool s_MouseWasPressed;
    };

} // namespace Wheatear
