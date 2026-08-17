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
        EndIf,
        // A line the parser could not interpret (unknown instruction, or an
        // instruction outside any event). Preserved verbatim so editor
        // round-trips never silently drop hand-written content; the executor
        // ignores it.
        RawLine
    };

    struct EventScriptInstruction
    {
        EventScriptInstructionType Type = EventScriptInstructionType::Command;
        std::string Text;
        float Seconds = 0.0f;
        int SourceLine = 0;
        // Comment lines ('#' / '//') that appeared directly above this
        // instruction in the source file; preserved so editor round-trips
        // do not destroy documentation comments.
        std::vector<std::string> LeadingComments;
    };

    struct EventScriptBlock
    {
        std::string Name;
        int SourceLine = 0;
        std::vector<EventScriptInstruction> Instructions;
        // Comment lines directly above the "event" line.
        std::vector<std::string> LeadingComments;
    };

    class WHEATEAR_API EventScript
    {
    public:
        static EventScript FromFile(const std::filesystem::path& filepath);
        static EventScript FromString(const std::string& text);

        const EventScriptBlock* FindEvent(const std::string& name) const;
        const std::vector<EventScriptBlock>& GetEvents() const { return m_Events; }
        const std::string& GetSourcePath() const { return m_SourcePath; }
        // Comment lines above the first event (file header).
        const std::vector<std::string>& GetHeaderComments() const { return m_HeaderComments; }
        // Lines that appeared outside any event block (file header, between
        // events, after the last "end"). Preserved with their source line so
        // the editor can write them back in order and nothing is lost.
        const std::vector<EventScriptInstruction>& GetOrphanLines() const { return m_OrphanLines; }
        // Total count of preserved-but-uninterpretable lines (unknown
        // instructions inside events plus orphan lines).
        size_t GetUnrecognizedLineCount() const;

    private:
        void Parse(const std::string& text);

    private:
        std::string m_SourcePath;
        std::vector<EventScriptBlock> m_Events;
        std::vector<EventScriptInstruction> m_OrphanLines;
        std::vector<std::string> m_HeaderComments;
    };

} // namespace Wheatear
