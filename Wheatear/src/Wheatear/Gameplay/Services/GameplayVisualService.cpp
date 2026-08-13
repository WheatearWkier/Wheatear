#include "wtpch.h"
#include "GameplayVisualService.h"

#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Gameplay/Services/GameplayTextService.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace Wheatear::GameplayVisualService {

    Ref<Texture2D> LoadTextureCached(const std::string& texturePath)
    {
        if (texturePath.empty())
            return nullptr;

        const std::string resolvedPath = AssetAliasRegistry::Resolve(texturePath);
        if (resolvedPath.empty())
            return nullptr;

        static std::unordered_map<std::string, Ref<Texture2D>> textureCache;
        if (auto it = textureCache.find(resolvedPath); it != textureCache.end())
            return it->second;

        Ref<Texture2D> texture = Texture2D::Create(resolvedPath);
        if (!texture || !texture->IsLoaded())
            return nullptr;

        textureCache[resolvedPath] = texture;
        return texture;
    }

    int ResolveFrameIndex(float timer, float frameRate, int frameCount, bool loop)
    {
        const int safeFrameCount = std::max(1, frameCount);
        const float safeFrameRate = std::max(1.0f, frameRate);
        int frame = 1 + static_cast<int>(std::floor(std::max(0.0f, timer) * safeFrameRate));

        if (loop)
            return ((frame - 1) % safeFrameCount) + 1;

        return std::clamp(frame, 1, safeFrameCount);
    }

    bool ResolveAtlasFrame(
        const TextureAtlasFrameSpec& atlas,
        int oneBasedFrame,
        Ref<Texture2D>* texture,
        glm::vec2* uvMin,
        glm::vec2* uvMax)
    {
        if (!atlas.IsValid() || !texture || !uvMin || !uvMax)
            return false;

        Ref<Texture2D> sheet = LoadTextureCached(atlas.SheetPath);
        if (!sheet)
            return false;

        const int sheetWidth = static_cast<int>(sheet->GetWidth());
        const int sheetHeight = static_cast<int>(sheet->GetHeight());
        if (sheetWidth <= 0 || sheetHeight <= 0 ||
            atlas.CellWidth <= 0 || atlas.CellHeight <= 0)
        {
            return false;
        }

        const int inferredColumns = std::max(1, sheetWidth / atlas.CellWidth);
        const int columns = std::max(1, atlas.Columns > 0 ? atlas.Columns : inferredColumns);
        const int frameOffset = std::max(0, atlas.StartFrame) + std::max(0, oneBasedFrame - 1);
        const int column = frameOffset % columns;
        const int row = frameOffset / columns;

        const int x0 = column * atlas.CellWidth;
        const int y0 = row * atlas.CellHeight;
        const int x1 = x0 + atlas.CellWidth;
        const int y1 = y0 + atlas.CellHeight;
        if (x1 > sheetWidth || y1 > sheetHeight)
            return false;

        *texture = sheet;
        *uvMin = {
            static_cast<float>(x0) / static_cast<float>(sheetWidth),
            1.0f - static_cast<float>(y1) / static_cast<float>(sheetHeight)
        };
        *uvMax = {
            static_cast<float>(x1) / static_cast<float>(sheetWidth),
            1.0f - static_cast<float>(y0) / static_cast<float>(sheetHeight)
        };
        return true;
    }

    bool ApplySpriteAtlasFrame(SpriteRendererComponent& sprite,
        const TextureAtlasFrameSpec& atlas,
        int oneBasedFrame)
    {
        Ref<Texture2D> texture;
        glm::vec2 uvMin{ 0.0f };
        glm::vec2 uvMax{ 1.0f };
        if (!ResolveAtlasFrame(atlas, oneBasedFrame, &texture, &uvMin, &uvMax))
            return false;

        sprite.Texture = texture;
        sprite.UVMin = uvMin;
        sprite.UVMax = uvMax;
        return true;
    }

    bool ApplySpriteFrame(SpriteRendererComponent& sprite,
        const std::string& framePattern,
        int oneBasedFrame)
    {
        const std::string path = GameplayTextService::FormatFramePath(framePattern, oneBasedFrame);
        if (path.empty())
            return false;

        if (Ref<Texture2D> texture = LoadTextureCached(path))
        {
            sprite.Texture = texture;
            sprite.UVMin = { 0.0f, 0.0f };
            sprite.UVMax = { 1.0f, 1.0f };
            return true;
        }
        return false;
    }

    bool ApplyUIImageAtlasFrame(Scene* scene,
        const std::string& entityName,
        const TextureAtlasFrameSpec& atlas,
        int oneBasedFrame,
        bool clearWhenEmpty)
    {
        if (!scene || entityName.empty())
            return false;

        Entity entity = SceneQueries::FindEntityByName(scene, entityName);
        if (!entity || !entity.HasComponent<UIImageComponent>())
            return false;

        Ref<Texture2D> texture;
        glm::vec2 uvMin{ 0.0f };
        glm::vec2 uvMax{ 1.0f };
        if (!ResolveAtlasFrame(atlas, oneBasedFrame, &texture, &uvMin, &uvMax))
        {
            if (clearWhenEmpty && atlas.SheetPath.empty())
                entity.GetComponent<UIImageComponent>().Texture = nullptr;
            return false;
        }

        auto& image = entity.GetComponent<UIImageComponent>();
        image.Texture = texture;
        image.UVMin = uvMin;
        image.UVMax = uvMax;
        return true;
    }

    bool ApplyUIImageFrame(Scene* scene,
        const std::string& entityName,
        const std::string& framePattern,
        int oneBasedFrame,
        bool clearWhenEmpty)
    {
        const std::string path = GameplayTextService::FormatFramePath(framePattern, oneBasedFrame);
        if (path.empty())
            return false;

        UIRuntimeTools::SetImageTexture(scene, entityName, path, clearWhenEmpty);
        if (Entity entity = SceneQueries::FindEntityByName(scene, entityName))
        {
            if (entity.HasComponent<UIImageComponent>())
            {
                auto& image = entity.GetComponent<UIImageComponent>();
                image.UVMin = { 0.0f, 0.0f };
                image.UVMax = { 1.0f, 1.0f };
            }
        }
        return true;
    }

} // namespace Wheatear::GameplayVisualService
