#pragma once

#include "Wheatear/Core/Core.h"

#include <string>
#include <vector>

namespace Wheatear {

    // In-editor help browser: lists Resources/Help/*.md topics on the left and
    // renders a Markdown subset (headings / lists / tables / code / quotes) on
    // the right. Opened from the Help menu.
    class EditorHelpPanel
    {
    public:
        void OnImGuiRender();
        void SetOpen(bool open) { m_Open = open; }
        bool IsOpen() const { return m_Open; }

    private:
        struct HelpTopic
        {
            std::string Title;
            std::string Path;
        };

        void LoadTopics();
        void LoadContent(int index);
        void RenderMarkdown(const std::string& text);
        void RenderTable(std::istringstream& stream);

        std::vector<HelpTopic> m_Topics;
        int m_SelectedTopic = 0;
        std::string m_Content;
        bool m_Open = false;
        bool m_TopicsLoaded = false;
        bool m_ContentLoaded = false;
    };

} // namespace Wheatear
