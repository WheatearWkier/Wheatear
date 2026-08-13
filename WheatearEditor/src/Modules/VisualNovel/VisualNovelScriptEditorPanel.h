#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Wheatear {

    namespace VisualNovelEditorRequests {
        void RequestOpenScript(const std::string& sourcePath);
        bool ConsumeOpenScriptRequest(std::string& sourcePath);
    }

    class VisualNovelScriptEditorPanel
    {
    public:
        void Open(const std::string& sourcePath);
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
        If
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
            std::string Raw;
            std::string Name;
            std::string Value;
            std::string Text;
            std::string Extra;
            std::vector<ChoiceEntry> Choices;
        };

    private:
        void Load();
        void Save();
        void ParseText(const std::string& text);
        std::string SerializeRows() const;
        void DrawToolbar();
        void DrawTimeline();
        void DrawRowEditor(Row& row);
        void DrawLibraries();
        void DrawRawPreview();
        void AddRow(RowKind kind);

    private:
        bool m_Open = false;
        bool m_Loaded = false;
        bool m_Dirty = false;
        int m_SelectedRow = -1;
        std::string m_SourcePath = "assets/vn/vertical_slice_intro.vn";
        std::filesystem::path m_ResolvedPath;
        std::string m_Status;
        std::vector<Row> m_Rows;
        std::vector<std::string> m_BGMAssets;
    };

} // namespace Wheatear
