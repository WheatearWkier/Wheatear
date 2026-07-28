#pragma once

#include "Wheatear/Core/Core.h"
#include "VisualNovelScript.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear {

    struct VisualNovelHistoryEntry
    {
        std::string Speaker;
        std::string Text;
        size_t LineIndex = 0;
        bool IsChoice = false;
    };

    class WHEATEAR_API VisualNovelRuntime
    {
    public:
        bool LoadScript(const std::filesystem::path& filepath);
        void SetScript(const VisualNovelScript& script);
        void Restart();

        void Update(float deltaSeconds);
        void Advance();
        void Choose(size_t choiceIndex);

        const VisualNovelScript& GetScript() const { return m_Script; }
        const VisualNovelLine* GetCurrentLine() const;

        const std::string& GetCurrentBackground() const;
        const std::vector<std::string>& GetCurrentVisibleCharacters() const;
        const std::unordered_map<std::string, std::string>& GetCurrentCharacterExpressions() const;
        const std::vector<VisualNovelChoice>& GetCurrentChoices() const;
        const std::vector<VisualNovelHistoryEntry>& GetHistory() const { return m_History; }

        std::string GetVisibleText() const;
        bool IsLineComplete() const;
        bool IsWaitingForChoice() const;
        bool IsFinished() const { return m_Finished; }
        size_t GetCurrentLineIndex() const { return m_CurrentLineIndex; }

        void SetCharactersPerSecond(float cps) { m_DefaultCharactersPerSecond = cps; }
        float GetCharactersPerSecond() const { return m_DefaultCharactersPerSecond; }

        void SetAutoPlay(bool enabled);
        bool IsAutoPlay() const { return m_AutoPlay; }
        void ToggleAutoPlay();
        void SetAutoPlayDelay(float seconds);
        float GetAutoPlayDelay() const { return m_AutoPlayDelay; }

        bool SaveState(const std::filesystem::path& filepath) const;
        bool LoadState(const std::filesystem::path& filepath);

    private:
        float GetCurrentLineSpeed() const;
        bool JumpToLabel(const std::string& label);
        void NormalizeCurrentNode();
        void RecordCurrentDialogueToHistory();

    private:
        VisualNovelScript m_Script;
        size_t m_CurrentLineIndex = 0;
        float m_VisibleCharacters = 0.0f;
        float m_DefaultCharactersPerSecond = 42.0f;
        bool m_Finished = true;

        bool m_AutoPlay = false;
        float m_AutoPlayDelay = 1.4f;
        float m_AutoPlayTimer = 0.0f;

        std::vector<VisualNovelHistoryEntry> m_History;
    };

} // namespace Wheatear
