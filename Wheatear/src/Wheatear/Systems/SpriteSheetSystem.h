#pragma once

#include "Wheatear/Systems/ISystem.h"
#include "Wheatear/Assets/SpriteSheetAsset.h"
#include "Wheatear/Scene/Entity.h"

namespace Wheatear {

    // Resolves SpriteRendererComponent / UIImageComponent sprite-sheet
    // references (.wtsheet + CellIndex) into Texture + UVMin/UVMax every
    // frame. Sheet definitions are cached and hot-reloaded on file change, so
    // editing a sheet's grid or trims updates every entity using it live.
    // Entities with a FollowAnimation BoxCollider2D also get their collider
    // driven from the cell's per-frame collision box.
    class SpriteSheetSystem : public ISystem
    {
    public:
        void OnUpdateRuntime(Scene* scene, Timestep ts) override { ResolveSheets(scene); }
        void OnUpdateEditor(Scene* scene, Timestep ts) override { ResolveSheets(scene); }

        // Drives a FollowAnimation BoxCollider2D from a resolved sheet cell
        // (world units derived via the sprite's PPU). Shared by component
        // resolution and animation frame application.
        static void ApplyColliderToEntity(Entity entity, const SpriteSheetAsset::ResolvedCell& resolved);

    private:
        static void ResolveSheets(Scene* scene);
    };

} // namespace Wheatear
