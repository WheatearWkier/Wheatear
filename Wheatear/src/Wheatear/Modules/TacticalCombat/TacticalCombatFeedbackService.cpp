#include "wtpch.h"
#include "TacticalCombatFeedbackService.h"

#include "Wheatear/Modules/Common/GameplayAudioService.h"

namespace Wheatear::TacticalCombatFeedbackService {

    void PlaySound(const std::string& path, float volume)
    {
        if (path.empty())
            return;

        GameplayAudioService::PlaySFX(path, volume);
    }

} // namespace Wheatear::TacticalCombatFeedbackService
