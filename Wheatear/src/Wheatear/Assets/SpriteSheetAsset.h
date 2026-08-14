#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Renderer/Texture.h"

#include <string>

namespace Wheatear {

    // Reusable sprite-sheet definition asset (.wtsheet): a texture plus its
    // grid layout. Components reference a sheet by path and a cell index;
    // the runtime resolves the cell to texture + UV, so redefining the grid
    // in one file updates every entity and animation that uses it.
    struct SpriteSheetData
    {
        std::string TexturePath;
        int Columns = 1;
        int Rows = 1;
    };

    namespace SpriteSheetAsset {

        // YAML layout:
        //   texture: assets/sprites/coin.png
        //   columns: 8
        //   rows: 8
        SpriteSheetData Load(const std::string& path);
        void Save(const std::string& path, const SpriteSheetData& data);

        int CellCount(const SpriteSheetData& data);
        bool IsValidCell(const SpriteSheetData& data, int cellIndex);

        // Cell index order is row-major (left-to-right, top-to-bottom).
        glm::vec2 CellUVMin(const SpriteSheetData& data, int cellIndex);
        glm::vec2 CellUVMax(const SpriteSheetData& data, int cellIndex);

        // Cached, hot-reloading cell resolution shared by every consumer
        // (SpriteSheetSystem components and animation frames). The sheet file
        // is re-read when its write time changes, so editing a grid updates
        // all entities and animations referencing it live.
        bool ResolveCell(const std::string& sheetPath, int cellIndex,
            Ref<Texture2D>& outTexture, glm::vec2& outUVMin, glm::vec2& outUVMax);

    } // namespace SpriteSheetAsset

} // namespace Wheatear
