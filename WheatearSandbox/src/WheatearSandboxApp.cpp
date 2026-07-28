#include <Wheatear.h>
#include <Wheatear/Core/EntryPoint.h>

#include "RuntimeSceneLayer.h"

#include "stb_image/stb_image.h"

#include <algorithm>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace {

	constexpr const char* kAssetPackFilename = "content.wtpack";
	constexpr char kAssetPackMagic[8] = { 'W', 'T', 'P', 'A', 'C', 'K', '1', '\0' };
	constexpr uint32_t kAssetPackVersion = 1;
	constexpr uint32_t kAssetPackMethodStore = 0;
	constexpr uint32_t kAssetPackMethodZlib = 1;

	static bool FirstPartEquals(const std::filesystem::path& path, const char* expected)
	{
		for (const auto& part : path)
		{
			const std::string text = part.generic_string();
			if (text.empty() || text == ".")
				continue;
			return text == expected;
		}
		return false;
	}

	static bool IsSafePackEntryPath(const std::filesystem::path& path)
	{
		if (path.empty() || path.is_absolute() || !FirstPartEquals(path, "assets"))
			return false;

		for (const auto& part : path)
		{
			const std::string text = part.generic_string();
			if (text == "..")
				return false;
		}
		return true;
	}

	template<typename T>
	static bool ReadValue(std::istream& input, T* value)
	{
		input.read(reinterpret_cast<char*>(value), sizeof(T));
		return input.good();
	}

	static bool ReadBytes(std::istream& input, void* data, size_t size)
	{
		if (size == 0)
			return true;

		input.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size));
		return input.good();
	}

	static uint64_t HashString(const std::string& value)
	{
		uint64_t hash = 1469598103934665603ull;
		for (unsigned char c : value)
		{
			hash ^= c;
			hash *= 1099511628211ull;
		}
		return hash;
	}

	static std::string BuildPackFingerprint(const std::filesystem::path& packPath)
	{
		std::error_code error;
		const uint64_t size = static_cast<uint64_t>(std::filesystem::file_size(packPath, error));
		const auto writeTime = std::filesystem::last_write_time(packPath, error).time_since_epoch().count();

		std::ostringstream input;
		input << packPath.generic_string() << ':' << size << ':' << writeTime;

		std::ostringstream output;
		output << "wtpack_" << std::hex << HashString(input.str());
		return output.str();
	}

	static std::filesystem::path GetRuntimeCacheRoot()
	{
#ifdef WT_PLATFORM_WINDOWS
		if (const char* localAppData = std::getenv("LOCALAPPDATA"))
			return std::filesystem::path(localAppData) / "Wheatear" / "PackageCache";
#endif
		if (const char* home = std::getenv("HOME"))
			return std::filesystem::path(home) / ".cache" / "Wheatear" / "PackageCache";
		return std::filesystem::temp_directory_path() / "Wheatear" / "PackageCache";
	}

	static std::filesystem::path FindRuntimeAssetPack()
	{
		std::filesystem::path cursor = std::filesystem::current_path();
		while (!cursor.empty())
		{
			const std::filesystem::path directPack = cursor / kAssetPackFilename;
			if (std::filesystem::is_regular_file(directPack))
				return directPack;

			const std::filesystem::path gamePack = cursor / "assets" / "game" / kAssetPackFilename;
			if (std::filesystem::is_regular_file(gamePack))
				return gamePack;

			const std::filesystem::path parent = cursor.parent_path();
			if (parent == cursor)
				break;
			cursor = parent;
		}

		return {};
	}

	static bool WriteExtractedFile(const std::filesystem::path& outputPath,
		const unsigned char* bytes,
		size_t size)
	{
		std::error_code error;
		std::filesystem::create_directories(outputPath.parent_path(), error);
		if (error)
			return false;

		std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
		if (!output.is_open())
			return false;

		if (size > 0)
			output.write(reinterpret_cast<const char*>(bytes), static_cast<std::streamsize>(size));
		return output.good();
	}

	static bool ExtractAssetPack(const std::filesystem::path& packPath,
		const std::filesystem::path& cacheRoot)
	{
		std::ifstream input(packPath, std::ios::binary);
		if (!input.is_open())
			return false;

		char magic[sizeof(kAssetPackMagic)] = {};
		if (!ReadBytes(input, magic, sizeof(magic)) ||
			std::memcmp(magic, kAssetPackMagic, sizeof(kAssetPackMagic)) != 0)
		{
			WT_CORE_ERROR("Runtime asset pack has an invalid header: {}", packPath.string());
			return false;
		}

		uint32_t version = 0;
		uint32_t entryCount = 0;
		if (!ReadValue(input, &version) || !ReadValue(input, &entryCount) ||
			version != kAssetPackVersion)
		{
			WT_CORE_ERROR("Runtime asset pack version is not supported: {}", packPath.string());
			return false;
		}

		std::error_code error;
		std::filesystem::remove_all(cacheRoot, error);
		std::filesystem::create_directories(cacheRoot, error);
		if (error)
			return false;

		for (uint32_t i = 0; i < entryCount; ++i)
		{
			uint16_t pathLength = 0;
			uint32_t method = 0;
			uint64_t originalSize = 0;
			uint64_t storedSize = 0;
			if (!ReadValue(input, &pathLength) ||
				!ReadValue(input, &method) ||
				!ReadValue(input, &originalSize) ||
				!ReadValue(input, &storedSize))
			{
				return false;
			}

			std::string entryPath(pathLength, '\0');
			if (!ReadBytes(input, entryPath.data(), entryPath.size()))
				return false;

			std::replace(entryPath.begin(), entryPath.end(), '\\', '/');
			const std::filesystem::path relativePath = std::filesystem::path(entryPath).lexically_normal();
			if (!IsSafePackEntryPath(relativePath) ||
				storedSize > static_cast<uint64_t>(std::numeric_limits<size_t>::max()) ||
				originalSize > static_cast<uint64_t>(std::numeric_limits<int>::max()))
			{
				WT_CORE_ERROR("Runtime asset pack contains an invalid entry: {}", entryPath);
				return false;
			}

			std::vector<unsigned char> storedBytes(static_cast<size_t>(storedSize));
			if (!ReadBytes(input, storedBytes.data(), storedBytes.size()))
				return false;

			const std::filesystem::path outputPath = cacheRoot / relativePath;
			if (method == kAssetPackMethodStore)
			{
				if (originalSize != storedSize ||
					!WriteExtractedFile(outputPath, storedBytes.data(), storedBytes.size()))
				{
					return false;
				}
			}
			else if (method == kAssetPackMethodZlib)
			{
				int decodedSize = 0;
				char* decoded = stbi_zlib_decode_malloc(
					reinterpret_cast<const char*>(storedBytes.data()),
					static_cast<int>(storedBytes.size()),
					&decodedSize);

				const bool ok = decoded &&
					decodedSize == static_cast<int>(originalSize) &&
					WriteExtractedFile(outputPath,
						reinterpret_cast<const unsigned char*>(decoded),
						static_cast<size_t>(decodedSize));

				if (decoded)
					stbi_image_free(decoded);
				if (!ok)
					return false;
			}
			else
			{
				WT_CORE_ERROR("Runtime asset pack contains an unknown compression method: {}", method);
				return false;
			}
		}

		std::ofstream marker(cacheRoot / ".complete", std::ios::binary | std::ios::trunc);
		marker << packPath.generic_string();
		return marker.good();
	}

	static std::filesystem::path PreparePackagedAssetRoot()
	{
		const std::filesystem::path packPath = FindRuntimeAssetPack();
		if (packPath.empty())
			return {};

		const std::filesystem::path cacheRoot = GetRuntimeCacheRoot() / BuildPackFingerprint(packPath);
		const std::filesystem::path marker = cacheRoot / ".complete";
		if (std::filesystem::exists(marker) &&
			std::filesystem::exists(cacheRoot / "assets" / "shaders" / "Renderer2D_Quad.glsl"))
		{
			return cacheRoot;
		}

		if (!ExtractAssetPack(packPath, cacheRoot))
		{
			WT_CORE_ERROR("Failed to extract runtime asset pack: {}", packPath.string());
			return {};
		}

		WT_CORE_INFO("Runtime asset pack extracted to '{}'", cacheRoot.string());
		return cacheRoot;
	}

} // namespace

