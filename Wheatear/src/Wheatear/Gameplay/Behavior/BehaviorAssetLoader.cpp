#include "wtpch.h"
#include "BehaviorAssetLoader.h"

#include "BehaviorCompiler.h"
#include "BehaviorDatabase.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/Log.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <system_error>
#include <vector>

namespace Wheatear::WAO {

    namespace {

        static bool IsBehaviorFile(const std::filesystem::path& path)
        {
            return path.extension().generic_string() == ".wtb";
        }

        static std::shared_ptr<BehaviorProgram> CompileFile(
            const std::filesystem::path& path,
            std::string* error)
        {
            try
            {
                BehaviorProgram parsed;
                if (!BehaviorCompiler::Compile(
                    YAML::LoadFile(path.string()),
                    path,
                    &parsed,
                    error))
                {
                    return {};
                }
                return std::make_shared<BehaviorProgram>(std::move(parsed));
            }
            catch (const YAML::Exception& exception)
            {
                if (error)
                    *error = exception.what();
                return {};
            }
        }

    } // namespace

    size_t BehaviorAssetLoader::LoadFile(const std::filesystem::path& path)
    {
        const std::filesystem::path resolved = AssetPath::ResolveRuntimeData(path);
        if (!std::filesystem::is_regular_file(resolved))
            return 0;

        std::string error;
        std::shared_ptr<BehaviorProgram> program = CompileFile(resolved, &error);
        if (!program)
        {
            WT_CORE_WARN("BehaviorAssetLoader: failed to load '{}': {}",
                resolved.string(),
                error);
            return 0;
        }

        if (!BehaviorDatabase::Register(program, &error))
        {
            WT_CORE_WARN("BehaviorAssetLoader: failed to register '{}': {}",
                resolved.string(),
                error);
            return 0;
        }

        WT_CORE_INFO("BehaviorAssetLoader: loaded {} behavior entr{} from '{}'",
            program->Entries.size(),
            program->Entries.size() == 1 ? "y" : "ies",
            resolved.string());
        return program->Entries.size();
    }

    size_t BehaviorAssetLoader::LoadDirectory(const std::filesystem::path& path)
    {
        const std::filesystem::path resolved = AssetPath::ResolveRuntimeData(path);
        if (!std::filesystem::is_directory(resolved))
            return 0;

        std::vector<std::filesystem::path> files;
        std::error_code error;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(resolved, error))
        {
            if (error || !entry.is_regular_file() || !IsBehaviorFile(entry.path()))
                continue;
            files.push_back(entry.path());
        }
        std::sort(files.begin(), files.end());

        size_t loaded = 0;
        for (const auto& file : files)
            loaded += LoadFile(file);
        return loaded;
    }

    size_t BehaviorAssetLoader::ReloadDirectory(const std::filesystem::path& path)
    {
        const std::filesystem::path resolved = AssetPath::ResolveRuntimeData(path);
        if (!std::filesystem::is_directory(resolved))
            return 0;

        std::vector<std::filesystem::path> files;
        std::error_code error;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(resolved, error))
        {
            if (error || !entry.is_regular_file() || !IsBehaviorFile(entry.path()))
                continue;
            files.push_back(entry.path());
        }
        std::sort(files.begin(), files.end());

        std::vector<std::shared_ptr<BehaviorProgram>> programs;
        programs.reserve(files.size());
        for (const auto& file : files)
        {
            std::string compileError;
            std::shared_ptr<BehaviorProgram> program =
                CompileFile(file, &compileError);
            if (!program)
            {
                WT_CORE_WARN("BehaviorAssetLoader: atomic reload rejected '{}': {}",
                    file.string(),
                    compileError);
                return 0;
            }
            programs.push_back(std::move(program));
        }

        std::string replaceError;
        if (!BehaviorDatabase::Replace(programs, &replaceError))
        {
            WT_CORE_WARN("BehaviorAssetLoader: atomic reload rejected '{}': {}",
                resolved.string(),
                replaceError);
            return 0;
        }

        size_t loaded = 0;
        for (const auto& program : programs)
            loaded += program->Entries.size();
        return loaded;
    }

} // namespace Wheatear::WAO
