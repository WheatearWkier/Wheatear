#include "wtpch.h"
#include "VisualNovelScript.h"

#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Utils/StringUtils.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <sstream>

namespace Wheatear {

    using Wheatear::StringUtils::StartsWith;
    using Wheatear::StringUtils::ToLower;
    using Wheatear::StringUtils::Trim;

    static std::string StripQuotes(const std::string& value)
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

    static std::vector<std::string> SplitWords(const std::string& text)
    {
        std::vector<std::string> result;
        std::istringstream stream(text);
        std::string token;
        while (stream >> token)
            result.push_back(token);
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

    // Splits a trailing " if flag <id>" / "if flag:<id>" gate off a TargetLabel.
    // Returns the label without the gate; writes the flag id into outFlag (empty if
    // no well-formed gate). The whole-token "if flag" form requires three words
    // after "->", so label text containing "if" is only matched when it forms a
    // standalone clause.
    static std::string ExtractRequiredCondition(std::string target,
        std::string& outFlag,
        std::string& outCondition)
    {
        target = Trim(target);
        if (target.empty())
            return target;

        // The gate is the trailing clause after a standalone " if " token
        // (whitespace-delimited; token-aware so "if" inside a word never matches).
        //   -> label if flag <id>   : progression flag gate (backward compatible)
        //   -> label if <expr>      : full VN condition (variables / comparisons)
        const std::vector<std::string> words = SplitWords(target);
        if (words.size() < 3)
            return target;

        size_t ifIndex = 0;
        bool foundIf = false;
        if (words.size() >= 3 && words[words.size() - 3] == "if")
        {
            ifIndex = words.size() - 3;
            foundIf = true;
        }
        if (!foundIf)
            return target;

        // Reconstruct the label as words[0..ifIndex-1] joined.
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

        if (words.size() == ifIndex + 3 && words[ifIndex + 1] == "flag")
        {
            std::string flagId = words[ifIndex + 2];
            // Normalize "flag:X" -> "X" if authors used the colon form.
            if (flagId.rfind("flag:", 0) == 0 && flagId.size() > 5)
                flagId = flagId.substr(5);
            outFlag = Trim(flagId);
        }
        else
        {
            outCondition = clause;
        }

        return Trim(label);
    }

    static std::vector<VisualNovelChoice> ParseChoices(const std::string& payload)
    {
        std::vector<VisualNovelChoice> choices;
        std::stringstream stream(payload);
        std::string segment;

        while (std::getline(stream, segment, '|'))
        {
            segment = Trim(segment);
            if (segment.empty())
                continue;

            const size_t arrow = segment.find("->");
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
        m_DefaultBackground = "studio";
        m_Characters.clear();
        m_Lines.clear();
        m_Labels.clear();

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
            std::string line = Trim(rawLine);
            if (line.empty())
                continue;

            if (StartsWith(line, "#") || StartsWith(line, "//") || StartsWith(line, ";"))
                continue;

            std::string payload;
            if (ReadAnyCommandPayload(line, { "background" }, payload))
            {
                currentBackground = StripQuotes(payload);
                if (!currentBackground.empty())
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
                    currentSpeed = std::max(1.0f, std::stof(payload));
                }
                catch (...)
                {
                    WT_CORE_WARN("VisualNovelScript: invalid speed '{0}'", payload);
                }
                continue;
            }

            if (ReadAnyCommandPayload(line, { "label" }, payload))
            {
                payload = StripQuotes(payload);
                if (!payload.empty())
                    m_Labels[payload] = m_Lines.size();
                continue;
            }

            if (ReadAnyCommandPayload(line, { "goto" }, payload))
            {
                VisualNovelLine jump;
                jump.Type = VisualNovelLineType::Goto;
                jump.TargetLabel = StripQuotes(payload);
                applyState(jump);
                m_Lines.push_back(std::move(jump));
                continue;
            }

            if (ReadAnyCommandPayload(line, { "end" }, payload))
            {
                VisualNovelLine end;
                end.Type = VisualNovelLineType::End;
                applyState(end);
                m_Lines.push_back(std::move(end));
                continue;
            }

            if (ReadAnyCommandPayload(line, { "choice" }, payload))
            {
                VisualNovelLine choice;
                choice.Type = VisualNovelLineType::Choice;
                choice.Text = "选择一个答案。";
                // Optional prompt prefix: "@choice prompt:请选择 | opt1 -> l1 | ..."
                std::string choicesPayload = payload;
                if (choicesPayload.rfind("prompt:", 0) == 0)
                {
                    const size_t separator = choicesPayload.find('|');
                    if (separator != std::string::npos)
                    {
                        choice.Text = StripQuotes(Trim(choicesPayload.substr(7, separator - 7)));
                        choicesPayload = choicesPayload.substr(separator + 1);
                    }
                }
                choice.Choices = ParseChoices(choicesPayload);
                applyState(choice);
                if (!choice.Choices.empty())
                    m_Lines.push_back(std::move(choice));
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
                    continue;
                VisualNovelLine setLine;
                setLine.Type = VisualNovelLineType::Set;
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
                    continue;

                std::string condition = Trim(remaining.substr(0, split));
                std::string labelPart = Trim(remaining.substr(split + (arrow != std::string::npos ? 2 : 4)));
                // goto <label> form keeps the label after "goto"
                if (arrow == std::string::npos)
                {
                    const size_t afterGoto = labelPart.find("goto");
                    if (afterGoto != std::string::npos)
                        labelPart = Trim(labelPart.substr(afterGoto + 4));
                }
                if (condition.empty())
                    continue;
                VisualNovelLine ifLine;
                ifLine.Type = VisualNovelLineType::If;
                ifLine.Condition = condition;
                ifLine.TargetLabel = StripQuotes(labelPart);
                applyState(ifLine);
                m_Lines.push_back(std::move(ifLine));
                continue;
            }

            const size_t colon = line.find(':');
            if (colon == std::string::npos)
                continue;

            VisualNovelLine dialogue;
            dialogue.Type = VisualNovelLineType::Dialogue;
            dialogue.Speaker = Trim(line.substr(0, colon));
            dialogue.Text = Trim(line.substr(colon + 1));
            applyState(dialogue);
            m_Lines.push_back(std::move(dialogue));
        }
    }

} // namespace Wheatear
