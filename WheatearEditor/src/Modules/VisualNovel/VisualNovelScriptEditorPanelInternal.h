#pragma once

// Shared file-internal helpers for the VN script editor panel, extracted from
// VisualNovelScriptEditorPanel.cpp so per-view translation units can be split.

#include "Editor/EditorContentPickers.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Modules/VisualNovel/VisualNovelScript.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear::VisualNovelScriptEditorPanelInternal {


        static bool s_HasPendingOpen = false;
        static std::string s_PendingOpenPath;

        inline static std::string Trim(const std::string& value)
        {
            const char* whitespace = " \t\r\n";
            const size_t begin = value.find_first_not_of(whitespace);
            if (begin == std::string::npos)
                return {};
            const size_t end = value.find_last_not_of(whitespace);
            return value.substr(begin, end - begin + 1);
        }

        inline static bool StartsWith(const std::string& value, const std::string& prefix)
        {
            return value.rfind(prefix, 0) == 0;
        }

        inline static std::string StripQuotes(const std::string& value)
        {
            std::string result = Trim(value);
            if (result.size() >= 2)
            {
                const char first = result.front();
                const char last = result.back();
                if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
                    return result.substr(1, result.size() - 2);
            }
            return result;
        }

        inline static std::string EscapeInlineText(const std::string& value)
        {
            std::string escaped;
            escaped.reserve(value.size());
            for (char c : value)
            {
                if (c == '\\')
                {
                    escaped += "\\\\";
                }
                else if (c == '\n')
                {
                    escaped += "\\n";
                }
                else if (c == '\r')
                {
                    continue;
                }
                else
                {
                    escaped.push_back(c);
                }
            }
            return escaped;
        }

        inline static std::string UnescapeInlineText(const std::string& value)
        {
            std::string result;
            result.reserve(value.size());
            for (size_t i = 0; i < value.size(); ++i)
            {
                if (value[i] == '\\' && i + 1 < value.size())
                {
                    if (value[i + 1] == 'n')
                    {
                        result.push_back('\n');
                        ++i;
                        continue;
                    }
                    if (value[i + 1] == '\\')
                    {
                        result.push_back('\\');
                        ++i;
                        continue;
                    }
                }
                result.push_back(value[i]);
            }
            return result;
        }

        inline static std::string QuoteIfNeeded(const std::string& value)
        {
            if (value.empty())
                return "\"\"";

            const bool needsQuote = value.find_first_of(" \t\r\n|") != std::string::npos;
            if (!needsQuote)
                return EscapeInlineText(value);

            std::string escaped;
            escaped.reserve(value.size() + 2);
            escaped.push_back('"');
            for (char c : value)
            {
                if (c == '"' || c == '\\')
                {
                    escaped.push_back('\\');
                    escaped.push_back(c);
                }
                else if (c == '\n')
                {
                    escaped += "\\n";
                }
                else if (c == '\r')
                {
                    continue;
                }
                else
                {
                    escaped.push_back(c);
                }
            }
            escaped.push_back('"');
            return escaped;
        }

        inline static std::string ConsumeToken(std::string& text)
        {
            text = Trim(text);
            if (text.empty())
                return {};

            const char quote = text.front();
            if (quote == '"' || quote == '\'')
            {
                const size_t end = text.find(quote, 1);
                if (end == std::string::npos)
                {
                    std::string token = text.substr(1);
                    text.clear();
                    return token;
                }

                std::string token = text.substr(1, end - 1);
                text = Trim(text.substr(end + 1));
                return token;
            }

            const size_t end = text.find_first_of(" \t\r\n");
            if (end == std::string::npos)
            {
                std::string token = text;
                text.clear();
                return token;
            }

            std::string token = text.substr(0, end);
            text = Trim(text.substr(end + 1));
            return token;
        }

        inline static bool ReadCommandPayload(const std::string& line,
            std::initializer_list<const char*> commands,
            std::string& payload)
        {
            for (const char* command : commands)
            {
                const std::string tagged = std::string("@") + command;
                if (line == tagged)
                {
                    payload.clear();
                    return true;
                }

                if (StartsWith(line, tagged + " "))
                {
                    payload = Trim(line.substr(tagged.size() + 1));
                    return true;
                }
            }
            return false;
        }

        using EditorWidgets::InputString;

        inline static bool InputMultiline(const char* label,
            std::string& value,
            const ImVec2& size,
            size_t capacity = 4096)
        {
            return EditorWidgets::InputMultilineString(label,
                value,
                size,
                capacity,
                ImGuiInputTextFlags_AllowTabInput);
        }

        // Mirrors the runtime grammar: the trailing standalone " if ..." clause
        // gates this option, e.g. "-> ending if gold >= 5" or "-> ending if flag CH02".
