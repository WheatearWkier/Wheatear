#pragma once

#include "Wheatear/Core/Core.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Wheatear {

    struct RuntimePatchDescriptor
    {
        std::string Id;
        std::string RelativePath;
        std::filesystem::path PackPath;
        std::string Sha256;
        uint64_t Size = 0;
        bool HasSize = false;
        uint32_t BehaviorSchema = 0;
        std::vector<std::string> DependsOn;
        std::vector<std::string> DeletePaths;
    };

    struct RuntimePatchSet
    {
        std::filesystem::path UpdatesRoot;
        std::filesystem::path ManifestPath;
        std::string Product;
        std::string BaseVersion;
        std::string ContentVersion;
        std::string MinimumRuntimeVersion;
        bool ManifestValid = true;
        bool SignatureRequired = false;
        bool SignatureVerified = false;
        std::vector<RuntimePatchDescriptor> Patches;
        std::vector<std::filesystem::path> PackPaths;
        std::vector<std::string> DeletePaths;
    };

    class WHEATEAR_API RuntimePatchManager
    {
    public:
        // Discovers active local patches. The manifest is optional; without
        // one, active/*.wtpatch is applied in lexical order.
        static RuntimePatchSet DiscoverActivePatches(
            const std::filesystem::path& executableDirectory);

        // Includes the base package and every active patch in the cache key.
        // With no patches this intentionally matches the legacy base-package
        // fingerprint so existing caches remain reusable.
        static std::string BuildContentFingerprint(
            const std::filesystem::path& basePack,
            const RuntimePatchSet& patches);

        // Applies an existing Wheatear pack to a directory. When clearTarget
        // is true the target is rebuilt from scratch; patch overlays pass
        // false and only replace files contained in the patch.
        static bool ApplyPackFile(
            const std::filesystem::path& packPath,
            const std::filesystem::path& targetRoot,
            bool clearTarget,
            std::string* errorMessage = nullptr);

        // Materializes base content and a validated patch set into a private
        // staging directory, then commits the completed directory atomically
        // by rename. A failed patch never leaves a partially usable target.
        static bool MaterializeRuntimeRoot(
            const std::filesystem::path& basePack,
            const RuntimePatchSet& patches,
            const std::filesystem::path& targetRoot,
            std::string* errorMessage = nullptr);

        static bool ApplyDeletePaths(
            const std::filesystem::path& targetRoot,
            const std::vector<std::string>& relativePaths,
            std::string* errorMessage = nullptr);

        // Validates a complete patch set in updates/staging/<version>, then
        // swaps it into updates/active while retaining the previous set.
        static bool ActivateStagedPatchSet(
            const std::filesystem::path& executableDirectory,
            const std::string& version,
            std::string* errorMessage = nullptr);

        // Restores the previous version retained under updates/backup.
        static bool RollbackActivePatches(
            const std::filesystem::path& executableDirectory,
            std::string* errorMessage = nullptr);

        // Repairs an interrupted directory swap after a process or machine
        // failure during activation.
        static bool RecoverInterruptedActivation(
            const std::filesystem::path& executableDirectory,
            std::string* errorMessage = nullptr);

        // Launch health markers allow the next startup to roll back a patch
        // set after repeated failures before the game reaches its healthy
        // scene state.
        static bool RecoverFailedLaunch(
            const std::filesystem::path& executableDirectory,
            std::string* errorMessage = nullptr);

        static bool BeginLaunch(
            const std::filesystem::path& executableDirectory,
            const std::string& contentFingerprint,
            std::string* errorMessage = nullptr);

        static bool MarkLaunchHealthy(
            const std::filesystem::path& executableDirectory,
            std::string* errorMessage = nullptr);
    };

} // namespace Wheatear
