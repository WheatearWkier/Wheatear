#include "wtpch.h"
#include "VisualNovelRuntime.h"

#include "Wheatear/Core/Log.h"
#include "Wheatear/Logic/LogicDiagnostics.h"
#include "Wheatear/Logic/LogicExpression.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Utils/StringUtils.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>

namespace Wheatear {

    using Wheatear::StringUtils::PayloadAfter;
    using Wheatear::StringUtils::StartsWith;
    using Wheatear::StringUtils::ToLower;

    namespace {

        static std::string EscapeField(const std::string& value)
        {
            std::string result;
            result.reserve(value.size());
            for (char c : value)
            {
                switch (c)
                {
                    case '\\': result += "\\\\"; break;
                    case '\n': result += "\\n"; break;
                    case '\r': break;
                    case '|': result += "\\|"; break;
                    default: result += c; break;
                }
            }
            return result;
        }

        static std::string UnescapeField(const std::string& value)
        {
            std::string result;
            result.reserve(value.size());
            bool escaping = false;

            for (char c : value)
            {
                if (escaping)
                {
                    switch (c)
                    {
                        case 'n': result += '\n'; break;
                        case '|': result += '|'; break;
                        case '\\': result += '\\'; break;
                        default: result += c; break;
                    }
                    escaping = false;
                    continue;
                }

                if (c == '\\')
                {
                    escaping = true;
                    continue;
                }

                result += c;
            }

            if (escaping)
                result += '\\';
            return result;
        }

        static std::vector<std::string> SplitEscaped(const std::string& line, char delimiter)
        {
            std::vector<std::string> fields;
            std::string current;
            bool escaping = false;

            for (char c : line)
            {
                if (escaping)
                {
                    current += '\\';
                    current += c;
                    escaping = false;
                    continue;
                }

                if (c == '\\')
                {
                    escaping = true;
                    continue;
                }

                if (c == delimiter)
                {
                    fields.push_back(current);
                    current.clear();
                    continue;
                }

                current += c;
            }

            if (escaping)
                current += '\\';
            fields.push_back(current);
            return fields;
        }

        static size_t AdvanceUTF8(const std::string& text, size_t index)
        {
            if (index >= text.size())
                return text.size();

            const unsigned char lead = static_cast<unsigned char>(text[index]);
            size_t length = 1;

            if ((lead & 0x80) == 0)
                length = 1;
            else if ((lead & 0xe0) == 0xc0)
                length = 2;
            else if ((lead & 0xf0) == 0xe0)
                length = 3;
            else if ((lead & 0xf8) == 0xf0)
                length = 4;
            else
                return index + 1;

            if (index + length > text.size())
                return text.size();

            for (size_t i = 1; i < length; ++i)
            {
                const unsigned char continuation = static_cast<unsigned char>(text[index + i]);
                if ((continuation & 0xc0) != 0x80)
                    return index + 1;
            }

            return index + length;
        }

        static size_t CountUTF8Characters(const std::string& text)
        {
            size_t count = 0;
            size_t index = 0;
            while (index < text.size())
            {
                index = AdvanceUTF8(text, index);
                ++count;
            }
            return count;
        }

        static size_t ByteOffsetForUTF8Characters(const std::string& text, size_t characterCount)
        {
            size_t index = 0;
            size_t count = 0;
            while (index < text.size() && count < characterCount)
            {
                index = AdvanceUTF8(text, index);
                ++count;
            }
            return index;
        }