static void ExtractChoiceRequiredCondition(std::string& target,
            std::string& outFlag,
            std::string& outCondition)
        {
            target = Trim(target);
            if (target.empty())
                return;

            std::vector<std::string> words;
            {
                std::istringstream w(target);
                std::string w0;
                while (w >> w0)
                    words.push_back(w0);
            }
            if (words.size() < 3)
                return;

            size_t ifIndex = std::string::npos;
            for (size_t i = words.size() - 1; i > 0; --i)
            {
                if (words[i - 1] == "if")
                {
                    ifIndex = i - 1;
                    break;
                }
            }
            if (ifIndex == std::string::npos || ifIndex + 1 >= words.size())
                return;

            std::string label;
            for (size_t i = 0; i < ifIndex; ++i)
            {
                if (!label.empty())
                    label += " ";
                label += words[i];
            }

            std::string clause;
            for (size_t i = ifIndex + 1; i < words.size(); ++i)
            {
                if (!clause.empty())
                    clause += " ";
                clause += words[i];
            }

            if (words.size() >= 3 && words[words.size() - 3] == "flag")
            {
                std::string flagId = words.back();
                if (flagId.rfind("flag:", 0) == 0 && flagId.size() > 5)
                    flagId = flagId.substr(5);
                outFlag = Trim(flagId);
            }
            else if (clause.rfind("flag:", 0) == 0)
            {
                outFlag = Trim(clause.substr(5));
            }
            else
            {
                outCondition = Trim(clause);
            }

            target = Trim(label);
        }

        inline static bool IsChoiceQuoteOpener(const std::string& text,
            size_t index, char c)
        {
            if (c == '"')
                return true;
            if (c != '\'')
                return false;
            if (index == 0)
                return true;
            const char previous = text[index - 1];
            return std::isspace(static_cast<unsigned char>(previous))
                || previous == '|' || previous == '>' || previous == '('
                || previous == '[' || previous == '{' || previous == '='
                || previous == ':' || previous == ',';
        }

        inline static std::vector<std::string> SplitChoiceSegments(
            const std::string& text)
        {
            std::vector<std::string> parts;
            std::string current;
            char quote = '\0';
            for (size_t i = 0; i < text.size(); ++i)
            {
                const char c = text[i];
                if (quote != '\0')
                {
                    current.push_back(c);
                    if (c == '\\' && i + 1 < text.size())
                        current.push_back(text[++i]);
                    else if (c == quote)
                        quote = '\0';
                    continue;
                }
                if (IsChoiceQuoteOpener(text, i, c))
                {
                    quote = c;
                    current.push_back(c);
                    continue;
                }
                if (c == '|')
                {
                    parts.push_back(Trim(current));
                    current.clear();
                    continue;
                }
                current.push_back(c);
            }
            parts.push_back(Trim(current));
            return parts;
        }

        inline static size_t FindChoiceArrow(const std::string& text)
        {
            char quote = '\0';
            for (size_t i = 0; i < text.size(); ++i)
            {
                const char c = text[i];
                if (quote != '\0')
                {
                    if (c == '\\')
                    {
                        ++i;
                        continue;
                    }
                    if (c == quote)
                        quote = '\0';
                    continue;
                }
                if (IsChoiceQuoteOpener(text, i, c))
                {
                    quote = c;
                    continue;
                }
                if (text.compare(i, 2, "->") == 0)
                    return i;
            }
            return std::string::npos;
        }

        inline static size_t FindChoiceSeparator(const std::string& text)
        {
            char quote = '\0';
            for (size_t i = 0; i < text.size(); ++i)
            {
                const char c = text[i];
                if (quote != '\0')
                {
                    if (c == '\\')
                        ++i;
                    else if (c == quote)
                        quote = '\0';
                    continue;
                }
                if (IsChoiceQuoteOpener(text, i, c))
                {
                    quote = c;
                    continue;
                }
                if (c == '|')
                    return i;
            }
            return std::string::npos;
        }

        inline static std::vector<VisualNovelScriptEditorPanel::ChoiceEntry> ParseChoices(const std::string& payload)
        {
            std::vector<VisualNovelScriptEditorPanel::ChoiceEntry> choices;
            for (std::string segment : SplitChoiceSegments(payload))
            {
                segment = Trim(segment);
                if (segment.empty())
                    continue;

                const size_t arrow = FindChoiceArrow(segment);
                VisualNovelScriptEditorPanel::ChoiceEntry choice;
                if (arrow == std::string::npos)
                {
                    choice.Text = UnescapeInlineText(StripQuotes(segment));
                }
                else
                {
                    choice.Text = UnescapeInlineText(StripQuotes(segment.substr(0, arrow)));
                    choice.Target = StripQuotes(segment.substr(arrow + 2));
                    ExtractChoiceRequiredCondition(choice.Target, choice.RequiredFlag, choice.RequiredCondition);
                }
                choices.push_back(std::move(choice));
            }
            return choices;
        }

        inline static const char* KindName(VisualNovelScriptEditorPanel::RowKind kind)
        {
            switch (kind)
            {
                case VisualNovelScriptEditorPanel::RowKind::Raw: return "Raw";
                case VisualNovelScriptEditorPanel::RowKind::Label: return "Label";
                case VisualNovelScriptEditorPanel::RowKind::Background: return "Background";
                case VisualNovelScriptEditorPanel::RowKind::Music: return "BGM";
                case VisualNovelScriptEditorPanel::RowKind::Speed: return "Speed";
                case VisualNovelScriptEditorPanel::RowKind::Character: return "Character";
                case VisualNovelScriptEditorPanel::RowKind::Show: return "Show";
                case VisualNovelScriptEditorPanel::RowKind::Hide: return "Hide";
                case VisualNovelScriptEditorPanel::RowKind::Expression: return "Expression";
                case VisualNovelScriptEditorPanel::RowKind::Dialogue: return "Dialogue";
                case VisualNovelScriptEditorPanel::RowKind::Choice: return "Choice";
                case VisualNovelScriptEditorPanel::RowKind::Goto: return "Goto";
                case VisualNovelScriptEditorPanel::RowKind::End: return "End";
                case VisualNovelScriptEditorPanel::RowKind::Set: return "Set";
                case VisualNovelScriptEditorPanel::RowKind::If: return "If";
                case VisualNovelScriptEditorPanel::RowKind::Sheet: return "Sheet";
                case VisualNovelScriptEditorPanel::RowKind::Char: return "Char";
            }
            return "Unknown";
        }

        inline static std::string RowSummary(const VisualNovelScriptEditorPanel::Row& row)
        {
            switch (row.Kind)
            {
                case VisualNovelScriptEditorPanel::RowKind::Raw: return row.Raw.empty() ? "(blank)" : row.Raw;
                case VisualNovelScriptEditorPanel::RowKind::Label: return "label " + row.Name;
                case VisualNovelScriptEditorPanel::RowKind::Background: return row.Value;
                case VisualNovelScriptEditorPanel::RowKind::Music: return row.Text.empty() ? row.Value : row.Text;
                case VisualNovelScriptEditorPanel::RowKind::Speed: return row.Value;
                case VisualNovelScriptEditorPanel::RowKind::Character: return row.Name + " / " + row.Text;
                case VisualNovelScriptEditorPanel::RowKind::Show: return row.Value;
                case VisualNovelScriptEditorPanel::RowKind::Hide: return row.Value;
                case VisualNovelScriptEditorPanel::RowKind::Expression: return row.Name + " -> " + row.Value;
                case VisualNovelScriptEditorPanel::RowKind::Dialogue: return row.Name + ": " + row.Text;
                case VisualNovelScriptEditorPanel::RowKind::Choice: return std::to_string(row.Choices.size()) + " choices";
                case VisualNovelScriptEditorPanel::RowKind::Goto: return row.Value;
                case VisualNovelScriptEditorPanel::RowKind::End: return "end";
                case VisualNovelScriptEditorPanel::RowKind::Set: return row.Name + " = " + row.Value;
                case VisualNovelScriptEditorPanel::RowKind::If: return row.Value + " -> " + row.Text;
                case VisualNovelScriptEditorPanel::RowKind::Sheet: return row.Value;
                case VisualNovelScriptEditorPanel::RowKind::Char: return row.Name + " (" + row.Value + "," + row.Text + ")";
            }
            return {};
        }

        inline static void SortUnique(std::vector<std::string>& values)
        {
            values.erase(std::remove_if(values.begin(), values.end(), [](const std::string& value)
            {
                return Trim(value).empty();
            }), values.end());
            std::sort(values.begin(), values.end());
            values.erase(std::unique(values.begin(), values.end()), values.end());
        }

        inline static std::vector<std::string> CollectLabels(const std::vector<VisualNovelScriptEditorPanel::Row>& rows)
        {
            std::vector<std::string> labels;
            for (const VisualNovelScriptEditorPanel::Row& row : rows)
            {
                if (row.Kind == VisualNovelScriptEditorPanel::RowKind::Label)
                    labels.push_back(row.Name);
            }
            SortUnique(labels);
            return labels;
        }

        // Returns human-readable descriptions of jump targets that do not match
        // any @label row (goto / if targets / choice targets). Playback warns at
        // runtime; surfacing it here lets designers catch typos before playing.
        inline static std::vector<std::string> ValidateLabelTargets(
            const std::vector<VisualNovelScriptEditorPanel::Row>& rows)
        {
            const std::vector<std::string> labels = CollectLabels(rows);
            std::vector<std::string> issues;

            auto isDangling = [&labels](const std::string& target)
            {
                if (target.empty())
                    return false;
                // External commands (scene:/event:/newgame:/loadgame:) are not
                // label jumps and must not be flagged.
                if (StartsWith(target, "scene:")
                    || StartsWith(target, "event:")
                    || StartsWith(target, "newgame:")
                    || StartsWith(target, "loadgame:"))
                    return false;
                return std::find(labels.begin(), labels.end(), target) == labels.end();
            };

            for (const VisualNovelScriptEditorPanel::Row& row : rows)
            {
                switch (row.Kind)
                {
                case VisualNovelScriptEditorPanel::RowKind::Goto:
                    if (isDangling(row.Value))
                        issues.push_back("Row: @goto target '" + row.Value + "' has no matching @label.");
                    break;
                case VisualNovelScriptEditorPanel::RowKind::If:
                    if (isDangling(row.Text))
                        issues.push_back("Row: @if jump target '" + row.Text + "' has no matching @label.");
                    break;
                case VisualNovelScriptEditorPanel::RowKind::Choice:
                    for (const auto& choice : row.Choices)
                    {
                        if (isDangling(choice.Target))
                            issues.push_back("Row: choice '" + choice.Text + "' target '" + choice.Target + "' has no matching @label.");
                    }
                    break;
                default:
                    break;
                }
            }
            return issues;
        }

        inline static std::vector<std::string> CollectCharacterIds(const std::vector<VisualNovelScriptEditorPanel::Row>& rows)
        {
            std::vector<std::string> ids;
            for (const VisualNovelScriptEditorPanel::Row& row : rows)
            {
                if (row.Kind == VisualNovelScriptEditorPanel::RowKind::Character)
                    ids.push_back(row.Name);
                else if (row.Kind == VisualNovelScriptEditorPanel::RowKind::Dialogue)
                    ids.push_back(row.Name);
                else if (row.Kind == VisualNovelScriptEditorPanel::RowKind::Expression)
                    ids.push_back(row.Name);
            }
            SortUnique(ids);
            return ids;
        }

        inline static std::vector<std::string> CollectExpressionIds(const std::vector<VisualNovelScriptEditorPanel::Row>& rows,
            const std::string& character)
        {
            std::vector<std::string> ids;
            for (const VisualNovelScriptEditorPanel::Row& row : rows)
            {
                if (row.Kind != VisualNovelScriptEditorPanel::RowKind::Expression)
                    continue;
                if (!character.empty() && row.Name != character)
                    continue;
                ids.push_back(row.Value);
            }
            SortUnique(ids);
            return ids;
        }

        inline std::vector<std::string>& PortraitStyleChoicesCache()
        {
            static std::vector<std::string> cache;
            return cache;
        }

        inline void RefreshPortraitStyleChoices()
        {
            auto& choices = PortraitStyleChoicesCache();
            choices = { AssetAliasRegistry::Path("vn.default.portrait_pattern") };
            const std::filesystem::path portraitRoot = AssetPath::Resolve("assets/sandbox/vn/portraits");
            if (std::filesystem::is_directory(portraitRoot))
            {
                std::error_code error;
                for (const auto& entry : std::filesystem::recursive_directory_iterator(portraitRoot, error))
                {
                    if (error || !entry.is_regular_file())
                        continue;

                    const std::string extension = entry.path().extension().generic_string();
                    if (extension != ".png" && extension != ".jpg" && extension != ".jpeg")
                        continue;

                    std::filesystem::path relative = std::filesystem::relative(entry.path(), AssetPath::GetProjectRoot(), error);
                    if (!error)
                        choices.push_back(relative.generic_string());
                }
            }
            SortUnique(choices);
        }

        inline const std::vector<std::string>& PortraitStyleChoices()
        {
            auto& cache = PortraitStyleChoicesCache();
            if (cache.empty())
                RefreshPortraitStyleChoices();
            return cache;
        }


} // namespace Wheatear::VisualNovelScriptEditorPanelInternal
