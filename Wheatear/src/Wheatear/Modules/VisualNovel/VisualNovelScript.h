#pragma once

#include "Wheatear/Core/Core.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear {

    enum class VisualNovelLineType
    {
        Dialogue = 0,
        Choice,
        Goto,
        End,
        // "Set" assigns a script-local variable (@set name value / @set name = expr);
        // "If" is a conditional jump (@if <cond> -> label / @if <cond> goto label).
        // Both are consumed by the runtime's NormalizeCurrentNode and never surface
        // as visible lines, so existing .vn files (which contain neither) are
        // unaffected.
        Set,
        If
    };

    struct VisualNovelCharacter
    {
        std::string Name;
        std::string DisplayName;
        std::string Style;
    };

    struct VisualNovelChoice
    {
        std::string Text;
        std::string TargetLabel;
        // Optional progression story flag; the choice only renders when this flag
        // is set (or always, when empty). Kept out of the line-text grammar via the
        // trailing "if flag <id>" token so existing scripts parse unchanged.
        std::string RequiredFlag;
        // Optional full VN condition expression (EvaluateVNExpression grammar);
        // evaluated when RequiredFlag is empty. Both gate visibility in
        // VisualNovelSystem::CollectVisibleChoiceIndices.
        std::string RequiredCondition;
    };

    struct VisualNovelLine
    {
        VisualNovelLineType Type = VisualNovelLineType::Dialogue;
        std::string Speaker;
        std::string Text;
        std::string Background;
        std::string Music;
        std::string MusicTitle;
        std::string TargetLabel;
        std::vector<std::string> VisibleCharacters;
        std::vector<VisualNovelChoice> Choices;
        std::unordered_map<std::string, std::string> CharacterExpressions;
        float CharactersPerSecond = -1.0f;

        // Set line: variable name / assigned value (a literal number or a
        // reference to another variable, e.g. "@set gold = 5" or "@set hp = maxhp").
        std::string VariableName;
        std::string VariableValue;

        // If line: condition string in VariableName (kept separate from Text so
        // the visible-text path never touches it); jump target in TargetLabel.
        std::string Condition;
    };

    class WHEATEAR_API VisualNovelScript
    {
    public:
        static VisualNovelScript FromFile(const std::filesystem::path& filepath);
        static VisualNovelScript FromString(const std::string& text);

        const std::vector<VisualNovelCharacter>& GetCharacters() const { return m_Characters; }
        const std::vector<VisualNovelLine>& GetLines() const { return m_Lines; }
        const std::string& GetBackground() const { return m_DefaultBackground; }
        const std::filesystem::path& GetSourcePath() const { return m_SourcePath; }
        size_t FindLabel(const std::string& label) const;
        bool IsEmpty() const { return m_Lines.empty(); }

    private:
        void Parse(const std::string& text);
        void UpsertCharacter(const std::string& name, const std::string& displayName, const std::string& style);

    private:
        std::filesystem::path m_SourcePath;
        std::string m_DefaultBackground = "studio";
        std::vector<VisualNovelCharacter> m_Characters;
        std::vector<VisualNovelLine> m_Lines;
        std::unordered_map<std::string, size_t> m_Labels;
    };

} // namespace Wheatear
