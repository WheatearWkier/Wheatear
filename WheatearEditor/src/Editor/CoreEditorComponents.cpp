#include "wtpch.h"
#include "CoreEditorComponents.h"

#include "Editor/EditorComponentRegistry.h"
#include "Panels/EditorCommands.h"
#include "Panels/SceneHierarchy/ComponentDrawers.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/UI/UIWidgetLayout.h"

#include <unordered_set>

namespace Wheatear {

    namespace {

        static Entity FindOwningCanvas(Entity entity)
        {
            if (!entity || !entity.GetScene())
                return {};
            if (entity.HasComponent<UICanvasComponent>())
                return entity;
            if (!entity.HasComponent<UIWidgetComponent>())
                return {};

            UIWidgetLayout::Context layout(entity.GetScene());
            auto& registry = entity.GetScene()->GetRegistry();
            std::unordered_set<uint32_t> visited;
            Entity current = entity;
            while (current && current.HasComponent<UIWidgetComponent>())
            {
                const uint32_t currentKey = static_cast<uint32_t>(static_cast<entt::entity>(current));
                if (!visited.insert(currentKey).second)
                    return {};

                const auto& widget = current.GetComponent<UIWidgetComponent>();
                const entt::entity parentID = layout.ResolveReference(widget.ParentEntity);
                if (parentID == entt::null || !registry.valid(parentID))
                    return {};

                Entity parent{ parentID, entity.GetScene() };
                if (parent.HasComponent<UICanvasComponent>())
                    return parent;

                current = parent;
            }

            return {};
        }

        static bool IsCanvasOrCanvasChild(Entity entity)
        {
            return static_cast<bool>(FindOwningCanvas(entity));
        }

        template<typename T>
        void RegisterEditorComponent(const char* category,
            const char* label,
            void (*draw)(Entity),
            bool ensureUIWidget = false)
        {
            EditorComponentRegistry::Register({
                category,
                label,
                [ensureUIWidget](Entity entity)
                {
                    if (!entity || entity.HasComponent<T>())
                        return false;
                    if (ensureUIWidget && !IsCanvasOrCanvasChild(entity))
                        return false;
                    return true;
                },
                [ensureUIWidget](Entity entity)
                {
                    if (!entity)
                        return;

                    if (ensureUIWidget && !entity.HasComponent<UIWidgetComponent>())
                    {
                        auto widgetCommand = std::make_unique<AddComponentCommand<UIWidgetComponent>>(entity);
                        widgetCommand->Execute();
                        CommandHistory::Get().Push(std::move(widgetCommand));
                    }

                    auto command = std::make_unique<AddComponentCommand<T>>(entity);
                    command->Execute();
                    CommandHistory::Get().Push(std::move(command));
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

        static void RegisterUIWidgetComponent()
        {
            EditorComponentRegistry::Register({
                "UI",
                "UI Widget",
                [](Entity entity)
                {
                    return entity
                        && entity.HasComponent<UICanvasComponent>()
                        && !entity.HasComponent<UIWidgetComponent>();
                },
                [](Entity entity)
                {
                    if (!entity || !entity.HasComponent<UICanvasComponent>())
                        return;

                    auto command = std::make_unique<AddComponentCommand<UIWidgetComponent>>(entity);
                    command->Execute();
                    CommandHistory::Get().Push(std::move(command));
                },
                [](Entity entity)
                {
                    DrawUIWidgetComponent(entity);
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
        RegisterEditorComponent<EventScriptComponent>("Scripting & Audio", "Event Script", DrawEventScriptComponent);
        RegisterEditorComponent<AudioSourceComponent>("Scripting & Audio", "Audio Source", DrawAudioSourceComponent);

        RegisterEditorComponent<UICanvasComponent>("UI", "UI Canvas", DrawUICanvasComponent);
        RegisterUIWidgetComponent();
        RegisterEditorComponent<UIAnimatorComponent>("UI", "UI Animator", DrawUIAnimatorComponent, true);
        RegisterEditorComponent<UIImageComponent>("UI", "UI Image", DrawUIImageComponent, true);
        RegisterEditorComponent<UIRadialCooldownComponent>("UI", "UI Radial Cooldown", DrawUIRadialCooldownComponent, true);
        RegisterEditorComponent<UIPanelComponent>("UI", "UI Panel", DrawUIPanelComponent, true);
        RegisterEditorComponent<UITextComponent>("UI", "UI Text", DrawUITextComponent, true);
        RegisterEditorComponent<UIButtonComponent>("UI", "UI Button", DrawUIButtonComponent, true);
        RegisterEditorComponent<UIProgressBarComponent>("UI", "UI Progress Bar", DrawUIProgressBarComponent, true);
        RegisterEditorComponent<UISliderComponent>("UI", "UI Slider", DrawUISliderComponent, true);
        RegisterEditorComponent<UIPagerComponent>("UI", "UI Pager", DrawUIPagerComponent, true);
        RegisterEditorComponent<UIScrollViewComponent>("UI", "UI Scroll View", DrawUIScrollViewComponent, true);
        RegisterEditorComponent<UIPathComponent>("UI", "UI Path", DrawUIPathComponent, true);
        RegisterEditorComponent<UISkillTreeViewComponent>("UI", "UI Skill Tree View", DrawUISkillTreeViewComponent, true);
        RegisterEditorComponent<UIPageItemComponent>("UI", "UI Page Item", DrawUIPageItemComponent, true);
        RegisterEditorComponent<UICheckboxComponent>("UI", "UI Checkbox", DrawUICheckboxComponent, true);
    }

} // namespace Wheatear
