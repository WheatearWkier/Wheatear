#include "wtpch.h"
#include "FileSystem.h"

#include <fstream>
#include <system_error>

namespace Wheatear::FileSystem {

    namespace {

        static bool Fail(std::string* errorMessage, const std::string& message)
        {
            if (errorMessage)
                *errorMessage = message;
            return false;
        }

        static bool Fail(std::string* errorMessage, const std::filesystem::path& path,
            const std::error_code& error)
        {
            if (errorMessage)
                *errorMessage = path.string() + ": " + error.message();
            return false;
        }

    } // namespace

    std::filesystem::path Normalize(const std::filesystem::path& path)
    {
        std::error_code error;
        std::filesystem::path absolute = path.is_absolute()
            ? path
            : std::filesystem::absolute(path, error);

        if (error)
            absolute = path;

        std::filesystem::path canonical = std::filesystem::weakly_canonical(absolute, error);
        return error ? absolute.lexically_normal() : canonical.lexically_normal();
    }

    bool IsSubPath(const std::filesystem::path& child, const std::filesystem::path& parent)
    {
        const std::filesystem::path normalizedChild = Normalize(child);
        const std::filesystem::path normalizedParent = Normalize(parent);
        if (normalizedChild == normalizedParent)
            return true;

        std::error_code error;
        const std::filesystem::path relative =
            std::filesystem::relative(normalizedChild, normalizedParent, error);
        if (error || relative.empty())
            return false;

        for (const auto& part : relative)
        {
            if (part == "..")
                return false;
        }

        return true;
    }

    bool EnsureDirectory(const std::filesystem::path& directory, std::string* errorMessage)
    {
        if (directory.empty())
            return Fail(errorMessage, "Directory path is empty.");

        std::error_code error;
        std::filesystem::create_directories(directory, error);
        return error ? Fail(errorMessage, directory, error) : true;
    }

    bool CopyDirectoryRecursive(const std::filesystem::path& source,
        const std::filesystem::path& destination,
        const DirectoryCopyFilter& shouldSkip,
        std::string* errorMessage)
    {
        if (!std::filesystem::exists(source))
            return Fail(errorMessage, "Source directory does not exist: " + source.string());

        if (!EnsureDirectory(destination, errorMessage))
            return false;

        std::error_code error;
        std::filesystem::recursive_directory_iterator it(source, error);
        const std::filesystem::recursive_directory_iterator end;
        while (!error && it != end)
        {
            const bool isDirectory = it->is_directory();
            const std::filesystem::path relative = std::filesystem::relative(it->path(), source, error);
            if (error)
                return Fail(errorMessage, it->path(), error);

            if (shouldSkip && shouldSkip(relative, isDirectory))
            {
                if (isDirectory)
                    it.disable_recursion_pending();
                it.increment(error);
                continue;
            }

            const std::filesystem::path target = destination / relative;
            if (isDirectory)
            {
                std::filesystem::create_directories(target, error);
            }
            else if (it->is_regular_file())
            {
                std::filesystem::create_directories(target.parent_path(), error);
                if (!error)
                {
                    std::filesystem::copy_file(it->path(), target,
                        std::filesystem::copy_options::overwrite_existing, error);
                }
            }

            if (error)
                return Fail(errorMessage, target, error);

            it.increment(error);
        }

        return error ? Fail(errorMessage, source, error) : true;
    }

    bool WriteTextFile(const std::filesystem::path& path,
        const std::string& contents,
        std::string* errorMessage)
    {
        if (!EnsureDirectory(path.parent_path(), errorMessage))
            return false;

        std::ofstream output(path, std::ios::trunc);
        if (!output.is_open())
            return Fail(errorMessage, "Failed to open file for writing: " + path.string());

        output << contents;
        return true;
    }

} // namespace Wheatear::FileSystem
