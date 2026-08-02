#include "wtpch.h"
#include "UIRuntimeTools.h"

#include "UIRenderer.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/SceneQueries.h"

#include <algorithm>
#include <unordered_map>

namespace Wheatear::UIRuntimeTools {

    namespace {

        std::unordered_map<std::string, Ref<Texture2D>>& TextureCache()
        {
            static std::unordered_map<std::string, Ref<Texture2D>> cache;
            return cache;
        }

    } // namespace

    void SetWidgetVisible(Scene* scene, const std::string& entityName, bool visible)
    {
        Entity entity = SceneQueries::FindEntityByName(scene, entityName);
        if (entity && entity.HasComponent<UIWidgetComponent>())
            entity.GetComponent<UIWidgetComponent>().Visible = visible;
    }

    void SetWidgetTopLeft(Scene* scene,
        const std::string& entityName,
        const glm::vec2& position,
        const glm::vec2& size)
    {
        Entity entity = SceneQueries::FindEntityByName(scene, entityName);
        if (!entity || !entity.HasComponent<UIWidgetComponent>())
            return;

        auto& widget = entity.GetComponent<UIWidgetComponent>();
        widget.Anchor = UIAnchor::TopLeft;
        widget.Position = position;
        widget.Size = size;
    }

    void SetWidgetCenter(Scene* scene,
        const std::string& entityName,
        const glm::vec2& position,
        const glm::vec2& size,
        float rotation)
    {
        Entity entity = SceneQueries::FindEntityByName(scene, entityName);
        if (!entity || !entity.HasComponent<UIWidgetComponent>())
            return;

        auto& widget = entity.GetComponent<UIWidgetComponent>();
        widget.Anchor = UIAnchor::MiddleCenter;
        widget.Position = position;
        widget.Size = size;
        widget.Rotation = rotation;
    }

    void SetWidgetParent(Scene* scene, const std::string& entityName, const std::string& parentName)
    {
        Entity entity = SceneQueries::FindEntityByName(scene, entityName);
        if (!entity || !entity.HasComponent<UIWidgetComponent>())
            return;

        Entity parent = SceneQueries::FindEntityByName(scene, parentName);
        auto& widget = entity.GetComponent<UIWidgetComponent>();
        widget.ParentEntity = parent ? parent.GetUUID() : UUID(0);
    }

    void SetText(Scene* scene, const std::string& entityName, const std::string& text)
    {
        Entity entity = SceneQueries::FindEntityByName(scene, entityName);
        if (!entity || !entity.HasComponent<UITextComponent>())
            return;

        auto& uiText = entity.GetComponent<UITextComponent>();
        if (uiText.Text == text)
            return;

        uiText.Text = text;
        UIRenderer::PreloadUIText(uiText);
    }

    void SetProgress(Scene* scene, const std::string& entityName, float value, float maxValue)
    {
        Entity entity = SceneQueries::FindEntityByName(scene, entityName);
        if (!entity || !entity.HasComponent<UIProgressBarComponent>())
            return;

        auto& bar = entity.GetComponent<UIProgressBarComponent>();
        bar.MaxValue = std::max(0.01f, maxValue);
        bar.Value = std::clamp(value, 0.0f, bar.MaxValue);
    }

    void SetImageAlpha(Scene* scene, const std::string& entityName, float alpha)
    {
        Entity entity = SceneQueries::FindEntityByName(scene, entityName);
        if (!entity)
            return;

        alpha = std::clamp(alpha, 0.0f, 1.0f);
        if (entity.HasComponent<UIImageComponent>())
            entity.GetComponent<UIImageComponent>().Color.a = alpha;
        if (entity.HasComponent<UIWidgetComponent>())
            entity.GetComponent<UIWidgetComponent>().Visible = alpha > 0.01f;
    }

    void SetImageColor(Scene* scene, const std::string& entityName, const glm::vec4& color)
    {
        Entity entity = SceneQueries::FindEntityByName(scene, entityName);
        if (entity && entity.HasComponent<UIImageComponent>())
            entity.GetComponent<UIImageComponent>().Color = color;
    }

    void SetImageTexture(Scene* scene, const std::string& entityName, const std::string& texturePath, bool clearWhenEmpty)
    {
        Entity entity = SceneQueries::FindEntityByName(scene, entityName);
        if (!entity || !entity.HasComponent<UIImageComponent>())
            return;

        auto& image = entity.GetComponent<UIImageComponent>();
        if (texturePath.empty())
        {
            if (clearWhenEmpty)
                image.Texture = nullptr;
            return;
        }

        auto& cache = TextureCache();
        Ref<Texture2D> texture;
        if (auto it = cache.find(texturePath); it != cache.end())
        {
            texture = it->second;
        }
        else
        {
            texture = Texture2D::Create(texturePath);
            if (texture && texture->IsLoaded())
                cache[texturePath] = texture;
        }

        if (texture && texture->IsLoaded())
            image.Texture = texture;
    }

    bool IsButtonHovered(Scene* scene, const std::string& entityName)
    {
        Entity entity = SceneQueries::FindEntityByName(scene, entityName);
        return entity
            && entity.HasComponent<UIWidgetComponent>()
            && entity.GetComponent<UIWidgetComponent>().Visible
            && entity.HasComponent<UIButtonComponent>()
            && entity.GetComponent<UIButtonComponent>().IsHovered;
    }

} // namespace Wheatear::UIRuntimeTools
