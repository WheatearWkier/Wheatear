#pragma once

#include "Wheatear/Core/Core.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Wheatear {

    enum class EventScriptInstructionType
    {
        Command = 0,
        Wait,
        If,
        EndIf
    };

    struct EventScriptInstruction
    {
        EventScriptInstructionType Type = EventScriptInstructionType::Command;
        std::string Text;
        float Seconds = 0.0f;
        int SourceLine = 0;
    };

    struct EventScriptBlock
    {
        std::string Name;
        std::vector<EventScriptInstruction> Instructions;
    };

    class WHEATEAR_API EventScript
    {
    public:
        static EventScript FromFile(const std::filesystem::path& filepath);
        static EventScript FromString(const std::string& text);

        const EventScriptBlock* FindEvent(const std::string& name) const;
        const std::vector<EventScriptBlock>& GetEvents() const { return m_Events; }
        const std::string& GetSourcePath() const { return m_SourcePath; }

    private:
        void Parse(const std::string& text);

    private:
        std::string m_SourcePath;
        std::vector<EventScriptBlock> m_Events;
    };

} // namespace Wheatear
