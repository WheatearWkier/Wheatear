#include "wepch.h"
#include "EditorPlatform.h"

#include <cstdlib>
#include <string>

#ifdef WT_PLATFORM_WINDOWS
    #include <shellapi.h>
    #include <windows.h>
#endif

namespace Wheatear::EditorPlatform {

    namespace {

        static std::string Quote(const std::filesystem::path& path)
        {
            return "\"" + path.string() + "\"";
        }

    } // namespace

    void OpenDirectory(const std::filesystem::path& directory)
    {
        if (directory.empty() || !std::filesystem::exists(directory))
            return;

#ifdef WT_PLATFORM_WINDOWS
        ShellExecuteA(nullptr, "open", directory.string().c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
#elif defined(WT_PLATFORM_MACOS)
        (void)std::system(("open " + Quote(directory)).c_str());
#else
        (void)std::system(("xdg-open " + Quote(directory)).c_str());
#endif
    }

} // namespace Wheatear::EditorPlatform