static bool ShouldEnableScripting(const Wheatear::ApplicationCommandLineArgs& args)
{
	for (int i = 1; i < args.Count; ++i)
	{
		const std::string argument = args[i];
		if (argument == "--no-scripts" || argument == "--disable-scripts")
			return false;
		if (argument == "--scripts" || argument == "--enable-scripts")
			return true;
	}

	return Wheatear::LoadRuntimePlayerConfig().EnableScripts;
}

static Wheatear::ApplicationSpecification CreateWheatearSandboxSpecification(
	const Wheatear::ApplicationCommandLineArgs& args)
{
	Wheatear::ApplicationSpecification specification;
	specification.Name = "Wheatear Sandbox";
	specification.CommandLineArgs = args;
	specification.EnableScripting = ShouldEnableScripting(args);
	const std::filesystem::path packagedAssetRoot = PreparePackagedAssetRoot();
	specification.ProjectRoot = packagedAssetRoot.empty()
		? Wheatear::AssetPath::DiscoverProjectRoot()
		: packagedAssetRoot;
	return specification;
}

class WheatearSandbox : public Wheatear::Application
{
public:
	WheatearSandbox(const Wheatear::ApplicationCommandLineArgs& args)
		: Wheatear::Application(CreateWheatearSandboxSpecification(args))
	{
		Wheatear::RegisterDefaultGameplayModules();
		PushLayer(new RuntimeSceneLayer());
	}

	~WheatearSandbox()
	{
	}
};

Wheatear::Application* Wheatear::CreateApplication(Wheatear::ApplicationCommandLineArgs args)
{
	return new WheatearSandbox(args);
}
