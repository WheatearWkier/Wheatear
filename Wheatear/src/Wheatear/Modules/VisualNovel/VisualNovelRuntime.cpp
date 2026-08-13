#include "wtpch.h"
#include "VisualNovelRuntime.h"

#include "Wheatear/Core/Log.h"
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

        static std::vector<std::string> SplitWords(const std::string& value)
        {
            std::vector<std::string> words;
            std::istringstream stream(value);
            std::string word;
            while (stream >> word)
                words.push_back(word);
            return words;
        }

        static bool CompareFloat(float left, const std::string& op, float right)
        {
            if (op == "==" || op == "=") return left == right;
            if (op == "!=") return left != right;
            if (op == ">") return left > right;
            if (op == ">=") return left >= right;
            if (op == "<") return left < right;
            if (op == "<=") return left <= right;
            return false;
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
        std::vector<std::string> words = SplitWords(expression);
        if (words.empty())
            return false;

        bool negate = false;
        if (ToLower(words.front()) == "not")
        {
            negate = true;
            words.erase(words.begin());
        }
        if (words.empty())
            return false;

        const std::string kind = ToLower(words[0]);
        bool value = false;

        if (kind == "always")
        {
            value = true;
        }
        else if (kind == "never")
        {
            value = false;
        }
        else if (kind == "flag" && words.size() >= 2)
        {
            // Progression story flags stay authoritative for cross-system
            // conditions (set via progression:set_flag / dungeon clears).
            value = GameProgress::GetState().StoryFlags.count(words[1]) > 0;
        }
        else if (words.size() >= 3)
        {
            // <operand> OP <operand>  — left may be a variable or number.
            float left = 0.0f;
            if (!TryParseFloat(words[0], left))
            {
                const auto it = variables.find(words[0]);
                if (it != variables.end())
                    left = it->second;
                else
                    return negate ? true : false; // unknown variable -> false
            }
            float right = 0.0f;
            if (!TryParseFloat(words[2], right))
            {
                const auto it = variables.find(words[2]);
                if (it != variables.end())
                    right = it->second;
                else
                    return negate ? true : false; // unknown variable -> false
            }
            value = CompareFloat(left, words[1], right);
        }

        return negate ? !value : value;
    }

    bool VisualNovelRuntime::LoadScript(const std::filesystem::path& filepath)
    {
        SetScript(VisualNovelScript::FromFile(filepath));
        return !m_Script.IsEmpty();
    }

    void VisualNovelRuntime::SetScript(const VisualNovelScript& script)
    {
        m_Script = script;
        Restart();
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
            std::filesystem::create_directories(parent);

        std::ofstream file(filepath, std::ios::trunc);
        if (!file.is_open())
        {
            WT_CORE_WARN("VisualNovelRuntime: cannot save state '{}'", filepath.string());
            return false;
        }

        file << "VNSTATE 1\n";
        file << "SOURCE " << EscapeField(m_Script.GetSourcePath().string()) << "\n";
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
            LoadScript(savedSource);

        if (m_Script.IsEmpty())
            return false;

        m_CurrentLineIndex = savedIndex;
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
                // Assign a literal number or copy another variable, then advance.
                float value = 0.0f;
                if (!TryParseFloat(line.VariableValue, value))
                {
                    const auto it = m_Variables.find(line.VariableValue);
                    if (it != m_Variables.end())
                        value = it->second;
                }
                m_Variables[line.VariableName] = value;
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
