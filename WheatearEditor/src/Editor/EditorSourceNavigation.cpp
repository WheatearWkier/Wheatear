#include "wepch.h"
#include "EditorSourceNavigation.h"

#include "Modules/VisualNovel/VisualNovelScriptEditorPanel.h"
#include "Panels/BehaviorGraphPanel.h"
#include "Panels/DataFileEditorPanel.h"
#include "Panels/EventScriptGraphPanel.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace Wheatear::EditorSourceNavigation {

    namespace {

        std::string LowerExtension(const std::string& path)
        {
            std::string extension = std::filesystem::path(path).extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return extension;
        }

    } // namespace

    bool OpenAsset(const std::string& path, int line)
    {
        if (path.empty())
            return false;

        const std::string extension = LowerExtension(path);
        if (extension == ".wts")
        {
            EventScriptGraphRequests::RequestOpenScript(path, {}, line);
            return true;
        }
        if (extension == ".vn")
        {
            VisualNovelEditorRequests::RequestOpenScript(path, line);
            return true;
        }
        if (extension == ".wtb")
        {
            BehaviorGraphRequests::RequestOpenBehavior(path);
            return true;
        }

        // Data files, action YAML, settings and unrecognized text all have a
        // generic structured/raw editor.
        DataFileEditorRequests::RequestOpen(path);
        return true;
    }

} // namespace Wheatear::EditorSourceNavigation
