#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

#include <glm/glm.hpp>

#include <string>
#include <utility>

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
        void DrawPreview();
        void DrawSelectedSpritePreview();
        void DrawApplyActions();

        bool AcceptTextureDrop();
        glm::vec2 GetCellUVMin(int col, int row) const;
        glm::vec2 GetCellUVMax(int col, int row) const;
        std::pair<int, int> GetSequenceCell(int frameIndex) const;
        bool IsCellValid(int col, int row) const;
        Ref<AnimationClip> GetOrCreateTargetClip();
        void ApplySequenceToClip(const Ref<AnimationClip>& clip);

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
    };

} // namespace Wheatear
