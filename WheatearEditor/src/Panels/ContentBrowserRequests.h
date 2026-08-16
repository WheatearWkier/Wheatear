#pragma once

// Lightweight cross-panel request channel for the content browser, so asset
// reference fields (inspectors, pickers, data editor) can ask the browser to
// reveal + highlight an asset without including the full panel header.
//
// Implemented in ContentBrowserPanel.cpp.

#include <string>

namespace Wheatear::ContentBrowserRequests {

    // Navigates the content browser to the asset's folder, selects it, and
    // flashes a highlight for a moment. `projectRelativePath` may also be an
    // absolute path; it is resolved through AssetPath.
    void RequestReveal(const std::string& projectRelativePath);
    bool ConsumeRevealRequest(std::string& outPath);

} // namespace Wheatear::ContentBrowserRequests
