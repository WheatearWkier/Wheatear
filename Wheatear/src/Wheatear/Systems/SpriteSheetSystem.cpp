#include "wtpch.h"
#include "SpriteSheetSystem.h"

#include "Wheatear/Assets/SpriteSheetAsset.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

namespace Wheatear {

    namespace {

        template<typename T>
        void ResolveComponent(Scene* scene)
        {
            auto& registry = scene->GetRegistry();
            for (auto entity : registry.view<T>())
            {
                T& component = registry.get<T>(entity);
                if (component.SpriteSheet.empty() || component.CellIndex < 0)
                    continue;

                // Shared cached resolution (hot-reloads on sheet file change).
                SpriteSheetAsset::ResolveCell(component.SpriteSheet, component.CellIndex,
                    component.Texture, component.UVMin, component.UVMax);
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
