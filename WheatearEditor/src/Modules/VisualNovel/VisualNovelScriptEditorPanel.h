#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "TextEditor.h"

namespace Wheatear {

    namespace VisualNovelEditorRequests {
        void RequestOpenScript(const std::string& sourcePath, int sourceLine = 0);
        bool ConsumeOpenScriptRequest(std::string& sourcePath, int& sourceLine);
    }

    class VisualNovelScriptEditorPanel
    {
    public:
        void Open(const std::string& sourcePath, int sourceLine = 0);
        void OnImGuiRender();

    public:
enum class RowKind
    {
        Raw = 0,
        Label,
        Background,
        Music,
        Speed,
        Character,
        Show,
        Hide,
        Expression,
        Dialogue,
        Choice,
        Goto,
        End,
        Set,
        If,
        Sheet,
        Char
    };

    struct ChoiceEntry
    {
        std::string Text;
        std::string Target;
        std::string RequiredFlag;
        std::string RequiredCondition;
    };

        struct Row
        {
            RowKind Kind = RowKind::Raw;
            int SourceLine = 0;
            std::string Raw;
            std::string Name;
            std::string Value;
            std::string Text;
            std::string Extra;
            std::vector<ChoiceEntry> Choices;
        };

    private:
        void ParseText(const std::string& text);
        std::string SerializeRows() const;
        void DrawToolbar();
        void DrawTimeline();
        void DrawRowEditor(Row& row);
        void DrawLibraries();
        void DrawRawPreview();
        void AddRow(RowKind kind);

    private:
        enum class PendingAction
        {
            None,
            Reload,
            OpenPath,
            Close
        };

        struct RowSnapshot
        {
            std::vector<Row> Rows;
            int SelectedRow = -1;
        };

        void Load();
        bool Save();
        void Undo();
        void Redo();
        void PushUndo(const std::vector<Row>& rows, int selectedRow);
        void DrawUnsavedPopup();
        void ExecutePendingAction();

    private:
        bool m_Open = false;
        bool m_Loaded = false;
        bool m_Dirty = false;
        int m_SelectedRow = -1;
        std::string m_SourcePath = "assets/vn/intro.vn";
        std::filesystem::path m_ResolvedPath;
        std::string m_Status;
        std::vector<Row> m_Rows;
        std::vector<std::string> m_BGMAssets;
        std::vector<RowSnapshot> m_UndoStack;
        std::vector<RowSnapshot> m_RedoStack;
        std::string m_LastObservedText;
        std::string m_SavedText;
        PendingAction m_PendingAction = PendingAction::None;
        std::string m_PendingPath;
        bool m_SuppressUndoCapture = false;
        int m_PendingRevealLine = 0;
        bool m_ScrollToSelected = false;
        std::unique_ptr<TextEditor> m_RawEditor;
        std::string m_RawEditorText;
        bool m_RawEditorDirty = false;
    };

} // namespace Wheatear
