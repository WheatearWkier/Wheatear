#include "wtpch.h"
#include "SceneTransitionService.h"

#include <algorithm>

namespace Wheatear {

    namespace {

        static std::vector<SceneTransitionRequest>& PendingSceneTransitions()
        {
            static std::vector<SceneTransitionRequest> requests;
            return requests;
        }

        static void Queue(SceneTransitionRequest request)
        {
            if (request.ScenePath.empty())
                return;

            request.Slot = std::max(1, request.Slot);
            PendingSceneTransitions().push_back(std::move(request));
        }

    } // namespace

    void SceneTransitionService::RequestLoadScene(const std::filesystem::path& scenePath, const std::string& sourceCommand)
    {
        Queue({ SceneTransitionMode::LoadScene, scenePath, 1, sourceCommand });
    }

    void SceneTransitionService::RequestNewGame(const std::filesystem::path& scenePath, const std::string& sourceCommand)
    {
        Queue({ SceneTransitionMode::NewGame, scenePath, 1, sourceCommand });
    }

    void SceneTransitionService::RequestLoadGame(const std::filesystem::path& scenePath, int slot, const std::string& sourceCommand)
    {
        Queue({ SceneTransitionMode::LoadGame, scenePath, slot, sourceCommand });
    }

    bool SceneTransitionService::HasPendingRequests()
    {
        return !PendingSceneTransitions().empty();
    }

    std::vector<SceneTransitionRequest> SceneTransitionService::DrainRequests()
    {
        std::vector<SceneTransitionRequest> requests;
        requests.swap(PendingSceneTransitions());
        return requests;
    }

} // namespace Wheatear
