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

	// The asset pack format is little-endian by design; read explicitly so
	// the reader is independent of host byte order.
	static bool ReadU16(std::istream& input, uint16_t* value)
	{
		char bytes[2] = {};
		input.read(bytes, sizeof(bytes));
		if (!input.good())
			return false;
		*value = static_cast<uint16_t>(
			static_cast<uint8_t>(bytes[0]) |
			(static_cast<uint16_t>(static_cast<uint8_t>(bytes[1])) << 8));
		return true;
	}

	static bool ReadU32(std::istream& input, uint32_t* value)
	{
		char bytes[4] = {};
		input.read(bytes, sizeof(bytes));
		if (!input.good())
			return false;
		*value = static_cast<uint32_t>(static_cast<uint8_t>(bytes[0]))
			| (static_cast<uint32_t>(static_cast<uint8_t>(bytes[1])) << 8)
			| (static_cast<uint32_t>(static_cast<uint8_t>(bytes[2])) << 16)
			| (static_cast<uint32_t>(static_cast<uint8_t>(bytes[3])) << 24);
		return true;
	}

	static bool ReadU64(std::istream& input, uint64_t* value)
	{
		char bytes[8] = {};
		input.read(bytes, sizeof(bytes));
		if (!input.good())
			return false;
		uint64_t result = 0;
		for (int i = 7; i >= 0; --i)
			result = (result << 8) | static_cast<uint8_t>(bytes[i]);
		*value = result;
		return true;
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

	static std::filesystem::path GetExecutableDirectory()
	{
#ifdef WT_PLATFORM_WINDOWS
		wchar_t buffer[MAX_PATH] = {};
		const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
		if (length > 0 && length < MAX_PATH)
			return std::filesystem::path(buffer).parent_path();
#endif
		return std::filesystem::current_path();
	}

	static std::filesystem::path GetRuntimeCacheRoot()
	{
		// Prefer a cache directory next to the executable so packaged games do
		// not consume the system drive; fall back to the user profile when the
		// install directory is not writable (e.g. Program Files).
		const std::filesystem::path exeCache = GetExecutableDirectory() / ".wheatear_cache";
		{
			std::error_code error;
			if (std::filesystem::create_directories(exeCache, error) || !error)
				return exeCache;
		}

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

		input.seekg(0, std::ios::end);
		const std::streampos fileSize = input.tellg();
		input.seekg(0, std::ios::beg);
		if (fileSize < 0)
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
		if (!ReadU32(input, &version) || !ReadU32(input, &entryCount) ||
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
			if (!ReadU16(input, &pathLength) ||
				!ReadU32(input, &method) ||
				!ReadU64(input, &originalSize) ||
				!ReadU64(input, &storedSize))
			{
				return false;
			}

			std::string entryPath(pathLength, '\0');
			if (!ReadBytes(input, entryPath.data(), entryPath.size()))
				return false;

			std::replace(entryPath.begin(), entryPath.end(), '\\', '/');
			const std::filesystem::path relativePath = std::filesystem::path(entryPath).lexically_normal();
			if (!IsSafePackEntryPath(relativePath) ||
				originalSize > static_cast<uint64_t>(std::numeric_limits<int>::max()))
			{
				WT_CORE_ERROR("Runtime asset pack contains an invalid entry: {}", entryPath);
				return false;
			}

			// A truncated or hand-tampered pack can claim an arbitrary
			// storedSize; only allocate what the file can actually deliver,
			// otherwise a bogus entry triggers a multi-GB allocation
			// (std::bad_alloc -> crash) before the read below can fail.
			const std::streampos position = input.tellg();
			const uint64_t remaining = (position >= 0 && fileSize > position)
				? static_cast<uint64_t>(fileSize - position) : 0;
			if (storedSize > remaining)
			{
				WT_CORE_ERROR("Runtime asset pack entry '{}' claims {} bytes but only {} remain in the pack.",
					entryPath, storedSize, remaining);
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
		marker << BuildPackFingerprint(packPath) << ':' << entryCount;
		return marker.good();
	}

	static std::filesystem::path PreparePackagedAssetRoot()
	{
		const std::filesystem::path packPath = FindRuntimeAssetPack();
		if (packPath.empty())
			return {};

		const std::string fingerprint = BuildPackFingerprint(packPath);
		const std::filesystem::path cacheRoot = GetRuntimeCacheRoot() / fingerprint;
		const std::filesystem::path marker = cacheRoot / ".complete";
		if (std::filesystem::exists(marker) &&
			std::filesystem::exists(cacheRoot / "assets" / "shaders" / "Renderer2D_Quad.glsl"))
		{
			// Trust the cache only when the marker names this exact pack
			// (fingerprint) and carries a sane entry count; a partially
			// deleted or stale cache re-extracts.
			std::ifstream markerInput(marker);
			std::string markerText;
			std::getline(markerInput, markerText);
			const size_t colon = markerText.find(':');
			const std::string markerFingerprint =
				colon == std::string::npos ? markerText : markerText.substr(0, colon);
			const bool saneCount = colon != std::string::npos
				&& markerText.size() > colon + 1
				&& std::all_of(markerText.begin() + static_cast<std::ptrdiff_t>(colon + 1),
					markerText.end(),
					[](unsigned char c) { return c >= '0' && c <= '9'; });
			if (markerFingerprint == fingerprint && saneCount)
				return cacheRoot;
		}

		if (!ExtractAssetPack(packPath, cacheRoot))
		{
			WT_CORE_ERROR("Failed to extract runtime asset pack: {}", packPath.string());
			return {};
		}

		// Prune stale fingerprint directories from previous packs so the
		// cache cannot grow unbounded across repacks.
		std::error_code error;
		for (const auto& entry : std::filesystem::directory_iterator(GetRuntimeCacheRoot(), error))
		{
			if (error)
				break;
			if (entry.is_directory() && entry.path().filename() != cacheRoot.filename())
				std::filesystem::remove_all(entry.path(), error);
		}

		WT_CORE_INFO("Runtime asset pack extracted to '{}'", cacheRoot.string());
		return cacheRoot;
	}

} // namespace

static std::filesystem::path ReadProjectArgument(const Wheatear::ApplicationCommandLineArgs& args)
{
	for (int i = 1; i < args.Count; ++i)
	{
		const std::string argument = args[i];
		const char* projectArgument = i + 1 < args.Count ? args[i + 1] : nullptr;
		if (argument == "--project" && i + 1 < args.Count
			&& projectArgument && projectArgument[0] != '\0'
			&& projectArgument[0] != '-')
		{
			return std::filesystem::path(projectArgument);
		}
	}
	return {};
}

// Default project when running loose from the engine repository (no wtpack,
// no --project): Projects/WheatearDemo, falling back to discovery.
static std::filesystem::path DefaultProjectRoot()
{
	const std::filesystem::path repositoryRoot = Wheatear::AssetPath::GetEngineRoot().parent_path();
	const std::filesystem::path demoProject = repositoryRoot / "Projects" / "WheatearDemo";
	if (std::filesystem::is_directory(demoProject / "assets"))
		return demoProject;
	return {};
}

static Wheatear::ApplicationSpecification CreateWheatearSandboxSpecification(
	const Wheatear::ApplicationCommandLineArgs& args)
{
	Wheatear::ApplicationSpecification specification;
	specification.Name = "Wheatear Sandbox";
	specification.CommandLineArgs = args;

	// --project <dir> runs any project's loose assets straight from the
	// engine repository (developer mode); otherwise the packaged wtpack
	// extraction path applies (packaged games, engine root = extracted root).
	const std::filesystem::path explicitProject = ReadProjectArgument(args);
	if (!explicitProject.empty())
	{
		specification.ProjectRoot = std::filesystem::absolute(explicitProject);
		Wheatear::AssetPath::SetEngineRoot(Wheatear::AssetPath::DiscoverProjectRoot());
		return specification;
	}

	const std::filesystem::path packagedAssetRoot = PreparePackagedAssetRoot();
	if (!packagedAssetRoot.empty())
	{
		specification.ProjectRoot = packagedAssetRoot;
		// Packaged games write saves/settings next to the executable (the
		// extracted cache is regenerated and must stay disposable).
		Wheatear::AssetPath::SetWritableRoot(GetExecutableDirectory());
		return specification;
	}

	const std::filesystem::path defaultProject = DefaultProjectRoot();
	specification.ProjectRoot = !defaultProject.empty()
		? defaultProject
		: Wheatear::AssetPath::DiscoverProjectRoot();
	return specification;
}

class WheatearSandbox : public Wheatear::Application
{
public:
	WheatearSandbox(const Wheatear::ApplicationCommandLineArgs& args)
		: Wheatear::Application(CreateWheatearSandboxSpecification(args))
	{
		Wheatear::RegisterDefaultGameplayModules();
		PushLayer(std::make_unique<RuntimeSceneLayer>());
	}

	~WheatearSandbox()
	{
	}
};

Wheatear::Application* Wheatear::CreateApplication(Wheatear::ApplicationCommandLineArgs args)
{
	return new WheatearSandbox(args);
}