        static bool TryParseFloat(const std::string& value, float& result)
        {
            if (value.empty())
                return false;
            try
            {
                size_t parsed = 0;
                const float parsedValue = std::stof(value, &parsed);
                if (parsed != value.size())
                    return false;
                result = parsedValue;
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

    } // namespace

    bool EvaluateVNExpression(const std::string& expression,
        const std::unordered_map<std::string, float>& variables)
    {
        const std::string normalized = Logic::NormalizeCondition(expression);
        Logic::VariableScope scope;
        for (const auto& [name, value] : variables)
            scope.SetLocal(name, static_cast<double>(value));
        const Logic::EvalContext context = scope.MakeContext();

        bool value = false;
        std::string error;
        if (!Logic::EvaluateBool(normalized, context, &value, &error))
        {
            WT_CORE_WARN("VisualNovel: invalid condition '{}' ({})",
                expression, error);
            Logic::Diagnostics::Warn("visual-novel",
                "Invalid condition '" + expression + "': " + error);
            return false;
        }
        return value;
    }

    bool VisualNovelRuntime::LoadScript(const std::filesystem::path& filepath)
    {
        SetScript(VisualNovelScript::FromFile(filepath));
        return !m_Script.IsEmpty();
    }

    void VisualNovelRuntime::SetScript(const VisualNovelScript& script)
    {
        m_Script = script;
        const std::string source = m_Script.GetSourcePath().empty()
            ? std::string("<inline>")
            : m_Script.GetSourcePath().generic_string();
        for (const VisualNovelIssue& issue : m_Script.GetIssues())
        {
            const std::string location = source + ":" + std::to_string(issue.SourceLine);
            if (issue.Error)
            {
                WT_CORE_ERROR("VisualNovel {}: {}", location, issue.Message);
                Logic::Diagnostics::Error("visual-novel",
                    location + ": " + issue.Message);
            }
            else
            {
                WT_CORE_WARN("VisualNovel {}: {}", location, issue.Message);
                Logic::Diagnostics::Warn("visual-novel",
                    location + ": " + issue.Message);
            }
        }
        Restart();
    }

    void VisualNovelRuntime::SetScriptPreservingPosition(const VisualNovelScript& script)
    {
        if (m_Script.IsEmpty() || script.IsEmpty())
        {
            SetScript(script);
            return;
        }

        // Find the label governing the current position. Resuming at the
        // nearest label is stable across line insertions/removals, unlike a
        // raw vector index.
        std::string nearestLabel;
        size_t nearestIndex = std::numeric_limits<size_t>::max();
        for (const auto& [label, index] : m_Script.GetLabels())
        {
            if (index <= m_CurrentLineIndex
                && (nearestIndex == std::numeric_limits<size_t>::max()
                    || index >= nearestIndex))
            {
                nearestIndex = index;
                nearestLabel = label;
            }
        }

        const size_t previousIndex = m_CurrentLineIndex;
        const size_t labelOffset = nearestLabel.empty()
            ? 0
            : previousIndex - nearestIndex;
        const auto variables = m_Variables;
        const auto history = m_History;
        const bool autoPlay = m_AutoPlay;

        SetScript(script);

        m_Variables = variables;
        m_History = history;
        m_AutoPlay = autoPlay;

        size_t target = std::numeric_limits<size_t>::max();
        if (!nearestLabel.empty())
            target = script.FindLabel(nearestLabel);
        if (target != std::numeric_limits<size_t>::max())
        {
            const size_t maxIndex = script.GetLines().empty()
                ? 0
                : script.GetLines().size() - 1;
            target = labelOffset <= maxIndex - std::min(target, maxIndex)
                ? target + labelOffset
                : maxIndex;
            target = std::min(target, maxIndex);
        }
        else if (!script.GetLines().empty())
        {
            target = std::min(previousIndex, script.GetLines().size() - 1);
            m_VisibleCharacters = 0.0f;
        }
        if (target != std::numeric_limits<size_t>::max())
        {
            m_CurrentLineIndex = target;
            m_Finished = false;
            NormalizeCurrentNode();
        }
    }

    void VisualNovelRuntime::Restart()
    {
        m_CurrentLineIndex = 0;
        m_VisibleCharacters = 0.0f;
        m_AutoPlayTimer = 0.0f;
        m_History.clear();
        m_Variables.clear();
        m_Finished = m_Script.IsEmpty();
        NormalizeCurrentNode();
    }

    float VisualNovelRuntime::GetVariable(const std::string& name) const
    {
        const auto it = m_Variables.find(name);
        return it == m_Variables.end() ? 0.0f : it->second;
    }

    void VisualNovelRuntime::SetVariable(const std::string& name, float value)
    {
        m_Variables[name] = value;
    }

    bool VisualNovelRuntime::HasVariable(const std::string& name) const
    {
        return m_Variables.count(name) > 0;
    }

    const VisualNovelLine* VisualNovelRuntime::GetCurrentLine() const
    {
        if (m_Finished || m_Script.IsEmpty() || m_CurrentLineIndex >= m_Script.GetLines().size())
            return nullptr;

        const VisualNovelLine& line = m_Script.GetLines()[m_CurrentLineIndex];
        if (line.Type == VisualNovelLineType::Goto
            || line.Type == VisualNovelLineType::End
            || line.Type == VisualNovelLineType::Set
            || line.Type == VisualNovelLineType::If)
            return nullptr;

        return &line;
    }

    const std::string& VisualNovelRuntime::GetCurrentBackground() const
    {
        static const std::string empty;
        if (const VisualNovelLine* line = GetCurrentLine())
            return line->Background.empty() ? m_Script.GetBackground() : line->Background;
        return m_Script.GetBackground().empty() ? empty : m_Script.GetBackground();
    }

    const std::string& VisualNovelRuntime::GetCurrentMusic() const
    {
        static const std::string empty;
        if (const VisualNovelLine* line = GetCurrentLine())
            return line->Music;
        return empty;
    }

    const std::string& VisualNovelRuntime::GetCurrentMusicTitle() const
    {
        static const std::string empty;
        if (const VisualNovelLine* line = GetCurrentLine())
            return line->MusicTitle;
        return empty;
    }

    const std::vector<std::string>& VisualNovelRuntime::GetCurrentVisibleCharacters() const
    {
        static const std::vector<std::string> empty;
        if (const VisualNovelLine* line = GetCurrentLine())
            return line->VisibleCharacters;
        return empty;
    }

    const std::unordered_map<std::string, std::string>& VisualNovelRuntime::GetCurrentCharacterExpressions() const
    {
        static const std::unordered_map<std::string, std::string> empty;
        if (const VisualNovelLine* line = GetCurrentLine())
            return line->CharacterExpressions;
        return empty;
    }

    const std::vector<VisualNovelChoice>& VisualNovelRuntime::GetCurrentChoices() const
    {
        static const std::vector<VisualNovelChoice> empty;
        const VisualNovelLine* line = GetCurrentLine();
        if (!line || line->Type != VisualNovelLineType::Choice)
            return empty;
        return line->Choices;
    }

    float VisualNovelRuntime::GetCurrentLineSpeed() const
    {
        const VisualNovelLine* line = GetCurrentLine();
        if (!line)
            return m_DefaultCharactersPerSecond;
        return line->CharactersPerSecond > 0.0f
            ? line->CharactersPerSecond
            : m_DefaultCharactersPerSecond;
    }

    void VisualNovelRuntime::Update(float deltaSeconds)
    {
        if (m_Finished)
            return;

        NormalizeCurrentNode();

        const VisualNovelLine* line = GetCurrentLine();
        if (!line)
        {
            m_Finished = true;
            return;
        }

        if (line->Type == VisualNovelLineType::Choice)
        {
            m_AutoPlayTimer = 0.0f;
            return;
        }

        const float lineLength = static_cast<float>(CountUTF8Characters(line->Text));
        const float speed = GetCurrentLineSpeed();
        if (speed > 0.0f && m_VisibleCharacters < lineLength)
        {
            m_VisibleCharacters += speed * deltaSeconds;
            if (m_VisibleCharacters > lineLength)
                m_VisibleCharacters = lineLength;
            m_AutoPlayTimer = 0.0f;
            return;
        }

        if (!m_AutoPlay || !IsLineComplete())
            return;

        m_AutoPlayTimer += deltaSeconds;
        if (m_AutoPlayTimer >= m_AutoPlayDelay)
        {
            m_AutoPlayTimer = 0.0f;
            Advance();
        }
    }

    void VisualNovelRuntime::Advance()
    {
        if (m_Script.IsEmpty())
            return;

        if (m_Finished)
        {
            Restart();
            return;
        }

        NormalizeCurrentNode();

        const VisualNovelLine* line = GetCurrentLine();
        if (!line)
        {
            m_Finished = true;
            return;
        }

        if (line->Type == VisualNovelLineType::Choice)
            return;

        if (!IsLineComplete())
        {
            m_VisibleCharacters = static_cast<float>(CountUTF8Characters(line->Text));
            m_AutoPlayTimer = 0.0f;
            return;
        }

        RecordCurrentDialogueToHistory();

        if (m_CurrentLineIndex + 1 < m_Script.GetLines().size())
        {
            ++m_CurrentLineIndex;
            m_VisibleCharacters = 0.0f;
            m_AutoPlayTimer = 0.0f;
            NormalizeCurrentNode();
        }
        else
        {
            m_Finished = true;
        }
    }

    void VisualNovelRuntime::Choose(size_t choiceIndex)
    {
        const VisualNovelLine* line = GetCurrentLine();
        if (!line || line->Type != VisualNovelLineType::Choice || choiceIndex >= line->Choices.size())
            return;

        const VisualNovelChoice& choice = line->Choices[choiceIndex];
        m_History.push_back({ "Choice", choice.Text, m_CurrentLineIndex, true });

        if (!choice.TargetLabel.empty() && JumpToLabel(choice.TargetLabel))
        {
            m_VisibleCharacters = 0.0f;
            m_AutoPlayTimer = 0.0f;
            NormalizeCurrentNode();
            return;
        }

        if (m_CurrentLineIndex + 1 < m_Script.GetLines().size())
        {
            ++m_CurrentLineIndex;
            m_VisibleCharacters = 0.0f;
            m_AutoPlayTimer = 0.0f;
            NormalizeCurrentNode();
        }
        else
        {
            m_Finished = true;
        }
    }

    bool VisualNovelRuntime::IsLineComplete() const
    {
        const VisualNovelLine* line = GetCurrentLine();
        if (!line)
            return true;
        if (line->Type == VisualNovelLineType::Choice)
            return true;
        return m_VisibleCharacters >= static_cast<float>(CountUTF8Characters(line->Text));
    }

    bool VisualNovelRuntime::IsWaitingForChoice() const
    {
        const VisualNovelLine* line = GetCurrentLine();
        return line && line->Type == VisualNovelLineType::Choice;
    }

    bool VisualNovelRuntime::SkipWaitingChoice()
    {
        if (!IsWaitingForChoice())
            return false;

        ++m_CurrentLineIndex;
        m_VisibleCharacters = 0.0f;
        m_Finished = false;
        NormalizeCurrentNode();
        return true;
    }

    std::string VisualNovelRuntime::GetVisibleText() const
    {
        const VisualNovelLine* line = GetCurrentLine();
        if (!line)
            return {};

        if (line->Type == VisualNovelLineType::Choice)
            return line->Text;

        const size_t visibleCount = std::min(
            static_cast<size_t>(m_VisibleCharacters),
            CountUTF8Characters(line->Text));
        return line->Text.substr(0, ByteOffsetForUTF8Characters(line->Text, visibleCount));
    }

    void VisualNovelRuntime::SetAutoPlay(bool enabled)
    {
        m_AutoPlay = enabled;
        m_AutoPlayTimer = 0.0f;
    }

    void VisualNovelRuntime::ToggleAutoPlay()
    {
        SetAutoPlay(!m_AutoPlay);
    }

    void VisualNovelRuntime::SetAutoPlayDelay(float seconds)
    {
        const float delay = std::max(0.2f, seconds);
        if (std::abs(m_AutoPlayDelay - delay) < 0.001f)
            return;

        m_AutoPlayDelay = delay;
        m_AutoPlayTimer = 0.0f;
    }

    bool VisualNovelRuntime::SaveState(const std::filesystem::path& filepath) const
    {
        const std::filesystem::path parent = filepath.parent_path();
        if (!parent.empty())
        {
            std::error_code directoryError;
            std::filesystem::create_directories(parent, directoryError);
            if (directoryError)
            {
                WT_CORE_WARN("VisualNovelRuntime: cannot create save directory '{}'",
                    parent.string());
                return false;
            }
        }

        std::ofstream file(filepath, std::ios::trunc);
        if (!file.is_open())
        {
            WT_CORE_WARN("VisualNovelRuntime: cannot save state '{}'", filepath.string());
            return false;
        }

        file << "VNSTATE 2\n";
        file << "SOURCE " << EscapeField(m_Script.GetSourcePath().string()) << "\n";
        file << "HASH " << m_Script.GetSourceHash() << "\n";

        // Remember the nearest preceding label so a script edit that inserts or
        // removes lines can still resume at a meaningful node.
        std::string nearestLabel;
        size_t nearestIndex = std::numeric_limits<size_t>::max();
        for (const auto& [label, index] : m_Script.GetLabels())
        {
            if (index <= m_CurrentLineIndex
                && (nearestIndex == std::numeric_limits<size_t>::max()
                    || index >= nearestIndex))
            {
                nearestIndex = index;
                nearestLabel = label;
            }
        }
        file << "LABEL " << EscapeField(nearestLabel) << "\n";
        file << "OFFSET "
             << (nearestLabel.empty() || nearestIndex == std::numeric_limits<size_t>::max()
                    ? 0
                    : m_CurrentLineIndex - nearestIndex)
             << "\n";
        file << "INDEX " << m_CurrentLineIndex << "\n";
        file << "VISIBLE " << m_VisibleCharacters << "\n";
        file << "FINISHED " << (m_Finished ? 1 : 0) << "\n";
        file << "AUTOPLAY " << (m_AutoPlay ? 1 : 0) << "\n";
        file << "HISTORY " << m_History.size() << "\n";
        for (const auto& entry : m_History)
        {
            file << "H "
                << entry.LineIndex << "|"
                << (entry.IsChoice ? 1 : 0) << "|"
                << EscapeField(entry.Speaker) << "|"
                << EscapeField(entry.Text) << "\n";
        }

        file << "VARS " << m_Variables.size() << "\n";
        for (const auto& [name, value] : m_Variables)
        {
            file << "V " << EscapeField(name) << "|" << value << "\n";
        }

        return true;
    }

    bool VisualNovelRuntime::LoadState(const std::filesystem::path& filepath)
    {
        std::ifstream file(filepath);
        if (!file.is_open())
        {
            WT_CORE_WARN("VisualNovelRuntime: cannot load state '{}'", filepath.string());
            return false;
        }

std::string line;
        std::filesystem::path savedSource;
        uint64_t savedHash = 0;
        bool hasSavedHash = false;
        std::string savedLabel;
        size_t savedOffset = 0;
        size_t savedIndex = 0;
        float savedVisibleCharacters = 0.0f;
        bool savedFinished = false;
        bool savedAutoPlay = false;
        std::vector<VisualNovelHistoryEntry> savedHistory;
        std::unordered_map<std::string, float> savedVariables;

        while (std::getline(file, line))
        {
            if (line == "VNSTATE 1")
                continue;

            if (StartsWith(line, "SOURCE "))
            {
                savedSource = UnescapeField(PayloadAfter(line, "SOURCE "));
                continue;
            }

            if (StartsWith(line, "HASH "))
            {
                try
                {
                    savedHash = std::stoull(PayloadAfter(line, "HASH "));
                    hasSavedHash = true;
                }
                catch (...) { hasSavedHash = false; }
                continue;
            }

            if (StartsWith(line, "LABEL "))
            {
                savedLabel = UnescapeField(PayloadAfter(line, "LABEL "));
                continue;
            }

            if (StartsWith(line, "OFFSET "))
            {
                try { savedOffset = static_cast<size_t>(std::stoull(PayloadAfter(line, "OFFSET "))); }
                catch (...) { savedOffset = 0; }
                continue;
            }

            if (StartsWith(line, "INDEX "))
            {
                // Guard against corrupt save files: a malformed index must not
                // throw out of the update loop and terminate the game.
                const std::string indexText = PayloadAfter(line, "INDEX ");
                if (!indexText.empty())
                {
                    try { savedIndex = static_cast<size_t>(std::stoull(indexText)); }
                    catch (...) { savedIndex = 0; }
                }
                continue;
            }

            if (StartsWith(line, "VISIBLE "))
            {
                float visible = 0.0f;
                if (TryParseFloat(PayloadAfter(line, "VISIBLE "), visible))
                    savedVisibleCharacters = visible;
                continue;
            }

            if (StartsWith(line, "FINISHED "))
            {
                savedFinished = PayloadAfter(line, "FINISHED ") == "1";
                continue;
            }

            if (StartsWith(line, "AUTOPLAY "))
            {
                savedAutoPlay = PayloadAfter(line, "AUTOPLAY ") == "1";
                continue;
            }

            if (StartsWith(line, "H "))
            {
                const auto fields = SplitEscaped(PayloadAfter(line, "H "), '|');
                if (fields.size() < 4)
                    continue;

                VisualNovelHistoryEntry entry;
                try { entry.LineIndex = static_cast<size_t>(std::stoull(fields[0])); }
                catch (...) { entry.LineIndex = 0; }
                entry.IsChoice = fields[1] == "1";
                entry.Speaker = UnescapeField(fields[2]);
                entry.Text = UnescapeField(fields[3]);
                savedHistory.push_back(std::move(entry));
            }

            if (StartsWith(line, "V "))
            {
                const auto fields = SplitEscaped(PayloadAfter(line, "V "), '|');
                if (fields.size() < 2)
                    continue;
                float value = 0.0f;
                if (TryParseFloat(fields[1], value))
                    savedVariables[UnescapeField(fields[0])] = value;
            }
        }

        if (!savedSource.empty() && savedSource != m_Script.GetSourcePath())
        {
            // Load into a temporary first: a save from another machine or a
            // renamed asset must not wipe the live runtime.
            const VisualNovelScript candidate = VisualNovelScript::FromFile(savedSource);
            if (!candidate.IsEmpty())
                m_Script = candidate;
            else
                WT_CORE_WARN("VisualNovelRuntime: saved source '{}' is missing or empty; "
                    "keeping the current script.", savedSource.string());
        }

        if (m_Script.IsEmpty())
            return false;

        const bool hashMatches = hasSavedHash
            && savedHash == m_Script.GetSourceHash();
        const bool scriptChanged = hasSavedHash && !hashMatches;
        size_t resolvedIndex = savedIndex;
        if (!hashMatches && !savedLabel.empty())
        {
            const size_t labelIndex = m_Script.FindLabel(savedLabel);
            const size_t maxIndex = m_Script.GetLines().empty()
                ? 0
                : m_Script.GetLines().size() - 1;
            if (labelIndex != std::numeric_limits<size_t>::max())
            {
                const size_t remaining = labelIndex <= maxIndex
                    ? maxIndex - labelIndex
                    : 0;
                resolvedIndex = labelIndex + std::min(savedOffset, remaining);
            }
            else if (scriptChanged)
            {
                WT_CORE_WARN("VisualNovelRuntime: save label '{}' no longer exists; "
                    "falling back to line index.", savedLabel);
            }
        }

        m_CurrentLineIndex = resolvedIndex;
        if (m_CurrentLineIndex >= m_Script.GetLines().size())
        {
            m_CurrentLineIndex = m_Script.GetLines().empty()
                ? 0
                : m_Script.GetLines().size() - 1;
            savedFinished = true;
        }

        m_VisibleCharacters = std::max(0.0f, savedVisibleCharacters);
        m_Finished = savedFinished;
        m_AutoPlay = savedAutoPlay;
        m_AutoPlayTimer = 0.0f;
        m_History = std::move(savedHistory);
        m_Variables = std::move(savedVariables);
        NormalizeCurrentNode();
        return true;
    }

    bool VisualNovelRuntime::JumpToLabel(const std::string& label)
    {
        const size_t target = m_Script.FindLabel(label);
        if (target == std::numeric_limits<size_t>::max())
        {
            WT_CORE_WARN("VisualNovelRuntime: label '{0}' not found", label);
            return false;
        }

        m_CurrentLineIndex = target;
        m_Finished = false;
        return true;
    }

    void VisualNovelRuntime::NormalizeCurrentNode()
    {
        if (m_Script.IsEmpty())
        {
            m_Finished = true;
            return;
        }

        const auto& lines = m_Script.GetLines();
        if (m_CurrentLineIndex >= lines.size())
        {
            m_Finished = true;
            return;
        }

        size_t guard = 0;
        const size_t maxSteps = lines.size() + 8;
        while (!m_Finished && m_CurrentLineIndex < lines.size() && guard++ < maxSteps)
        {
            const VisualNovelLine& line = lines[m_CurrentLineIndex];
            if (line.Type == VisualNovelLineType::Goto)
            {
                if (!JumpToLabel(line.TargetLabel))
                    m_Finished = true;
                m_VisibleCharacters = 0.0f;
                continue;
            }

            if (line.Type == VisualNovelLineType::Set)
            {
                // @set accepts the shared expression language, so authors can
                // write "@set gold = gold + 5" or "@set hp = maxhp - damage".
                Logic::VariableScope scope;
                for (const auto& [name, value] : m_Variables)
                    scope.SetLocal(name, static_cast<double>(value));
                double value = 0.0;
                std::string error;
                if (!Logic::Evaluate(line.VariableValue, scope.MakeContext(), &value, &error))
                {
                    WT_CORE_WARN("VisualNovel: invalid set expression '{}' for '{}' ({})",
                        line.VariableValue, line.VariableName, error);
                    Logic::Diagnostics::Warn("visual-novel",
                        "Invalid set expression '" + line.VariableValue + "' for '"
                            + line.VariableName + "': " + error);
                }
                m_Variables[line.VariableName] = static_cast<float>(value);
                ++m_CurrentLineIndex;
                continue;
            }

            if (line.Type == VisualNovelLineType::If)
            {
                // Conditional jump: evaluate against variables; true -> label,
                // false -> fall through to the next line.
                if (EvaluateVNExpression(line.Condition, m_Variables))
                {
                    if (!JumpToLabel(line.TargetLabel))
                        m_Finished = true;
                    m_VisibleCharacters = 0.0f;
                }
                else
                {
                    ++m_CurrentLineIndex;
                }
                continue;
            }

            if (line.Type == VisualNovelLineType::End)
            {
                m_Finished = true;
                return;
            }

            if (line.Type == VisualNovelLineType::Choice)
            {
                m_VisibleCharacters = static_cast<float>(CountUTF8Characters(line.Text));
                return;
            }

            const float lineLength = static_cast<float>(CountUTF8Characters(line.Text));
            if (m_VisibleCharacters > lineLength)
                m_VisibleCharacters = lineLength;
            return;
        }

        if (guard >= maxSteps)
        {
            WT_CORE_WARN("VisualNovelRuntime: command loop detected");
            m_Finished = true;
        }
    }

    void VisualNovelRuntime::RecordCurrentDialogueToHistory()
    {
        const VisualNovelLine* line = GetCurrentLine();
        if (!line || line->Type != VisualNovelLineType::Dialogue)
            return;

        m_History.push_back({
            line->Speaker,
            line->Text,
            m_CurrentLineIndex,
            false
        });
    }

} // namespace Wheatear
