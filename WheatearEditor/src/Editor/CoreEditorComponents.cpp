#include "wepch.h"
#include "CoreEditorComponents.h"

#include "Editor/EditorComponentRegistry.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorCommands.h"
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

        RegisterEditorComponent<CameraComponent>("Rendering", EditorLocale::Text("Camera", "相机"), DrawCameraComponent);
        RegisterEditorComponent<SpriteRendererComponent>("Rendering", EditorLocale::Text("Sprite Renderer", "精灵渲染器"), DrawSpriteRendererComponent);
        RegisterEditorComponent<SpriteAnimatorComponent>("Rendering", EditorLocale::Text("Sprite Animator", "精灵动画器"), DrawSpriteAnimatorComponent);
        RegisterEditorComponent<CircleRendererComponent>("Rendering", EditorLocale::Text("Circle Renderer", "圆形渲染器"), DrawCircleRendererComponent);
        RegisterEditorComponent<MeshRendererComponent>("Rendering", EditorLocale::Text("Mesh Renderer", "网格渲染器"), DrawMeshRendererComponent);

        RegisterEditorComponent<DirectionalLightComponent>("Lighting", EditorLocale::Text("Directional Light", "方向光"), DrawDirectionalLightComponent);
        RegisterEditorComponent<PointLightComponent>("Lighting", EditorLocale::Text("Point Light", "点光源"), DrawPointLightComponent);

        RegisterEditorComponent<Rigidbody2DComponent>("Physics", EditorLocale::Text("Rigidbody 2D", "刚体 2D"), DrawRigidbody2DComponent);
        RegisterEditorComponent<BoxCollider2DComponent>("Physics", EditorLocale::Text("Box Collider 2D", "盒形碰撞体 2D"), DrawBoxCollider2DComponent);
        RegisterEditorComponent<CircleCollider2DComponent>("Physics", EditorLocale::Text("Circle Collider 2D", "圆形碰撞体 2D"), DrawCircleCollider2DComponent);

        RegisterEditorComponent<ScriptComponent>("Scripting & Audio", EditorLocale::Text("Script", "脚本"), DrawScriptComponent);
        RegisterEditorComponent<EventScriptComponent>("Scripting & Audio", EditorLocale::Text("Event Script", "事件脚本"), DrawEventScriptComponent);
        RegisterEditorComponent<AudioSourceComponent>("Scripting & Audio", EditorLocale::Text("Audio Source", "音频源"), DrawAudioSourceComponent);

        RegisterEditorComponent<UICanvasComponent>("UI", EditorLocale::Text("UI Canvas", "UI 画布"), DrawUICanvasComponent);
        RegisterUIWidgetComponent();
        RegisterEditorComponent<UIAnimatorComponent>("UI", EditorLocale::Text("UI Animator", "UI 动画器"), DrawUIAnimatorComponent, true);
        RegisterEditorComponent<UIImageComponent>("UI", EditorLocale::Text("UI Image", "UI 图片"), DrawUIImageComponent, true);
        RegisterEditorComponent<UIRadialCooldownComponent>("UI", EditorLocale::Text("UI Radial Cooldown", "UI 径向冷却"), DrawUIRadialCooldownComponent, true);
        RegisterEditorComponent<UIPanelComponent>("UI", EditorLocale::Text("UI Panel", "UI 面板"), DrawUIPanelComponent, true);
        RegisterEditorComponent<UITextComponent>("UI", EditorLocale::Text("UI Text", "UI 文本"), DrawUITextComponent, true);
        RegisterEditorComponent<UIButtonComponent>("UI", EditorLocale::Text("UI Button", "UI 按钮"), DrawUIButtonComponent, true);
        RegisterEditorComponent<UIProgressBarComponent>("UI", EditorLocale::Text("UI Progress Bar", "UI 进度条"), DrawUIProgressBarComponent, true);
        RegisterEditorComponent<UISliderComponent>("UI", EditorLocale::Text("UI Slider", "UI 滑条"), DrawUISliderComponent, true);
        RegisterEditorComponent<UIPagerComponent>("UI", EditorLocale::Text("UI Pager", "UI 分页器"), DrawUIPagerComponent, true);
        RegisterEditorComponent<UIScrollViewComponent>("UI", EditorLocale::Text("UI Scroll View", "UI 滚动视图"), DrawUIScrollViewComponent, true);
        RegisterEditorComponent<UIPathComponent>("UI", EditorLocale::Text("UI Path", "UI 路径"), DrawUIPathComponent, true);
        RegisterEditorComponent<UISkillTreeViewComponent>("UI", EditorLocale::Text("UI Skill Tree View", "UI 技能树视图"), DrawUISkillTreeViewComponent, true);
        RegisterEditorComponent<UIPageItemComponent>("UI", EditorLocale::Text("UI Page Item", "UI 页面项"), DrawUIPageItemComponent, true);
        RegisterEditorComponent<UICheckboxComponent>("UI", EditorLocale::Text("UI Checkbox", "UI 复选框"), DrawUICheckboxComponent, true);
    }

} // namespace Wheatear
