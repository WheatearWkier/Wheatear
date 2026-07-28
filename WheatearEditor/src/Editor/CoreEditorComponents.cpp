#include "wtpch.h"
#include "CoreEditorComponents.h"

#include "Editor/EditorComponentRegistry.h"
#include "Panels/SceneHierarchy/ComponentDrawers.h"
#include "Wheatear/Scene/Components.h"

namespace Wheatear {

    namespace {

        template<typename T>
        void RegisterEditorComponent(const char* category,
            const char* label,
            void (*draw)(Entity),
            bool ensureUIWidget = false)
        {
            EditorComponentRegistry::Register({
                category,
                label,
                [](Entity entity)
                {
                    return entity && !entity.HasComponent<T>();
                },
                [ensureUIWidget](Entity entity)
                {
                    if (!entity)
                        return;

                    if (ensureUIWidget && !entity.HasComponent<UIWidgetComponent>())
                        entity.AddComponent<UIWidgetComponent>();

                    entity.AddComponent<T>();
                },
                [draw](Entity entity)
                {
                    draw(entity);
                }
            });
        }

        template<typename T>
        void RegisterReadOnlyEditorComponent(const char* category,
            const char* label,
            void (*draw)(Entity))
        {
            EditorComponentRegistry::Register({
                category,
                label,
                [](Entity) { return false; },
                [](Entity) {},
                [draw](Entity entity)
                {
                    draw(entity);
                }
            });
        }

    } // namespace

    void RegisterCoreEditorComponents()
    {
        static bool registered = false;
        if (registered)
            return;
        registered = true;

        RegisterReadOnlyEditorComponent<TransformComponent>("Core", "Transform", DrawTransformComponent);

        RegisterEditorComponent<CameraComponent>("Rendering", "Camera", DrawCameraComponent);
        RegisterEditorComponent<SpriteRendererComponent>("Rendering", "Sprite Renderer", DrawSpriteRendererComponent);
        RegisterEditorComponent<SpriteAnimatorComponent>("Rendering", "Sprite Animator", DrawSpriteAnimatorComponent);
        RegisterEditorComponent<CircleRendererComponent>("Rendering", "Circle Renderer", DrawCircleRendererComponent);
        RegisterEditorComponent<MeshRendererComponent>("Rendering", "Mesh Renderer", DrawMeshRendererComponent);

        RegisterEditorComponent<DirectionalLightComponent>("Lighting", "Directional Light", DrawDirectionalLightComponent);
        RegisterEditorComponent<PointLightComponent>("Lighting", "Point Light", DrawPointLightComponent);

        RegisterEditorComponent<Rigidbody2DComponent>("Physics", "Rigidbody 2D", DrawRigidbody2DComponent);
        RegisterEditorComponent<BoxCollider2DComponent>("Physics", "Box Collider 2D", DrawBoxCollider2DComponent);
        RegisterEditorComponent<CircleCollider2DComponent>("Physics", "Circle Collider 2D", DrawCircleCollider2DComponent);

        RegisterEditorComponent<ScriptComponent>("Scripting & Audio", "Script", DrawScriptComponent);
        RegisterEditorComponent<AudioSourceComponent>("Scripting & Audio", "Audio Source", DrawAudioSourceComponent);

        RegisterEditorComponent<UICanvasComponent>("UI", "UI Canvas", DrawUICanvasComponent);
        RegisterEditorComponent<UIWidgetComponent>("UI", "UI Widget", DrawUIWidgetComponent);
        RegisterEditorComponent<UIImageComponent>("UI", "UI Image", DrawUIImageComponent, true);
        RegisterEditorComponent<UIPanelComponent>("UI", "UI Panel", DrawUIPanelComponent, true);
        RegisterEditorComponent<UITextComponent>("UI", "UI Text", DrawUITextComponent, true);
        RegisterEditorComponent<UIButtonComponent>("UI", "UI Button", DrawUIButtonComponent, true);
        RegisterEditorComponent<UIProgressBarComponent>("UI", "UI Progress Bar", DrawUIProgressBarComponent, true);
        RegisterEditorComponent<UISliderComponent>("UI", "UI Slider", DrawUISliderComponent, true);
        RegisterEditorComponent<UICheckboxComponent>("UI", "UI Checkbox", DrawUICheckboxComponent, true);
    }

} // namespace Wheatear
