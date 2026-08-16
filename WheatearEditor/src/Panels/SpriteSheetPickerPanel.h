#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Assets/SpriteSheetAsset.h"
#include "Wheatear/Scene/Entity.h"

#include <glm/glm.hpp>

#include <string>
#include <utility>
#include <vector>

namespace Wheatear {

    class AnimationClip;
    class Texture2D;

    class SpriteSheetPickerPanel
    {
    public:
        enum class SequenceMode
        {
            Horizontal = 0,
            RowMajor,
            Vertical
        };

        void SetEntity(Entity entity);
        void OnImGuiRender();

        void OpenForEntity(Entity entity);
        void Open();

        // Opens the picker bound to a reusable .wtsheet asset (no entity
        // needed; Apply actions stay disabled until an entity is selected).
        void OpenSheet(const std::string& sheetPath);

        static void RequestOpen(Entity entity);

    private:
        void ConsumeOpenRequest();
        void SyncTextureFromEntity();
        void SetTexture(const Ref<Texture2D>& texture, const std::string& texturePath);

        void DrawTextureDropZone();
        void DrawTargetSummary();
        void DrawSheetTools();
        void DrawGridControls();
        void DrawTrimTools();
        void DrawNamedRects();
        void DrawPreview();
        void DrawSelectedSpritePreview();
        void DrawApplyActions();

        bool AcceptTextureDrop();
        glm::vec2 GetCellUVMin(int col, int row) const;
        glm::vec2 GetCellUVMax(int col, int row) const;
        // Pixel-accurate UVs for the trimmed content of a cell (falls back
        // to the full cell when no trim is set). Matches the runtime
        // SpriteSheetAsset::ResolveCell convention.
        glm::vec2 GetTrimmedUVMin(int col, int row) const;
        glm::vec2 GetTrimmedUVMax(int col, int row) const;
        const SpriteSheetData::CellTrim* GetTrim(int cellIndex) const;
        std::pair<int, int> GetSequenceCell(int frameIndex) const;
        bool IsCellValid(int col, int row) const;
        Ref<AnimationClip> GetOrCreateTargetClip();
        void ApplySequenceToClip(const Ref<AnimationClip>& clip);

        // Decodes the current texture (CPU) and computes each cell's opaque
        // content bounding box into m_Trims.
        void DetectContentBounds();

    private:
        Entity m_Entity;
        bool m_Open = false;

        bool m_ShowReplaceConfirm = false;
        std::string m_ReplaceConfirmClipName;
        int m_ReplaceConfirmFrameCount = 0;

        Ref<Texture2D> m_Texture;
        std::string m_TexturePath;

        int m_Cols = 8;
        int m_Rows = 8;
        int m_SelectedCol = 0;
        int m_SelectedRow = 0;
        int m_FrameCount = 4;
        int m_FrameStep = 1;
        float m_FrameDuration = 0.08f;
        bool m_RowOriginTop = true;
        bool m_AppendFrames = false;
        SequenceMode m_SequenceMode = SequenceMode::Horizontal;
        float m_Zoom = 1.0f;

        std::string m_LastAction;
        std::string m_SheetPath;   // reusable .wtsheet asset (empty = not saved yet)

        // Per-cell content trims/colliders (index = cell, row-major, top row
        // first), persisted into the .wtsheet. Saved with Save As Sheet.
        std::vector<SpriteSheetData::CellTrim> m_Trims;

        // Irregular-atlas named sub-rects (SpriteSheetData::Rects). Loaded and
        // saved together with the sheet so opening a sheet in this panel can
        // never silently drop them.
        std::vector<SpriteSheetData::NamedRect> m_Rects;
        int m_SelectedRect = -1;
        std::string m_NewRectName;
    };

} // namespace Wheatear
