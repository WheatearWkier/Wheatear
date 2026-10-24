#pragma once

namespace Wheatear {

    // Registers the RTS systems with SceneSystemRegistry. Called only by the
    // --rts sandbox mode so the existing demo scenes keep their system set.
    void RegisterRTSModule();

} // namespace Wheatear
