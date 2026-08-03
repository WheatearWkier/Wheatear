#include "wtpch.h"
#include "EventScript.h"

#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/Log.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace Wheatear {

    namespace {

        static std::string Trim(std::string value)
        {
            auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
            value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                [&](unsigned char c) { return !isSpace(c); }));
            value.erase(std::find_if(value.rbegin(), value.rend(),
                [&](unsigned char c) { return !isSpace(c); }).base(), value.end());
            return value;
        }

        static std::string ToLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        static bool StartsWith(const std::string& value, const std::string& prefix)
        {
            return value.rfind(prefix, 0) == 0;
        }

        static bool IsBareCommand(const std::string& line)
        {
            return line == "quit"
                || StartsWith(line, "scene:")
                || StartsWith(line, "newgame:")
                || StartsWith(line, "loadgame:")
                || StartsWith(line, "progression:")
                || StartsWith(line, "ui:")
                || StartsWith(line, "event:");
        }

        static std::string StripEventName(std::string payload)
        {
            payload = Trim(std::move(payload));
            if (!payload.empty() && payload.back() == ':')
                payload.pop_back();
            return Trim(payload);
        }

    } // namespace

    EventScript EventScript::FromFile(const std::filesystem::path& filepath)
    {
        EventScript script;
        const std::filesystem::path resolved = AssetPath::ResolveRuntimeData(filepath);
        script.m_SourcePath = resolved.generic_string();

        std::ifstream input(resolved, std::ios::binary);
        if (!input)
        {
            WT_CORE_WARN("EventScript: cannot open '{}'", resolved.string());
            return script;
        }

        std::stringstream buffer;
        buffer << input.rdbuf();
        script.Parse(buffer.str());
        return script;
    }

    EventScript EventScript::FromString(const std::string& text)
    {
        EventScript script;
        script.Parse(text);
        return script;
    }

    const EventScriptBlock* EventScript::FindEvent(const std::string& name) const
    {
        const auto it = std::find_if(m_Events.begin(), m_Events.end(),
            [&](const EventScriptBlock& block) { return block.Name == name; });
        return it == m_Events.end() ? nullptr : &(*it);
    }

    void EventScript::Parse(const std::string& text)
    {
        m_Events.clear();

        std::istringstream stream(text);
        std::string rawLine;
        EventScriptBlock* current = nullptr;
        int lineNumber = 0;

        while (std::getline(stream, rawLine))
        {
            ++lineNumber;
            std::string line = Trim(rawLine);
            if (line.empty() || StartsWith(line, "#") || StartsWith(line, "//"))
                continue;

            const std::string lower = ToLower(line);
            if (StartsWith(lower, "event "))
            {
                const std::string eventName = StripEventName(line.substr(6));
                if (eventName.empty())
                {
                    WT_CORE_WARN("EventScript: empty event name at line {}", lineNumber);
                    current = nullptr;
                    continue;
                }

                m_Events.push_back({ eventName, {} });
                current = &m_Events.back();
                continue;
            }

            if (StartsWith(lower, "event:"))
            {
                const std::string eventName = StripEventName(line.substr(6));
                if (eventName.empty())
                {
                    WT_CORE_WARN("EventScript: empty event name at line {}", lineNumber);
                    current = nullptr;
                    continue;
                }

                m_Events.push_back({ eventName, {} });
                current = &m_Events.back();
                continue;
            }

            if (!current)
            {
                WT_CORE_WARN("EventScript: instruction outside event at line {}", lineNumber);
                continue;
            }

            if (lower == "end" || lower == "endevent")
            {
                current = nullptr;
                continue;
            }

            EventScriptInstruction instruction;
            instruction.SourceLine = lineNumber;

            if (StartsWith(lower, "command ") || StartsWith(lower, "cmd "))
            {
                const size_t offset = StartsWith(lower, "command ") ? 8 : 4;
                instruction.Type = EventScriptInstructionType::Command;
                instruction.Text = Trim(line.substr(offset));
            }
            else if (IsBareCommand(line))
            {
                instruction.Type = EventScriptInstructionType::Command;
                instruction.Text = line;
            }
            else if (StartsWith(lower, "wait "))
            {
                instruction.Type = EventScriptInstructionType::Wait;
                try
                {
                    instruction.Seconds = std::max(0.0f, std::stof(Trim(line.substr(5))));
                }
                catch (...)
                {
                    WT_CORE_WARN("EventScript: invalid wait value at line {}", lineNumber);
                    instruction.Seconds = 0.0f;
                }
            }
            else if (StartsWith(lower, "if "))
            {
                instruction.Type = EventScriptInstructionType::If;
                instruction.Text = Trim(line.substr(3));
            }
            else if (lower == "endif" || lower == "end if")
            {
                instruction.Type = EventScriptInstructionType::EndIf;
            }
            else
            {
                WT_CORE_WARN("EventScript: unknown instruction '{}' at line {}", line, lineNumber);
                continue;
            }

            current->Instructions.push_back(std::move(instruction));
        }
    }

} // namespace Wheatear
