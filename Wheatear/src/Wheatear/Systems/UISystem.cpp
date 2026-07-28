#include "wtpch.h"
#include "UISystem.h"

#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Core/Input.h"
#include "Wheatear/UI/UIRenderer.h"
#include "Wheatear/UI/UIInputSystem.h"

#include <glm/glm.hpp>

namespace Wheatear {

    void UISystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        UIInputSystem::OnUpdate(
            scene,
            Input::GetMouseX() - m_ViewportOffset.x,
            Input::GetMouseY() - m_ViewportOffset.y,
            scene->GetViewportWidth(),
            scene->GetViewportHeight());
    }

    void UISystem::OnUpdateEditor(Scene* scene, Timestep ts)
    {
        // 编辑模式下 UI 只渲染，不处理输入
        // 渲染由 RenderSystem 在合适时机调用 RenderUI()
    }

    void UISystem::RenderUI(Scene* scene)
    {
        auto& registry = scene->GetRegistry();

        // 收集并按 SortOrder 排序
        std::vector<std::pair<int, entt::entity>> entries;
        for (auto e : registry.view<UIWidgetComponent>())
            entries.emplace_back(registry.get<UIWidgetComponent>(e).SortOrder, e);

        std::sort(entries.begin(), entries.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        for (auto [order, e] : entries)
        {
            auto& widget = registry.get<UIWidgetComponent>(e);
            if (!widget.Visible) continue;
            int id = static_cast<int>(static_cast<uint32_t>(e));

            if (auto* panel = registry.try_get<UIPanelComponent>(e))
                UIRenderer::DrawUIPanel(widget, *panel, id);
            if (auto* pb = registry.try_get<UIProgressBarComponent>(e))
                UIRenderer::DrawUIProgressBar(widget, *pb, id);
            if (auto* btn = registry.try_get<UIButtonComponent>(e))
                UIRenderer::DrawUIButton(widget, *btn, id);
            if (auto* slider = registry.try_get<UISliderComponent>(e))
                UIRenderer::DrawUISlider(widget, *slider, id);
            if (auto* checkbox = registry.try_get<UICheckboxComponent>(e))
                UIRenderer::DrawUICheckbox(widget, *checkbox, id);
            if (auto* img = registry.try_get<UIImageComponent>(e))
                UIRenderer::DrawUIImage(widget, *img, id);
            if (auto* text = registry.try_get<UITextComponent>(e))
                UIRenderer::DrawUIText(widget, *text, id);
        }
    }

} // namespace Wheatear