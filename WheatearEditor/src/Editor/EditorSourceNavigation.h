#pragma once

#include <string>

namespace Wheatear::EditorSourceNavigation {

    // Opens an authored asset in its best editor. `line` is 1-based and is used
    // by the .wts / .vn editors to select the nearest instruction or row.
    bool OpenAsset(const std::string& path, int line = 0);

} // namespace Wheatear::EditorSourceNavigation
