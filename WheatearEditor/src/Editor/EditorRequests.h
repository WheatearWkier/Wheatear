#pragma once

#include "Wheatear/Core/Core.h"

namespace Wheatear {

    // Lightweight editor-wide request queue. Panels that do not own the
    // editor layer (tuning editors, data editors, ...) post requests here;
    // EditorLayerBase consumes them each frame (SyncPanels).
    namespace EditorRequests {

        void RequestOpenInputBindings();
        bool ConsumeOpenInputBindingsRequest();

        // Selects the first SideCombatLevelComponent entity in the active
        // scene (jump from the tuning editor's item-slots workflow).
        void RequestSelectSideCombatLevelEntity();
        bool ConsumeSelectSideCombatLevelEntityRequest();

        // Generic entity selection by UUID.
        void RequestSelectEntity(UUID uuid);
        bool ConsumeSelectEntityRequest(UUID& uuid);

    } // namespace EditorRequests

} // namespace Wheatear
