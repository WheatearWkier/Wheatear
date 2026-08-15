#pragma once

#include "ComponentGroup.h"
#include "Components.h"
#include "Wheatear/Modules/GameplayModuleComponents.h"

namespace Wheatear {

    using CoreCopyableSceneComponents = ComponentGroup
    <
        TransformComponent,
        SpriteRendererComponent,
        CircleRendererComponent,
        MeshRendererComponent,
        DirectionalLightComponent,
        PointLightComponent,
        CameraComponent,
        Rigidbody2DComponent,
        BoxCollider2DComponent,
        CircleCollider2DComponent
    >;

    using AnimationCopyableSceneComponents = ComponentGroup
    <
        SpriteAnimatorComponent
    >;

    using ScriptingCopyableSceneComponents = ComponentGroup
    <
        EventScriptComponent,
        AudioSourceComponent
    >;

    using UICopyableSceneComponents = ComponentGroup
    <
        UICanvasComponent,
        UIWidgetComponent,
        UIAnimatorComponent,
        UIImageComponent,
        UIRadialCooldownComponent,
        UIPanelComponent,
        UITextComponent,
        UIButtonComponent,
        UIProgressBarComponent,
        UISliderComponent,
        UIPagerComponent,
        UIScrollViewComponent,
        UIPathComponent,
        UISkillTreeViewComponent,
        UIPageItemComponent,
        UICheckboxComponent
    >;

    using AllCopyableSceneComponents = ComponentGroupConcat_t
    <
        CoreCopyableSceneComponents,
        AnimationCopyableSceneComponents,
        ScriptingCopyableSceneComponents,
        UICopyableSceneComponents,
        GameplayModuleSceneComponents
    >;

} // namespace Wheatear
