#pragma once

namespace Wheatear {

    // Functional domain for editor tools and components. Shared infrastructure
    // (Transform, audio, events, UI) is Any; 2D/3D-only entries disappear from
    // the other workspace instead of being shown as disabled clutter.
    enum class EditorDomain
    {
        Any = 0,
        TwoD,
        ThreeD
    };

    class EditorWorkspaceContext
    {
    public:
        static void SetCurrentDomain(EditorDomain domain) { s_Domain = domain; }
        static EditorDomain GetCurrentDomain() { return s_Domain; }

        static bool IsVisible(EditorDomain domain)
        {
            return domain == EditorDomain::Any || s_Domain == EditorDomain::Any
                || domain == s_Domain;
        }

    private:
        inline static EditorDomain s_Domain = EditorDomain::Any;
    };

} // namespace Wheatear
