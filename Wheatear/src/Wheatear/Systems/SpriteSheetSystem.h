#pragma once

#include "Wheatear/Systems/ISystem.h"

namespace Wheatear {

    // Resolves SpriteRendererComponent / UIImageComponent sprite-sheet
    // references (.wtsheet + CellIndex) into Texture + UVMin/UVMax every
    // frame. Sheet definitions are cached and hot-reloaded on file change, so
    // editing a sheet's grid updates every entity using it live.
    class SpriteSheetSystem : public ISystem
    {
    public:
        void OnUpdateRuntime(Scene* scene, Timestep ts) override { ResolveSheets(scene); }
        void OnUpdateEditor(Scene* scene, Timestep ts) override { ResolveSheets(scene); }

    private:
        static void ResolveSheets(Scene* scene);
    };

} // namespace Wheatear
