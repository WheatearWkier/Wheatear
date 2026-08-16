#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Renderer/Texture.h"

#include <string>
#include <vector>

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

        // Optional per-cell content trim (pixel rect inside the cell,
        // 0 = full cell) and per-cell collision box. Cells without an entry
        // fall back to the full cell. Lets irregular art (un-anchored
        // animation frames, shapes that change size) render and collide
        // exactly as authored.
        struct CellTrim
        {
            int Left = 0;    // px from the cell's left edge
            int Top = 0;     // px from the cell's top edge
            int Width = 0;   // px; 0 = full cell width
            int Height = 0;  // px; 0 = full cell height

            bool HasCollider = false;
            int ColliderLeft = 0;   // px from the cell's left edge
            int ColliderTop = 0;    // px from the cell's top edge
            int ColliderWidth = 0;
            int ColliderHeight = 0;
        };
        std::vector<CellTrim> Trims;

        // Optional irregular sub-rects (Unity-style named sprites in an
        // atlas). Unlike the regular grid, each rect is an arbitrary pixel
        // rectangle with a name; components/animations can reference either a
        // grid cell (CellIndex) or a named rect (SubRect).
        struct NamedRect
        {
            std::string Name;
            int Left = 0;    // px from the texture's left edge
            int Top = 0;     // px from the texture's top edge
            int Width = 0;
            int Height = 0;

            bool HasCollider = false;
            int ColliderLeft = 0;   // px from the texture's left edge
            int ColliderTop = 0;    // px from the texture's top edge
            int ColliderWidth = 0;
            int ColliderHeight = 0;
        };
        std::vector<NamedRect> Rects;
    };

    namespace SpriteSheetAsset {

        // YAML layout:
        //   texture: assets/sprites/coin.png
        //   columns: 8
        //   rows: 8
        //   trims:                      (optional)
        //     - {left: 0, top: 0, width: 470, height: 312}
        //     - {left: 8, top: 4, width: 470, height: 312,
        //        collider: {left: 4, top: 4, width: 460, height: 300}}
        SpriteSheetData Load(const std::string& path);
        void Save(const std::string& path, const SpriteSheetData& data);

        // Hot-reloading cached view of a sheet (re-reads when the file's write
        // time changes, at most every ~500ms). Editor inspectors use this to
        // show named-rect dropdowns without per-frame file I/O. Returns
        // nullptr when the sheet cannot be loaded.
        const SpriteSheetData* GetCachedSheetData(const std::string& path);

        int CellCount(const SpriteSheetData& data);
        bool IsValidCell(const SpriteSheetData& data, int cellIndex);

        // Cell index order is row-major (left-to-right, top-to-bottom).
        // These return the FULL cell rect; trimmed cells resolve through
        // ResolveCell below.
        glm::vec2 CellUVMin(const SpriteSheetData& data, int cellIndex);
        glm::vec2 CellUVMax(const SpriteSheetData& data, int cellIndex);

        // Cached, hot-reloading cell resolution shared by every consumer
        // (SpriteSheetSystem components, animation frames, editor previews).
        // The sheet file is re-read when its write time changes, so editing
        // a grid or trims updates all entities and animations live.
        struct ResolvedCell
        {
            Ref<Texture2D> Texture;
            glm::vec2 UVMin = { 0.0f, 0.0f };
            glm::vec2 UVMax = { 1.0f, 1.0f };
            // Content box in texture pixels (trimmed region; full cell when
            // the sheet has no trim for this cell).
            int ContentX = 0;
            int ContentY = 0;
            int ContentWidth = 0;
            int ContentHeight = 0;
            // Optional per-cell collision box in texture pixels.
            bool HasCollider = false;
            int ColliderLeft = 0;
            int ColliderTop = 0;
            int ColliderWidth = 0;
            int ColliderHeight = 0;
        };
        bool ResolveCell(const std::string& sheetPath, int cellIndex, ResolvedCell& out);
        // Named-rect variant for irregular atlases (see SpriteSheetData::Rects).
        bool ResolveCell(const std::string& sheetPath, const std::string& rectName, ResolvedCell& out);

    } // namespace SpriteSheetAsset

} // namespace Wheatear
