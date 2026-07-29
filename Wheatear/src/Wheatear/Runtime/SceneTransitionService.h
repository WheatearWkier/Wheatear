#pragma once

#include "Wheatear/Core/Core.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Wheatear {

    enum class SceneTransitionMode
    {
        LoadScene,
        NewGame,
        LoadGame
    };

    struct SceneTransitionRequest
    {
        SceneTransitionMode Mode = SceneTransitionMode::LoadScene;
        std::filesystem::path ScenePath;
        int Slot = 1;
        std::string SourceCommand;
    };

    class WHEATEAR_API SceneTransitionService
    {
    public:
        static void RequestLoadScene(const std::filesystem::path& scenePath, const std::string& sourceCommand = {});
        static void RequestNewGame(const std::filesystem::path& scenePath, const std::string& sourceCommand = {});
        static void RequestLoadGame(const std::filesystem::path& scenePath, int slot, const std::string& sourceCommand = {});

        static bool HasPendingRequests();
        static std::vector<SceneTransitionRequest> DrainRequests();
    };

} // namespace Wheatear
