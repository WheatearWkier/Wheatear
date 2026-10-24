#include "wtpch.h"
#include "VisualNovelScript.h"

#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Logic/LogicExpression.h"
#include "Wheatear/Utils/StringUtils.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace Wheatear {

    using Wheatear::StringUtils::StartsWith;
    using Wheatear::StringUtils::ToLower;
    using Wheatear::StringUtils::Trim;

    namespace {

        uint64_t HashSource(const std::string& text)
        {
            uint64_t hash = 1469598103934665603ull;
            for (unsigned char c : text)
            {
                hash ^= c;
                hash *= 1099511628211ull;
            }
            return hash;
        }

    } // namespace

    static std::string StripQuotes(const std::string& value)
    {
        std::string result = Trim(value);
        if (result.size() >= 2)
        {
            const char first = result.front();
            const char last = result.back();
            if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
            {
                const char quote = first;
                std::string unescaped;
                unescaped.reserve(result.size() - 2);
                for (size_t i = 1; i + 1 < result.size(); ++i)
                {
                    if (result[i] == '\\' && i + 2 < result.size())
                    {
                        const char escaped = result[i + 1];
                        if (escaped == '\\' || escaped == '"' || escaped == '\'')
                        {
                            unescaped.push_back(escaped);
                            ++i;
                            continue;
                        }
                        if (escaped == 'n')
                        {
                            unescaped.push_back('\n');
                            ++i;
                            continue;
                        }
                    }
                    unescaped.push_back(result[i]);
                }
                (void)quote;
                return unescaped;
            }
        }
        return result;
    }

    static bool IsQuoteOpener(const std::string& text, size_t index, char c)
    {
        if (c == '"')
            return true;
        if (c != '\'')
            return false;
        // Treat a single quote as an opener only at a token boundary so
        // ordinary apostrophes ("Don't", "what's") do not swallow the line.
        if (index == 0)
            return true;
        const char previous = text[index - 1];
        return std::isspace(static_cast<unsigned char>(previous))
            || previous == '|' || previous == '>' || previous == '('
            || previous == '[' || previous == '{' || previous == '='
            || previous == ':' || previous == ',';
    }

    static std::vector<std::string> SplitOutsideQuotes(const std::string& text,
        char separator)
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
            if (IsQuoteOpener(text, i, c))
            {
                quote = c;
                current.push_back(c);
                continue;
            }
            if (c == separator)
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

    static size_t FindOutsideQuotes(const std::string& text,
        const std::string& needle)
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
            if (IsQuoteOpener(text, i, c))
            {
                quote = c;
                continue;
            }
            if (text.compare(i, needle.size(), needle) == 0)
                return i;
        }
        return std::string::npos;
    }

    static std::vector<std::string> SplitWords(const std::string& text)
    {
        std::vector<std::string> result;
        std::istringstream stream(text);
        std::string token;
        while (stream >> token)
            result.push_back(token);
        return result;
    }

    static std::string UnescapeInlineText(const std::string& text)
    {
        std::string result;
        result.reserve(text.size());
        for (size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] == '\\' && i + 1 < text.size())
            {
                if (text[i + 1] == 'n')
                {
                    result.push_back('\n');
                    ++i;
                    continue;
                }
                if (text[i + 1] == '\\')
                {
                    result.push_back('\\');
                    ++i;
                    continue;
                }
            }
            result.push_back(text[i]);
        }
        return result;
    }

    static std::string ConsumeToken(std::string& text)
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

    static bool ReadCommandPayload(const std::string& line,
        const std::string& command,
        std::string& payload)
    {
        const std::string taggedCommand = "@" + command;
        if (line == command || line == taggedCommand)
        {
            payload.clear();
            return true;
        }

        if (StartsWith(line, command + " "))
        {
            payload = Trim(line.substr(command.size() + 1));
            return true;
        }

        if (StartsWith(line, taggedCommand + " "))
        {
            payload = Trim(line.substr(taggedCommand.size() + 1));
            return true;
        }

        return false;
    }

    static bool ReadAnyCommandPayload(const std::string& line,
        std::initializer_list<const char*> commands,
        std::string& payload)
    {
        for (const char* command : commands)
        {
            if (ReadCommandPayload(line, command, payload))
                return true;
        }
        return false;
    }

    // Splits a trailing " if <condition>" gate off a TargetLabel. The last
    // standalone "if" token wins so conditions may contain any number of
    // tokens, e.g. "-> ending if gold >= 5 and flag:CH02". The legacy
    // "if flag <id>" form is kept in RequiredFlag for save compatibility;
    // everything else goes to RequiredCondition.
    static std::string ExtractRequiredCondition(std::string target,
        std::string& outFlag,
        std::string& outCondition)
    {
        target = Trim(target);
        if (target.empty())
            return target;

        const std::vector<std::string> words = SplitWords(target);
        if (words.size() < 3)
            return target;

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
            return target;

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
        clause = Trim(clause);

        // Backward-compatible flag-only gate.
        if (words.size() == ifIndex + 3 && words[ifIndex + 1] == "flag")
        {
            std::string flagId = words[ifIndex + 2];
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
            outCondition = clause;
        }

        return StripQuotes(Trim(label));
    }

    static std::vector<VisualNovelChoice> ParseChoices(const std::string& payload)
    {
        std::vector<VisualNovelChoice> choices;
        for (std::string segment : SplitOutsideQuotes(payload, '|'))
        {
            segment = Trim(segment);
            if (segment.empty())
                continue;

            const size_t arrow = FindOutsideQuotes(segment, "->");
            if (arrow == std::string::npos)
            {
                choices.push_back({ StripQuotes(segment), {}, {}, {} });
                continue;
            }

            VisualNovelChoice choice;
            choice.Text = StripQuotes(segment.substr(0, arrow));
            choice.TargetLabel = StripQuotes(segment.substr(arrow + 2));

            // Optional trailing "if flag <id>" / "if <expr>" gate the choice.
            // Only the part after "->" is scanned so option text containing "if"
            // stays unaffected.
            if (!choice.TargetLabel.empty())
            {
                choice.TargetLabel = ExtractRequiredCondition(choice.TargetLabel,
                    choice.RequiredFlag,
                    choice.RequiredCondition);
            }

            if (!choice.Text.empty())
                choices.push_back(std::move(choice));
        }

        return choices;
    }

    static std::pair<std::string, std::string> ParseCharacterToken(const std::string& token)
    {
        const size_t colon = token.find(':');
        if (colon == std::string::npos)
            return { token, {} };

        return {
            Trim(token.substr(0, colon)),
            Trim(token.substr(colon + 1))
        };
    }

    VisualNovelScript VisualNovelScript::FromFile(const std::filesystem::path& filepath)
    {
        VisualNovelScript script;
        const std::filesystem::path resolved = AssetPath::ResolveRuntimeData(filepath);
        script.m_SourcePath = resolved;

        std::ifstream file(resolved);
        if (!file.is_open())
        {
            WT_CORE_WARN("VisualNovelScript: cannot open {0}", resolved.string());
            return script;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        script.Parse(buffer.str());
        return script;
    }

    VisualNovelScript VisualNovelScript::FromString(const std::string& text)
    {
        VisualNovelScript script;
        script.Parse(text);
        return script;
    }

    size_t VisualNovelScript::FindLabel(const std::string& label) const
    {
        const auto it = m_Labels.find(label);
        if (it == m_Labels.end())
            return std::numeric_limits<size_t>::max();
        return it->second;
    }

    void VisualNovelScript::UpsertCharacter(const std::string& name,
        const std::string& displayName,
        const std::string& style)
    {
        if (name.empty())
            return;

        const std::string resolvedDisplayName = displayName.empty() ? name : displayName;

        auto it = std::find_if(m_Characters.begin(), m_Characters.end(),
            [&](const VisualNovelCharacter& character)
            {
                return character.Name == name;
            });

        if (it == m_Characters.end())
        {
            m_Characters.push_back({ name, resolvedDisplayName, style });
        }
        else
        {
            it->DisplayName = resolvedDisplayName;
            it->Style = style;
        }
    }

    void VisualNovelScript::Parse(const std::string& text)
    {
        m_SourceHash = HashSource(text);
        m_DefaultBackground = "studio";
        m_Characters.clear();
        m_Lines.clear();
        m_Issues.clear();
        m_Labels.clear();

        int sourceLine = 0;
        const auto report = [this](int line, bool error, const std::string& message)
        {
            m_Issues.push_back({ line, error, message });
        };

        std::string currentBackground = m_DefaultBackground;
        std::string currentMusic;
        std::string currentMusicTitle;
        std::vector<std::string> activeCharacters;
        std::unordered_map<std::string, std::string> activeExpressions;
        float currentSpeed = 42.0f;

        auto applyState = [&](VisualNovelLine& line)
        {
            line.Background = currentBackground;
            line.Music = currentMusic;
            line.MusicTitle = currentMusicTitle;
            line.VisibleCharacters = activeCharacters;
            line.CharacterExpressions = activeExpressions;
            line.CharactersPerSecond = currentSpeed;
        };

        std::istringstream stream(text);
        std::string rawLine;
        while (std::getline(stream, rawLine))
        {
            ++sourceLine;
            std::string line = Trim(rawLine);
            if (sourceLine == 1 && StartsWith(line, "\xEF\xBB\xBF"))
                line = Trim(line.substr(3));
            if (line.empty())
                continue;

            if (StartsWith(line, "#") || StartsWith(line, "//") || StartsWith(line, ";"))
                continue;

            std::string payload;
            if (ReadAnyCommandPayload(line, { "background" }, payload))
            {
                currentBackground = StripQuotes(payload);
                if (currentBackground.empty())
                    report(sourceLine, true, "background directive requires a texture path");
                else
                    m_DefaultBackground = currentBackground;
                continue;
            }

            if (ReadAnyCommandPayload(line, { "music", "bgm" }, payload))
            {
                std::string remaining = payload;
                currentMusic = StripQuotes(ConsumeToken(remaining));
                currentMusicTitle = StripQuotes(remaining);

                const std::string loweredMusic = ToLower(currentMusic);
                if (loweredMusic == "none" || loweredMusic == "stop" || loweredMusic == "off")
                {
                    currentMusic.clear();
                    currentMusicTitle.clear();
                }
                continue;
            }

            if (ReadAnyCommandPayload(line, { "speed" }, payload))
            {
                try
                {
                    size_t consumed = 0;
                    const float parsed = std::stof(payload, &consumed);
                    if (consumed != payload.size())
                        throw std::invalid_argument("trailing characters");
                    if (!std::isfinite(parsed) || parsed <= 0.0f)
                        throw std::invalid_argument("speed must be positive");
                    currentSpeed = std::max(1.0f, parsed);
                }
                catch (...)
                {
                    WT_CORE_WARN("VisualNovelScript: invalid speed '{0}'", payload);
                    report(sourceLine, true,
                        "invalid speed '" + payload + "' (expected a positive number)");
                }
                continue;
            }

            if (ReadAnyCommandPayload(line, { "label" }, payload))
            {
                payload = StripQuotes(payload);
                if (payload.empty())
                {
                    report(sourceLine, true, "label directive requires a name");
                    continue;
                }
                if (m_Labels.count(payload) > 0)
                    report(sourceLine, true, "duplicate label '" + payload + "'");
                m_Labels[payload] = m_Lines.size();
                continue;
            }

            if (ReadAnyCommandPayload(line, { "goto" }, payload))
            {
                VisualNovelLine jump;
                jump.Type = VisualNovelLineType::Goto;
                jump.SourceLine = sourceLine;
                jump.TargetLabel = StripQuotes(payload);
                if (jump.TargetLabel.empty())
                    report(sourceLine, true, "goto directive requires a label");
                applyState(jump);
                m_Lines.push_back(std::move(jump));
                continue;
            }

            if (ReadAnyCommandPayload(line, { "end" }, payload))
            {
                VisualNovelLine end;
                end.Type = VisualNovelLineType::End;
                end.SourceLine = sourceLine;
                applyState(end);
                m_Lines.push_back(std::move(end));
                continue;
            }

            if (ReadAnyCommandPayload(line, { "choice" }, payload))
            {
                VisualNovelLine choice;
                choice.Type = VisualNovelLineType::Choice;
                choice.SourceLine = sourceLine;
                choice.Text = "选择一个答案。";
                // Optional prompt prefix: "@choice prompt:请选择 | opt1 -> l1 | ..."
                std::string choicesPayload = payload;
                if (choicesPayload.rfind("prompt:", 0) == 0)
                {
                    const size_t separator = FindOutsideQuotes(choicesPayload, "|");
                    if (separator == std::string::npos)
                    {
                        report(sourceLine, true,
                            "choice prompt requires a '|' before the options");
                        continue;
                    }
                    choice.Text = StripQuotes(Trim(choicesPayload.substr(7, separator - 7)));
                    choicesPayload = choicesPayload.substr(separator + 1);
                }
                choice.Choices = ParseChoices(choicesPayload);
                applyState(choice);
                if (!choice.Choices.empty())
                    m_Lines.push_back(std::move(choice));
                else
                    report(sourceLine, true,
                        "choice directive has no options (expected 'text -> label | ...')");
                continue;
            }

            if (ReadAnyCommandPayload(line, { "expression" }, payload))
            {
                std::istringstream command(payload);
                std::string name;
                std::string expression;
                command >> name >> expression;
                if (!name.empty())
                    activeExpressions[name] = expression.empty() ? "neutral" : expression;
                continue;
            }

            if (ReadAnyCommandPayload(line, { "show" }, payload))
            {
                for (const auto& token : SplitWords(payload))
                {
                    auto [name, expression] = ParseCharacterToken(token);
                    if (name.empty())
                        continue;

                    if (std::find(activeCharacters.begin(), activeCharacters.end(), name) ==
                        activeCharacters.end())
                    {
                        activeCharacters.push_back(name);
                    }

                    if (!expression.empty())
                        activeExpressions[name] = expression;
                }
                continue;
            }

            if (ReadAnyCommandPayload(line, { "hide" }, payload))
            {
                const auto tokens = SplitWords(payload);
                if (tokens.empty() || tokens.front() == "all")
                {
                    activeCharacters.clear();
                    activeExpressions.clear();
                }
                else
                {
                    for (const auto& token : tokens)
                    {
                        auto [name, expression] = ParseCharacterToken(token);
                        activeCharacters.erase(
                            std::remove(activeCharacters.begin(), activeCharacters.end(), name),
                            activeCharacters.end());
                        activeExpressions.erase(name);
                    }
                }
                continue;
            }

            if (ReadAnyCommandPayload(line, { "character" }, payload))
            {
                std::string remaining = payload;
                std::string name = ConsumeToken(remaining);
                std::string displayName = name;
                std::string style;

                std::string firstPayloadToken = ConsumeToken(remaining);
                if (!firstPayloadToken.empty())
                {
                    if (firstPayloadToken.find(':') != std::string::npos || firstPayloadToken.find('/') != std::string::npos)
                    {
                        style = firstPayloadToken;
                        if (!remaining.empty())
                            style += " " + remaining;
                    }
                    else
                    {
                        displayName = firstPayloadToken;
                        style = StripQuotes(remaining);
                    }
                }

                if (style.empty())
                    style = "procedural:" + name;
                UpsertCharacter(name, displayName, style);
                continue;
            }

            if (ReadAnyCommandPayload(line, { "sheet" }, payload))
            {
                std::istringstream command(payload);
                std::string texture;
                float w = 0.0f, h = 0.0f;
                command >> texture >> w >> h;
                if (!texture.empty())
                    currentBackground = texture;
                continue;
            }

            if (ReadAnyCommandPayload(line, { "char" }, payload))
            {
                std::istringstream command(payload);
                std::string name;
                int x = 0, y = 0;
                command >> name >> x >> y;
                UpsertCharacter(name, name, "sheet:" + std::to_string(x) + "," + std::to_string(y));
                continue;
            }

            if (ReadAnyCommandPayload(line, { "set" }, payload))
            {
                // @set name value | @set name = value  (value: number or variable ref)
                std::string remaining = payload;
                std::string name = ConsumeToken(remaining);
                remaining = Trim(remaining);
                if (StartsWith(remaining, "="))
                    remaining = Trim(remaining.substr(1));
                std::string value = StripQuotes(remaining);
                if (name.empty())
                {
                    report(sourceLine, true, "set directive requires a variable name");
                    continue;
                }
                if (value.empty())
                {
                    report(sourceLine, true,
                        "set directive for '" + name + "' requires a value");
                    continue;
                }
                std::string expressionError;
                if (!Logic::Validate(value, &expressionError))
                    report(sourceLine, true,
                        "invalid set expression for '" + name + "': " + expressionError);
                VisualNovelLine setLine;
                setLine.Type = VisualNovelLineType::Set;
                setLine.SourceLine = sourceLine;
                setLine.VariableName = name;
                setLine.VariableValue = value;
                applyState(setLine);
                m_Lines.push_back(std::move(setLine));
                continue;
            }

            if (ReadAnyCommandPayload(line, { "if" }, payload))
            {
                // @if <condition> -> <label>  |  @if <condition> goto <label>
                // Condition grammar (runtime evaluates against script variables):
                //   always | never | flag <id> | <var> OP <number>
                // Optional leading "not ". OP in ==,=,!=,>,>=,<,<=
                std::string remaining = payload;
                const size_t arrow = remaining.find("->");
                size_t split = arrow;
                if (split == std::string::npos)
                {
                    const size_t gotoPos = remaining.find("goto");
                    if (gotoPos != std::string::npos && gotoPos > 0
                        && std::isspace(static_cast<unsigned char>(remaining[gotoPos - 1]))
                        && (gotoPos + 4 >= remaining.size()
                            || std::isspace(static_cast<unsigned char>(remaining[gotoPos + 4]))))
                        split = gotoPos;
                }
                if (split == std::string::npos)
                {
                    report(sourceLine, true,
                        "if directive requires '-> label' or 'goto label'");
                    continue;
                }

                std::string condition = Trim(remaining.substr(0, split));
                std::string labelPart = Trim(remaining.substr(split + (arrow != std::string::npos ? 2 : 4)));
                if (condition.empty())
                {
                    report(sourceLine, true, "if directive requires a condition");
                    continue;
                }
                if (labelPart.empty())
                    report(sourceLine, true, "if directive requires a target label");
                std::string conditionError;
                if (!Logic::Validate(Logic::NormalizeCondition(condition), &conditionError))
                    report(sourceLine, true,
                        "invalid if condition '" + condition + "': " + conditionError);
                VisualNovelLine ifLine;
                ifLine.Type = VisualNovelLineType::If;
                ifLine.SourceLine = sourceLine;
                ifLine.Condition = condition;
                ifLine.TargetLabel = StripQuotes(labelPart);
                applyState(ifLine);
                m_Lines.push_back(std::move(ifLine));
                continue;
            }

            const size_t colon = line.find(':');
            if (colon == std::string::npos)
            {
                report(sourceLine, false,
                    "unrecognized line (expected 'Speaker: text' or '@directive')");
                continue;
            }

            VisualNovelLine dialogue;
            dialogue.Type = VisualNovelLineType::Dialogue;
            dialogue.SourceLine = sourceLine;
            dialogue.Speaker = Trim(line.substr(0, colon));
            dialogue.Text = UnescapeInlineText(Trim(line.substr(colon + 1)));
            if (!dialogue.Speaker.empty() && dialogue.Speaker.front() == '@')
                report(sourceLine, false,
                    "unknown directive '" + dialogue.Speaker + "' (missing space after the directive name?)");
            applyState(dialogue);
            m_Lines.push_back(std::move(dialogue));
        }

        // Cross-reference validation runs after the whole file is parsed so
        // forward labels are legal.
        const auto isExternalTarget = [](const std::string& target)
        {
            return StartsWith(target, "scene:")
                || StartsWith(target, "newgame:")
                || StartsWith(target, "loadgame:")
                || StartsWith(target, "event:");
        };

        for (const VisualNovelLine& line : m_Lines)
        {
            if (line.Type == VisualNovelLineType::Goto
                || line.Type == VisualNovelLineType::If)
            {
                if (!line.TargetLabel.empty()
                    && m_Labels.count(line.TargetLabel) == 0)
                {
                    report(line.SourceLine, true,
                        "jump target '" + line.TargetLabel + "' does not exist");
                }
            }

            if (line.Type == VisualNovelLineType::Choice)
            {
                for (const VisualNovelChoice& choice : line.Choices)
                {
                    if (choice.TargetLabel.empty())
                        continue;
                    if (!isExternalTarget(choice.TargetLabel)
                        && m_Labels.count(choice.TargetLabel) == 0)
                    {
                        report(line.SourceLine, true,
                            "choice target '" + choice.TargetLabel + "' does not exist");
                    }

                    if (!choice.RequiredCondition.empty())
                    {
                        std::string error;
                        if (!Logic::Validate(
                            Logic::NormalizeCondition(choice.RequiredCondition), &error))
                        {
                            report(line.SourceLine, true,
                                "invalid choice condition '" + choice.RequiredCondition
                                    + "': " + error);
                        }
                    }
                }
            }
        }
    }

} // namespace Wheatear
