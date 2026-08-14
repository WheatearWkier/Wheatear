#include "wtpch.h"
#include "SpriteSheetSystem.h"

#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Assets/SpriteSheetAsset.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace Wheatear {

    namespace {

        struct CachedSheet
        {
            SpriteSheetData Data;
            Ref<Texture2D> Texture;
            std::filesystem::file_time_type WriteTime{};
            bool Loaded = false;
        };

        std::unordered_map<std::string, CachedSheet>& SheetCache()
        {
            static std::unordered_map<std::string, CachedSheet> cache;
            return cache;
        }

        const CachedSheet* GetCachedSheet(const std::string& sheetPath)
        {
            if (sheetPath.empty())
                return nullptr;

            // Runtime data resolution handles both loose editor files and the
            // packaged content.wtpack (same path convention as .vn/.wts).
            const std::filesystem::path resolved = AssetPath::ResolveRuntimeData(sheetPath);
            std::error_code error;
            const auto writeTime = std::filesystem::exists(resolved, error)
                ? std::filesystem::last_write_time(resolved, error)
                : std::filesystem::file_time_type{};

            CachedSheet& cached = SheetCache()[sheetPath];
            if (cached.Loaded && cached.WriteTime == writeTime)
                return &cached;

            cached.Data = SpriteSheetAsset::Load(resolved.generic_string());
            cached.Texture = cached.Data.TexturePath.empty()
                ? nullptr : Texture2D::Create(cached.Data.TexturePath);
            cached.WriteTime = writeTime;
            cached.Loaded = true;
            return &cached;
        }

        template<typename T>
        void ResolveComponent(Scene* scene)
        {
            auto& registry = scene->GetRegistry();
            for (auto entity : registry.view<T>())
            {
                T& component = registry.get<T>(entity);
                if (component.SpriteSheet.empty() || component.CellIndex < 0)
                    continue;

                const CachedSheet* sheet = GetCachedSheet(component.SpriteSheet);
                if (!sheet || !sheet->Texture || !sheet->Loaded)
                    continue;

                if (!SpriteSheetAsset::IsValidCell(sheet->Data, component.CellIndex))
                    continue;

                component.Texture = sheet->Texture;
                component.UVMin = SpriteSheetAsset::CellUVMin(sheet->Data, component.CellIndex);
                component.UVMax = SpriteSheetAsset::CellUVMax(sheet->Data, component.CellIndex);
            }
        }

    } // namespace

    void SpriteSheetSystem::ResolveSheets(Scene* scene)
    {
        if (!scene)
            return;

        ResolveComponent<SpriteRendererComponent>(scene);
        ResolveComponent<UIImageComponent>(scene);
    }

} // namespace Wheatear
