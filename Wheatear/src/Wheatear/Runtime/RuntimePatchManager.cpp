#include "wtpch.h"
#include "RuntimePatchManager.h"

#include "Wheatear/Core/Log.h"

#include <stb_image.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <functional>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#ifdef WT_PLATFORM_WINDOWS
    #include <bcrypt.h>
#endif

namespace Wheatear {

    namespace {

        constexpr char kAssetPackMagic[8] = { 'W', 'T', 'P', 'A', 'C', 'K', '1', '\0' };
        constexpr uint32_t kAssetPackVersion = 1;
        constexpr uint32_t kAssetPackMethodStore = 0;
        constexpr uint32_t kAssetPackMethodZlib = 1;

        static void SetError(std::string* errorMessage,
            const std::string& message)
        {
            if (errorMessage)
                *errorMessage = message;
        }

        static bool ReadBytes(std::istream& input,
            void* data,
            size_t size)
        {
            if (size == 0)
                return true;
            input.read(reinterpret_cast<char*>(data),
                static_cast<std::streamsize>(size));
            return input.good();
        }

        static bool ReadU16(std::istream& input, uint16_t* value)
        {
            char bytes[2] = {};
            if (!ReadBytes(input, bytes, sizeof(bytes)))
                return false;
            *value = static_cast<uint16_t>(
                static_cast<uint8_t>(bytes[0]) |
                (static_cast<uint16_t>(static_cast<uint8_t>(bytes[1])) << 8));
            return true;
        }

        static bool ReadU32(std::istream& input, uint32_t* value)
        {
            char bytes[4] = {};
            if (!ReadBytes(input, bytes, sizeof(bytes)))
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
            if (!ReadBytes(input, bytes, sizeof(bytes)))
                return false;

            uint64_t result = 0;
            for (int index = 7; index >= 0; --index)
                result = (result << 8) | static_cast<uint8_t>(bytes[index]);
            *value = result;
            return true;
        }

        static bool IsSafePackEntryPath(const std::filesystem::path& path)
        {
            if (path.empty() || path.is_absolute())
                return false;

            auto part = path.begin();
            if (part == path.end() || part->generic_string() != "assets")
                return false;

            for (const auto& component : path)
            {
                const std::string text = component.generic_string();
                if (text.empty() || text == "." || text == "..")
                    return false;
            }
            return true;
        }

        static bool IsSafeRelativePath(const std::filesystem::path& path)
        {
            if (path.empty() || path.is_absolute() ||
                path.has_root_name() || path.has_root_directory())
            {
                return false;
            }

            for (const auto& component : path)
            {
                const std::string text = component.generic_string();
                if (text.empty() || text == "." || text == "..")
                    return false;
            }
            return true;
        }

        static bool WriteExtractedFile(const std::filesystem::path& outputPath,
            const unsigned char* bytes,
            size_t size,
            std::string* errorMessage)
        {
            std::error_code error;
            std::filesystem::create_directories(outputPath.parent_path(), error);
            if (error)
            {
                SetError(errorMessage, error.message());
                return false;
            }

            std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
            if (!output.is_open())
            {
                SetError(errorMessage, "Could not write " + outputPath.string());
                return false;
            }

            if (size > 0)
                output.write(reinterpret_cast<const char*>(bytes),
                    static_cast<std::streamsize>(size));
            if (!output.good())
            {
                SetError(errorMessage, "Write failed for " + outputPath.string());
                return false;
            }
            return true;
        }

        static uint64_t HashString(const std::string& value)
        {
            uint64_t hash = 1469598103934665603ull;
            for (unsigned char character : value)
            {
                hash ^= character;
                hash *= 1099511628211ull;
            }
            return hash;
        }

        static void AddFileFingerprint(uint64_t* hash,
            const std::filesystem::path& path)
        {
            if (!hash)
                return;

            std::error_code error;
            const uintmax_t size = std::filesystem::file_size(path, error);
            error.clear();
            const auto writeTime = std::filesystem::last_write_time(path, error)
                .time_since_epoch().count();
            const std::string key = path.generic_string() + ":" +
                std::to_string(size) + ":" + std::to_string(writeTime);
            for (unsigned char character : key)
            {
                *hash ^= character;
                *hash *= 1099511628211ull;
            }
        }

        static void AddStringFingerprint(uint64_t* hash,
            const std::string& value)
        {
            if (!hash)
                return;
            for (unsigned char character : value)
            {
                *hash ^= character;
                *hash *= 1099511628211ull;
            }
            *hash ^= 0xff;
            *hash *= 1099511628211ull;
        }

        static bool IsPatchFile(const std::filesystem::path& path)
        {
            return path.extension().generic_string() == ".wtpatch";
        }

        static std::filesystem::path ResolvePatchPath(
            const std::filesystem::path& updatesRoot,
            const std::filesystem::path& activeRoot,
            const std::string& manifestPath)
        {
            const std::filesystem::path relative = std::filesystem::path(manifestPath).lexically_normal();
            if (!IsSafeRelativePath(relative))
                return {};

            const std::filesystem::path activeCandidate = activeRoot / relative;
            if (std::filesystem::is_regular_file(activeCandidate))
                return activeCandidate;

            const std::filesystem::path rootCandidate = updatesRoot / relative;
            if (std::filesystem::is_regular_file(rootCandidate))
                return rootCandidate;
            return {};
        }

        static std::string LowerAscii(std::string value)
        {
            for (char& character : value)
                character = static_cast<char>(
                    std::tolower(static_cast<unsigned char>(character)));
            return value;
        }

        static std::string TrimAscii(std::string value)
        {
            const auto isSpace = [](unsigned char character)
            {
                return std::isspace(character) != 0;
            };

            while (!value.empty() && isSpace(static_cast<unsigned char>(value.front())))
                value.erase(value.begin());
            while (!value.empty() && isSpace(static_cast<unsigned char>(value.back())))
                value.pop_back();
            return value;
        }

        static int HexValue(char character)
        {
            if (character >= '0' && character <= '9')
                return character - '0';
            if (character >= 'a' && character <= 'f')
                return character - 'a' + 10;
            if (character >= 'A' && character <= 'F')
                return character - 'A' + 10;
            return -1;
        }

        static int Base64Value(char character)
        {
            if (character >= 'A' && character <= 'Z')
                return character - 'A';
            if (character >= 'a' && character <= 'z')
                return character - 'a' + 26;
            if (character >= '0' && character <= '9')
                return character - '0' + 52;
            if (character == '+')
                return 62;
            if (character == '/')
                return 63;
            return -1;
        }

        static bool DecodeEncodedBytes(
            std::string text,
            std::vector<unsigned char>* bytes)
        {
            if (!bytes)
                return false;
            bytes->clear();

            text = TrimAscii(std::move(text));
            if (text.rfind("base64:", 0) == 0)
                text = text.substr(7);
            else if (text.rfind("hex:", 0) == 0)
                text = text.substr(4);

            std::string compact;
            compact.reserve(text.size());
            for (unsigned char character : text)
            {
                if (!std::isspace(character))
                    compact.push_back(static_cast<char>(character));
            }
            if (compact.empty())
                return false;

            bool looksHex = (compact.size() % 2) == 0;
            for (char character : compact)
            {
                if (HexValue(character) < 0)
                {
                    looksHex = false;
                    break;
                }
            }
            if (looksHex)
            {
                bytes->reserve(compact.size() / 2);
                for (size_t index = 0; index < compact.size(); index += 2)
                {
                    const int high = HexValue(compact[index]);
                    const int low = HexValue(compact[index + 1]);
                    bytes->push_back(static_cast<unsigned char>((high << 4) | low));
                }
                return true;
            }

            uint32_t accumulator = 0;
            int bits = -8;
            bool padding = false;
            size_t paddingCount = 0;
            for (char character : compact)
            {
                if (character == '=')
                {
                    padding = true;
                    ++paddingCount;
                    if (paddingCount > 2)
                        return false;
                    continue;
                }

                if (padding)
                    return false;

                const int value = Base64Value(character);
                if (value < 0)
                    return false;
                accumulator = (accumulator << 6) |
                    static_cast<uint32_t>(value);
                bits += 6;
                if (bits >= 0)
                {
                    bytes->push_back(static_cast<unsigned char>(
                        (accumulator >> bits) & 0xff));
                    bits -= 8;
                }
            }

            return !bytes->empty();
        }

        static bool ReadBinaryFile(
            const std::filesystem::path& path,
            std::vector<unsigned char>* bytes,
            std::string* errorMessage)
        {
            if (!bytes)
                return false;

            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
            {
                SetError(errorMessage,
                    "Could not open key file: " + path.string());
                return false;
            }

            input.seekg(0, std::ios::end);
            const std::streampos size = input.tellg();
            input.seekg(0, std::ios::beg);
            if (size <= 0)
            {
                SetError(errorMessage,
                    "Key file is empty: " + path.string());
                return false;
            }

            bytes->resize(static_cast<size_t>(size));
            if (!ReadBytes(input, bytes->data(), bytes->size()))
            {
                SetError(errorMessage,
                    "Could not read key file: " + path.string());
                return false;
            }
            return true;
        }

        static bool ComputeSha256(const std::filesystem::path& path,
            std::string* digest,
            std::string* errorMessage)
        {
            if (!digest)
                return false;

#ifdef WT_PLATFORM_WINDOWS
            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
            {
                SetError(errorMessage, "Could not open patch for hashing: " + path.string());
                return false;
            }

            BCRYPT_ALG_HANDLE algorithm = nullptr;
            BCRYPT_HASH_HANDLE hash = nullptr;
            DWORD objectSize = 0;
            DWORD resultSize = 0;
            NTSTATUS status = BCryptOpenAlgorithmProvider(
                &algorithm,
                BCRYPT_SHA256_ALGORITHM,
                nullptr,
                0);
            if (status != 0)
            {
                SetError(errorMessage, "Could not initialize SHA-256.");
                return false;
            }

            status = BCryptGetProperty(
                algorithm,
                BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(&objectSize),
                sizeof(objectSize),
                &resultSize,
                0);
            if (status != 0 || objectSize == 0)
            {
                BCryptCloseAlgorithmProvider(algorithm, 0);
                SetError(errorMessage, "Could not query SHA-256 state size.");
                return false;
            }

            std::vector<UCHAR> object(objectSize);
            status = BCryptCreateHash(
                algorithm,
                &hash,
                object.data(),
                objectSize,
                nullptr,
                0,
                0);
            if (status != 0)
            {
                BCryptCloseAlgorithmProvider(algorithm, 0);
                SetError(errorMessage, "Could not create SHA-256 hash.");
                return false;
            }

            std::array<char, 64 * 1024> buffer = {};
            while (input.good())
            {
                input.read(buffer.data(),
                    static_cast<std::streamsize>(buffer.size()));
                const std::streamsize count = input.gcount();
                if (count <= 0)
                    break;
                status = BCryptHashData(
                    hash,
                    reinterpret_cast<PUCHAR>(buffer.data()),
                    static_cast<ULONG>(count),
                    0);
                if (status != 0)
                    break;
            }

            std::array<UCHAR, 32> bytes = {};
            if (status == 0)
            {
                status = BCryptFinishHash(
                    hash,
                    bytes.data(),
                    static_cast<ULONG>(bytes.size()),
                    0);
            }

            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            if (status != 0)
            {
                SetError(errorMessage, "Could not compute SHA-256 for " + path.string());
                return false;
            }

            std::ostringstream output;
            output << std::hex << std::setfill('0');
            for (const UCHAR byte : bytes)
                output << std::setw(2) << static_cast<unsigned int>(byte);
            *digest = output.str();
            return true;
#else
            (void)path;
            SetError(errorMessage, "SHA-256 verification is unavailable on this platform.");
            return false;
#endif
        }

        static bool ComputeSha256Bytes(
            const std::string& input,
            std::array<unsigned char, 32>* digest,
            std::string* errorMessage)
        {
            if (!digest)
                return false;

#ifdef WT_PLATFORM_WINDOWS
            BCRYPT_ALG_HANDLE algorithm = nullptr;
            BCRYPT_HASH_HANDLE hash = nullptr;
            DWORD objectSize = 0;
            DWORD resultSize = 0;
            NTSTATUS status = BCryptOpenAlgorithmProvider(
                &algorithm,
                BCRYPT_SHA256_ALGORITHM,
                nullptr,
                0);
            if (status != 0)
            {
                SetError(errorMessage, "Could not initialize SHA-256.");
                return false;
            }

            status = BCryptGetProperty(
                algorithm,
                BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(&objectSize),
                sizeof(objectSize),
                &resultSize,
                0);
            if (status != 0 || objectSize == 0)
            {
                BCryptCloseAlgorithmProvider(algorithm, 0);
                SetError(errorMessage, "Could not query SHA-256 state size.");
                return false;
            }

            std::vector<UCHAR> object(objectSize);
            status = BCryptCreateHash(
                algorithm,
                &hash,
                object.data(),
                objectSize,
                nullptr,
                0,
                0);
            if (status == 0 && !input.empty())
            {
                status = BCryptHashData(
                    hash,
                    reinterpret_cast<PUCHAR>(
                        const_cast<char*>(input.data())),
                    static_cast<ULONG>(input.size()),
                    0);
            }
            if (status == 0)
            {
                status = BCryptFinishHash(
                    hash,
                    digest->data(),
                    static_cast<ULONG>(digest->size()),
                    0);
            }

            if (hash)
                BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            if (status != 0)
            {
                SetError(errorMessage, "Could not hash manifest signature payload.");
                return false;
            }
            return true;
#else
            (void)input;
            SetError(errorMessage,
                "SHA-256 signature verification is unavailable on this platform.");
            return false;
#endif
        }

        static void AppendCanonicalField(
            std::string* payload,
            const std::string& value)
        {
            if (!payload)
                return;
            *payload += std::to_string(value.size());
            *payload += ':';
            *payload += value;
            *payload += '\n';
        }

        static std::string JoinCanonicalList(
            const std::vector<std::string>& values)
        {
            std::string result;
            for (size_t index = 0; index < values.size(); ++index)
            {
                if (index > 0)
                    result += ',';
                result += values[index];
            }
            return result;
        }

        static std::string BuildManifestSignaturePayload(
            const std::string& product,
            const std::string& baseVersion,
            const std::string& contentVersion,
            const std::string& minimumRuntimeVersion,
            const std::vector<RuntimePatchDescriptor>& patches)
        {
            std::string payload = "WHEATEAR_PATCH_MANIFEST_V1\n";
            AppendCanonicalField(&payload, product);
            AppendCanonicalField(&payload, baseVersion);
            AppendCanonicalField(&payload, contentVersion);
            AppendCanonicalField(&payload, minimumRuntimeVersion);
            AppendCanonicalField(&payload, std::to_string(patches.size()));

            for (const RuntimePatchDescriptor& patch : patches)
            {
                AppendCanonicalField(&payload, patch.Id);
                AppendCanonicalField(&payload, patch.RelativePath);
                AppendCanonicalField(&payload, LowerAscii(patch.Sha256));
                AppendCanonicalField(
                    &payload,
                    patch.HasSize ? std::to_string(patch.Size) : "");
                AppendCanonicalField(
                    &payload,
                    std::to_string(patch.BehaviorSchema));
                AppendCanonicalField(
                    &payload,
                    JoinCanonicalList(patch.DependsOn));
                AppendCanonicalField(
                    &payload,
                    JoinCanonicalList(patch.DeletePaths));
            }
            return payload;
        }

        static bool VerifyEcdsaP256Signature(
            const std::string& payload,
            const std::vector<unsigned char>& publicKeyBytes,
            const std::vector<unsigned char>& signatureBytes,
            std::string* errorMessage)
        {
#ifdef WT_PLATFORM_WINDOWS
            if (publicKeyBytes.size() != 64)
            {
                SetError(errorMessage,
                    "ECDSA P-256 public key must contain 64 raw bytes.");
                return false;
            }
            if (signatureBytes.size() != 64)
            {
                SetError(errorMessage,
                    "ECDSA P-256 signature must contain 64 raw bytes.");
                return false;
            }

            std::array<unsigned char, 32> digest = {};
            if (!ComputeSha256Bytes(payload, &digest, errorMessage))
                return false;

            BCRYPT_ALG_HANDLE algorithm = nullptr;
            BCRYPT_KEY_HANDLE key = nullptr;
            NTSTATUS status = BCryptOpenAlgorithmProvider(
                &algorithm,
                BCRYPT_ECDSA_P256_ALGORITHM,
                nullptr,
                0);
            if (status != 0)
            {
                SetError(errorMessage,
                    "Could not initialize ECDSA P-256 verification.");
                return false;
            }

            std::vector<unsigned char> blob(
                sizeof(BCRYPT_ECCKEY_BLOB) + publicKeyBytes.size());
            auto* header = reinterpret_cast<BCRYPT_ECCKEY_BLOB*>(blob.data());
            header->dwMagic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC;
            header->cbKey = 32;
            std::memcpy(
                blob.data() + sizeof(BCRYPT_ECCKEY_BLOB),
                publicKeyBytes.data(),
                publicKeyBytes.size());

            status = BCryptImportKeyPair(
                algorithm,
                nullptr,
                BCRYPT_ECCPUBLIC_BLOB,
                &key,
                blob.data(),
                static_cast<ULONG>(blob.size()),
                0);
            if (status == 0)
            {
                status = BCryptVerifySignature(
                    key,
                    nullptr,
                    digest.data(),
                    static_cast<ULONG>(digest.size()),
                    const_cast<PUCHAR>(signatureBytes.data()),
                    static_cast<ULONG>(signatureBytes.size()),
                    0);
            }

            if (key)
                BCryptDestroyKey(key);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            if (status != 0)
            {
                SetError(errorMessage,
                    "ECDSA P-256 signature verification failed.");
                return false;
            }
            return true;
#else
            (void)payload;
            (void)publicKeyBytes;
            (void)signatureBytes;
            SetError(errorMessage,
                "ECDSA P-256 signature verification is unavailable on this platform.");
            return false;
#endif
        }

        static bool VerifyPatch(const RuntimePatchDescriptor& descriptor,
            std::string* errorMessage)
        {
            std::error_code error;
            if (!std::filesystem::is_regular_file(descriptor.PackPath, error))
            {
                SetError(errorMessage,
                    "Patch file is missing: " + descriptor.PackPath.string());
                return false;
            }

            if (descriptor.HasSize)
            {
                const uintmax_t actualSize =
                    std::filesystem::file_size(descriptor.PackPath, error);
                if (error || actualSize != descriptor.Size)
                {
                    SetError(errorMessage,
                        "Patch size mismatch: " + descriptor.PackPath.string());
                    return false;
                }
            }

            if (!descriptor.Sha256.empty())
            {
                if (descriptor.Sha256.size() != 64)
                {
                    SetError(errorMessage,
                        "Patch sha256 must contain 64 hexadecimal characters: " +
                        descriptor.Id);
                    return false;
                }

                std::string actual;
                if (!ComputeSha256(descriptor.PackPath, &actual, errorMessage))
                    return false;
                if (LowerAscii(actual) != LowerAscii(descriptor.Sha256))
                {
                    SetError(errorMessage,
                        "Patch sha256 mismatch: " + descriptor.Id);
                    return false;
                }
            }
            return true;
        }

        static std::vector<std::string> ReadStringList(
            const YAML::Node& node,
            std::initializer_list<const char*> keys)
        {
            for (const char* key : keys)
            {
                const YAML::Node value = node[key];
                if (!value)
                    continue;

                std::vector<std::string> result;
                if (value.IsScalar())
                {
                    const std::string item = value.as<std::string>("");
                    if (!item.empty())
                        result.push_back(item);
                    return result;
                }
                if (value.IsSequence())
                {
                    for (const YAML::Node& item : value)
                    {
                        if (item.IsScalar())
                        {
                            const std::string text = item.as<std::string>("");
                            if (!text.empty())
                                result.push_back(text);
                        }
                    }
                    return result;
                }
            }
            return {};
        }

        static bool OrderPatches(
            std::vector<RuntimePatchDescriptor>* patches,
            std::string* errorMessage)
        {
            if (!patches)
                return false;

            std::unordered_map<std::string, size_t> indices;
            for (size_t index = 0; index < patches->size(); ++index)
            {
                RuntimePatchDescriptor& patch = (*patches)[index];
                if (patch.Id.empty())
                    patch.Id = patch.PackPath.stem().string();
                if (!indices.emplace(patch.Id, index).second)
                {
                    SetError(errorMessage,
                        "Duplicate patch id: " + patch.Id);
                    return false;
                }
            }

            std::vector<uint8_t> state(patches->size(), 0);
            std::vector<RuntimePatchDescriptor> ordered;
            ordered.reserve(patches->size());
            std::function<bool(size_t)> visit = [&](size_t index)
            {
                if (state[index] == 2)
                    return true;
                if (state[index] == 1)
                {
                    SetError(errorMessage,
                        "Patch dependency cycle at '" + (*patches)[index].Id + "'.");
                    return false;
                }

                state[index] = 1;
                for (const std::string& dependency : (*patches)[index].DependsOn)
                {
                    const auto dependencyIt = indices.find(dependency);
                    if (dependencyIt == indices.end())
                    {
                        SetError(errorMessage,
                            "Patch '" + (*patches)[index].Id +
                            "' depends on missing patch '" + dependency + "'.");
                        return false;
                    }
                    if (!visit(dependencyIt->second))
                        return false;
                }
                state[index] = 2;
                ordered.push_back((*patches)[index]);
                return true;
            };

            for (size_t index = 0; index < patches->size(); ++index)
            {
                if (!visit(index))
                    return false;
            }
            *patches = std::move(ordered);
            return true;
        }

        static bool ParseManifestPatches(
            const std::filesystem::path& updatesRoot,
            const std::filesystem::path& activeRoot,
            const std::filesystem::path& manifestPath,
            std::vector<RuntimePatchDescriptor>* patches,
            std::string* errorMessage)
        {
            if (!patches || !std::filesystem::is_regular_file(manifestPath))
                return false;

            try
            {
                const YAML::Node root = YAML::LoadFile(manifestPath.string());
                const YAML::Node entries = root["patches"];
                if (!entries || !entries.IsSequence())
                {
                    SetError(errorMessage, "Patch manifest has no patches sequence.");
                    return false;
                }

                for (const YAML::Node& entry : entries)
                {
                    const std::string path = entry.IsScalar()
                        ? entry.as<std::string>("")
                        : entry["path"].as<std::string>("");
                    const std::filesystem::path resolved =
                        ResolvePatchPath(updatesRoot, activeRoot, path);
                    if (resolved.empty() || !IsPatchFile(resolved))
                    {
                        SetError(errorMessage,
                            "Patch manifest references an unsafe or missing pack: " +
                            path);
                        return false;
                    }

                    RuntimePatchDescriptor descriptor;
                    descriptor.PackPath = resolved;
                    descriptor.RelativePath =
                        std::filesystem::path(path).lexically_normal().generic_string();
                    descriptor.Id = entry.IsMap()
                        ? entry["id"].as<std::string>("")
                        : resolved.stem().string();
                    if (entry.IsMap())
                    {
                        descriptor.Sha256 = entry["sha256"].as<std::string>("");
                        if (entry["size"])
                        {
                            descriptor.Size = entry["size"].as<uint64_t>(0);
                            descriptor.HasSize = true;
                        }
                        descriptor.BehaviorSchema = entry["behaviorSchema"].as<uint32_t>(0);
                        descriptor.DependsOn = ReadStringList(
                            entry,
                            { "dependsOn", "dependencies" });

                        const std::vector<std::string> deletes = ReadStringList(
                            entry,
                            { "delete", "deletes", "deletePaths" });
                        for (const std::string& deletePath : deletes)
                        {
                            const std::filesystem::path relative =
                                std::filesystem::path(deletePath).lexically_normal();
                            if (!IsSafePackEntryPath(relative))
                            {
                                SetError(errorMessage,
                                    "Patch manifest contains an unsafe delete path: " +
                                    deletePath);
                                return false;
                            }
                            descriptor.DeletePaths.push_back(relative.generic_string());
                        }
                    }

                    std::string verifyError;
                    if (!VerifyPatch(descriptor, &verifyError))
                    {
                        SetError(errorMessage, verifyError);
                        return false;
                    }
                    patches->push_back(std::move(descriptor));
                }
                return OrderPatches(patches, errorMessage);
            }
            catch (const YAML::Exception& exception)
            {
                SetError(errorMessage,
                "Invalid patch manifest '" + manifestPath.string() +
                    "': " + exception.what());
                return false;
            }
        }

        static bool LoadManifestPatchSet(
            const std::filesystem::path& updatesRoot,
            const std::filesystem::path& activeRoot,
            const std::filesystem::path& manifestPath,
            RuntimePatchSet* result,
            std::string* errorMessage)
        {
            if (!result || !std::filesystem::is_regular_file(manifestPath))
            {
                SetError(errorMessage,
                    "Patch manifest is missing: " + manifestPath.string());
                return false;
            }

            result->ManifestPath = manifestPath;
            result->ManifestValid = false;
            result->SignatureRequired = false;
            result->SignatureVerified = false;
            result->Patches.clear();
            result->PackPaths.clear();
            result->DeletePaths.clear();

            try
            {
                const YAML::Node root = YAML::LoadFile(manifestPath.string());
                result->Product = root["product"].as<std::string>("");
                result->BaseVersion = root["baseVersion"].as<std::string>("");
                result->ContentVersion = root["contentVersion"].as<std::string>("");
                result->MinimumRuntimeVersion =
                    root["minimumRuntimeVersion"].as<std::string>("");
                result->SignatureRequired =
                    root["signatureRequired"].as<bool>(false);
            }
            catch (const YAML::Exception& exception)
            {
                SetError(errorMessage,
                    "Invalid patch manifest '" + manifestPath.string() +
                    "': " + exception.what());
                return false;
            }

            if (!ParseManifestPatches(
                updatesRoot,
                activeRoot,
                manifestPath,
                &result->Patches,
                errorMessage))
            {
                return false;
            }

            try
            {
                const YAML::Node root = YAML::LoadFile(manifestPath.string());
                const std::string signature =
                    root["signature"].as<std::string>("");
                const bool hasSignature = !TrimAscii(signature).empty();
                if (!hasSignature)
                {
                    if (result->SignatureRequired)
                    {
                        SetError(errorMessage,
                            "Patch manifest requires a signature but none was provided.");
                        return false;
                    }
                }
                else
                {
                    const std::string algorithm =
                        root["signatureAlgorithm"].as<std::string>(
                            "ECDSA-P256-SHA256");
                    if (algorithm != "ECDSA-P256-SHA256")
                    {
                        SetError(errorMessage,
                            "Unsupported patch signature algorithm: " + algorithm);
                        return false;
                    }

                    std::vector<unsigned char> publicKey;
                    const std::string publicKeyText =
                        root["publicKey"].as<std::string>("");
                    const std::string publicKeyFile =
                        root["publicKeyFile"].as<std::string>("");
                    if (!publicKeyText.empty())
                    {
                        if (!DecodeEncodedBytes(publicKeyText, &publicKey))
                        {
                            SetError(errorMessage,
                                "Patch manifest publicKey is not valid hex or base64.");
                            return false;
                        }
                    }
                    else if (!publicKeyFile.empty())
                    {
                        const std::filesystem::path relative =
                            std::filesystem::path(publicKeyFile).lexically_normal();
                        if (!IsSafeRelativePath(relative))
                        {
                            SetError(errorMessage,
                                "Patch manifest publicKeyFile is unsafe: " +
                                publicKeyFile);
                            return false;
                        }

                        const std::filesystem::path keyPath =
                            manifestPath.parent_path() / relative;
                        if (!ReadBinaryFile(keyPath, &publicKey, errorMessage))
                            return false;
                        if (publicKey.size() != 64)
                        {
                            const std::string encoded(
                                reinterpret_cast<const char*>(publicKey.data()),
                                publicKey.size());
                            std::vector<unsigned char> decoded;
                            if (DecodeEncodedBytes(encoded, &decoded))
                                publicKey = std::move(decoded);
                        }
                    }
                    else
                    {
                        SetError(errorMessage,
                            "Signed patch manifest has no publicKey or publicKeyFile.");
                        return false;
                    }

                    std::vector<unsigned char> signatureBytes;
                    if (!DecodeEncodedBytes(signature, &signatureBytes))
                    {
                        SetError(errorMessage,
                            "Patch manifest signature is not valid hex or base64.");
                        return false;
                    }

                    const std::string payload = BuildManifestSignaturePayload(
                        result->Product,
                        result->BaseVersion,
                        result->ContentVersion,
                        result->MinimumRuntimeVersion,
                        result->Patches);
                    if (!VerifyEcdsaP256Signature(
                        payload,
                        publicKey,
                        signatureBytes,
                        errorMessage))
                    {
                        return false;
                    }
                    result->SignatureVerified = true;
                }
            }
            catch (const YAML::Exception& exception)
            {
                SetError(errorMessage,
                    "Invalid patch signature metadata '" + manifestPath.string() +
                    "': " + exception.what());
                return false;
            }

            result->ManifestValid = true;
            for (const RuntimePatchDescriptor& patch : result->Patches)
            {
                result->PackPaths.push_back(patch.PackPath);
                result->DeletePaths.insert(
                    result->DeletePaths.end(),
                    patch.DeletePaths.begin(),
                    patch.DeletePaths.end());
            }
            return true;
        }

        static std::filesystem::path FindManifestFile(
            const std::filesystem::path& root)
        {
            const std::filesystem::path json = root / "manifest.json";
            if (std::filesystem::is_regular_file(json))
                return json;

            const std::filesystem::path yaml = root / "manifest.yaml";
            if (std::filesystem::is_regular_file(yaml))
                return yaml;
            return {};
        }

        static bool IsSafeVersionToken(const std::string& value)
        {
            if (value.empty() || value == "." || value == "..")
                return false;

            const std::filesystem::path path(value);
            if (path.has_root_name() ||
                path.has_root_directory() ||
                path.filename().generic_string() != value)
            {
                return false;
            }

            for (const char character : value)
            {
                if (std::isalnum(static_cast<unsigned char>(character)) ||
                    character == '.' ||
                    character == '-' ||
                    character == '_')
                {
                    continue;
                }
                return false;
            }
            return true;
        }

        static std::string MakeTimestampToken()
        {
            const auto now = std::chrono::system_clock::now();
            const auto milliseconds =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count();
            return std::to_string(milliseconds);
        }

        static bool ReadFirstLine(
            const std::filesystem::path& path,
            std::string* value)
        {
            if (!value)
                return false;
            std::ifstream input(path);
            if (!input.is_open())
                return false;
            std::getline(input, *value);
            *value = TrimAscii(*value);
            return true;
        }

        static bool WriteTextFile(
            const std::filesystem::path& path,
            const std::string& text,
            std::string* errorMessage)
        {
            std::error_code error;
            std::filesystem::create_directories(path.parent_path(), error);
            if (error)
            {
                SetError(errorMessage, error.message());
                return false;
            }

            const std::filesystem::path temporary =
                path.parent_path() / (path.filename().string() + ".tmp");
            {
                std::ofstream output(
                    temporary,
                    std::ios::binary | std::ios::trunc);
                if (!output.is_open())
                {
                    SetError(errorMessage,
                        "Could not write " + temporary.string());
                    return false;
                }
                output << text;
                if (!output.good())
                {
                    SetError(errorMessage,
                        "Write failed for " + temporary.string());
                    return false;
                }
            }

            std::filesystem::remove(path, error);
            error.clear();
            std::filesystem::rename(temporary, path, error);
            if (error)
            {
                SetError(errorMessage,
                    "Could not commit " + path.string() + ": " +
                    error.message());
                std::filesystem::remove(temporary, error);
                return false;
            }
            return true;
        }

        static bool ReadKeyValueLine(
            const std::filesystem::path& path,
            const std::string& key,
            std::string* value)
        {
            if (!value)
                return false;

            std::ifstream input(path);
            if (!input.is_open())
                return false;

            std::string line;
            while (std::getline(input, line))
            {
                const size_t separator = line.find('=');
                if (separator == std::string::npos)
                    continue;
                if (TrimAscii(line.substr(0, separator)) == key)
                {
                    *value = TrimAscii(line.substr(separator + 1));
                    return true;
                }
            }
            return false;
        }

    } // namespace

    RuntimePatchSet RuntimePatchManager::DiscoverActivePatches(
        const std::filesystem::path& executableDirectory)
    {
        RuntimePatchSet result;
        result.UpdatesRoot = executableDirectory / "updates";
        const std::filesystem::path activeRoot = result.UpdatesRoot / "active";
        if (!std::filesystem::is_directory(activeRoot))
            return result;

        const std::filesystem::path activeJsonManifest =
            activeRoot / "manifest.json";
        const std::filesystem::path activeYamlManifest =
            activeRoot / "manifest.yaml";
        const std::filesystem::path jsonManifest = result.UpdatesRoot / "manifest.json";
        const std::filesystem::path yamlManifest = result.UpdatesRoot / "manifest.yaml";
        if (std::filesystem::is_regular_file(activeJsonManifest))
            result.ManifestPath = activeJsonManifest;
        else if (std::filesystem::is_regular_file(activeYamlManifest))
            result.ManifestPath = activeYamlManifest;
        else if (std::filesystem::is_regular_file(jsonManifest))
            result.ManifestPath = jsonManifest;
        else if (std::filesystem::is_regular_file(yamlManifest))
            result.ManifestPath = yamlManifest;

        if (!result.ManifestPath.empty())
        {
            std::string manifestError;
            result.ManifestValid = LoadManifestPatchSet(
                result.UpdatesRoot,
                activeRoot,
                result.ManifestPath,
                &result,
                &manifestError);
            if (!result.ManifestValid)
            {
                WT_CORE_WARN(
                    "RuntimePatchManager: rejected patch manifest '{}': {}",
                    result.ManifestPath.string(),
                    manifestError);
                return result;
            }
            return result;
        }

        std::error_code error;
        for (const auto& entry :
            std::filesystem::directory_iterator(activeRoot, error))
        {
            if (error)
                break;
            if (entry.is_regular_file() && IsPatchFile(entry.path()))
            {
                RuntimePatchDescriptor descriptor;
                descriptor.Id = entry.path().stem().string();
                descriptor.RelativePath = entry.path().filename().generic_string();
                descriptor.PackPath = entry.path();
                result.Patches.push_back(std::move(descriptor));
            }
        }
        std::sort(result.Patches.begin(), result.Patches.end(),
            [](const RuntimePatchDescriptor& left,
                const RuntimePatchDescriptor& right)
            {
                return left.PackPath < right.PackPath;
            });
        for (const RuntimePatchDescriptor& patch : result.Patches)
            result.PackPaths.push_back(patch.PackPath);
        return result;
    }

    std::string RuntimePatchManager::BuildContentFingerprint(
        const std::filesystem::path& basePack,
        const RuntimePatchSet& patches)
    {
        uint64_t hash = 1469598103934665603ull;
        AddFileFingerprint(&hash, basePack);
        if (!patches.ManifestPath.empty())
            AddFileFingerprint(&hash, patches.ManifestPath);
        AddStringFingerprint(&hash, patches.Product);
        AddStringFingerprint(&hash, patches.BaseVersion);
        AddStringFingerprint(&hash, patches.ContentVersion);
        AddStringFingerprint(&hash, patches.MinimumRuntimeVersion);
        AddStringFingerprint(&hash, patches.SignatureRequired ? "signed" : "unsigned");
        AddStringFingerprint(&hash, patches.SignatureVerified ? "verified" : "unverified");
        for (const RuntimePatchDescriptor& patch : patches.Patches)
        {
            AddStringFingerprint(&hash, patch.Id);
            AddStringFingerprint(&hash, patch.RelativePath);
            AddFileFingerprint(&hash, patch.PackPath);
            AddStringFingerprint(&hash, patch.Sha256);
            for (const std::string& dependency : patch.DependsOn)
                AddStringFingerprint(&hash, dependency);
            for (const std::string& deletePath : patch.DeletePaths)
                AddStringFingerprint(&hash, deletePath);
        }

        std::ostringstream output;
        output << "wtpack_" << std::hex << hash;
        return output.str();
    }

    bool RuntimePatchManager::ApplyPackFile(
        const std::filesystem::path& packPath,
        const std::filesystem::path& targetRoot,
        bool clearTarget,
        std::string* errorMessage)
    {
        std::ifstream input(packPath, std::ios::binary);
        if (!input.is_open())
        {
            SetError(errorMessage, "Could not open pack " + packPath.string());
            return false;
        }

        input.seekg(0, std::ios::end);
        const std::streampos fileSize = input.tellg();
        input.seekg(0, std::ios::beg);
        if (fileSize < 0)
        {
            SetError(errorMessage, "Could not read pack size " + packPath.string());
            return false;
        }

        char magic[sizeof(kAssetPackMagic)] = {};
        uint32_t version = 0;
        uint32_t entryCount = 0;
        if (!ReadBytes(input, magic, sizeof(magic)) ||
            std::memcmp(magic, kAssetPackMagic, sizeof(kAssetPackMagic)) != 0 ||
            !ReadU32(input, &version) ||
            !ReadU32(input, &entryCount) ||
            version != kAssetPackVersion)
        {
            SetError(errorMessage, "Unsupported or invalid pack header: " + packPath.string());
            return false;
        }

        if (clearTarget)
        {
            std::error_code error;
            std::filesystem::remove_all(targetRoot, error);
            std::filesystem::create_directories(targetRoot, error);
            if (error)
            {
                SetError(errorMessage, error.message());
                return false;
            }
        }

        for (uint32_t index = 0; index < entryCount; ++index)
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
                SetError(errorMessage, "Truncated pack entry header.");
                return false;
            }

            std::string entryPath(pathLength, '\0');
            if (!ReadBytes(input, entryPath.data(), entryPath.size()))
            {
                SetError(errorMessage, "Truncated pack entry path.");
                return false;
            }

            std::replace(entryPath.begin(), entryPath.end(), '\\', '/');
            const std::filesystem::path relativePath =
                std::filesystem::path(entryPath).lexically_normal();
            if (!IsSafePackEntryPath(relativePath) ||
                originalSize > static_cast<uint64_t>(std::numeric_limits<int>::max()) ||
                storedSize > static_cast<uint64_t>(std::numeric_limits<int>::max()))
            {
                SetError(errorMessage, "Unsafe pack entry: " + entryPath);
                return false;
            }

            const std::streampos position = input.tellg();
            const uint64_t remaining = position >= 0 && fileSize > position
                ? static_cast<uint64_t>(fileSize - position)
                : 0;
            if (storedSize > remaining)
            {
                SetError(errorMessage, "Truncated payload for pack entry: " + entryPath);
                return false;
            }

            std::vector<unsigned char> storedBytes(static_cast<size_t>(storedSize));
            if (!ReadBytes(input, storedBytes.data(), storedBytes.size()))
            {
                SetError(errorMessage, "Could not read pack entry: " + entryPath);
                return false;
            }

            const std::filesystem::path outputPath = targetRoot / relativePath;
            if (std::filesystem::is_symlink(outputPath))
            {
                SetError(errorMessage,
                    "Refusing to overwrite symlinked pack entry: " +
                    outputPath.string());
                return false;
            }
            bool ok = false;
            if (method == kAssetPackMethodStore)
            {
                ok = originalSize == storedSize &&
                    WriteExtractedFile(outputPath,
                        storedBytes.data(),
                        storedBytes.size(),
                        errorMessage);
            }
            else if (method == kAssetPackMethodZlib)
            {
                int decodedSize = 0;
                char* decoded = stbi_zlib_decode_malloc(
                    reinterpret_cast<const char*>(storedBytes.data()),
                    static_cast<int>(storedBytes.size()),
                    &decodedSize);
                ok = decoded &&
                    decodedSize == static_cast<int>(originalSize) &&
                    WriteExtractedFile(
                        outputPath,
                        reinterpret_cast<const unsigned char*>(decoded),
                        static_cast<size_t>(decodedSize),
                        errorMessage);
                if (decoded)
                    stbi_image_free(decoded);
            }
            else
            {
                SetError(errorMessage, "Unknown compression method in " + packPath.string());
                return false;
            }

            if (!ok)
            {
                if (errorMessage && errorMessage->empty())
                    *errorMessage = "Could not extract pack entry: " + entryPath;
                return false;
            }
        }

        return true;
    }

    bool RuntimePatchManager::ApplyDeletePaths(
        const std::filesystem::path& targetRoot,
        const std::vector<std::string>& relativePaths,
        std::string* errorMessage)
    {
        for (const std::string& text : relativePaths)
        {
            const std::filesystem::path relative =
                std::filesystem::path(text).lexically_normal();
            if (!IsSafePackEntryPath(relative))
            {
                SetError(errorMessage, "Unsafe patch delete path: " + text);
                return false;
            }

            const std::filesystem::path outputPath = targetRoot / relative;
            if (std::filesystem::is_symlink(outputPath))
            {
                std::error_code removeError;
                std::filesystem::remove(outputPath, removeError);
                if (removeError)
                {
                    SetError(errorMessage,
                        "Could not remove patch symlink '" + outputPath.string() +
                        "': " + removeError.message());
                    return false;
                }
                continue;
            }

            std::error_code removeError;
            std::filesystem::remove_all(outputPath, removeError);
            if (removeError)
            {
                SetError(errorMessage,
                    "Could not delete patched asset '" + outputPath.string() +
                    "': " + removeError.message());
                return false;
            }
        }
        return true;
    }

    bool RuntimePatchManager::MaterializeRuntimeRoot(
        const std::filesystem::path& basePack,
        const RuntimePatchSet& patches,
        const std::filesystem::path& targetRoot,
        std::string* errorMessage)
    {
        if (!std::filesystem::is_regular_file(basePack))
        {
            SetError(errorMessage, "Base content pack is missing: " + basePack.string());
            return false;
        }
        if (!patches.ManifestValid)
        {
            SetError(errorMessage, "Active patch manifest was rejected.");
            return false;
        }

        const std::filesystem::path parent =
            targetRoot.parent_path().empty()
            ? std::filesystem::current_path()
            : targetRoot.parent_path();
        const std::string name = targetRoot.filename().string();
        if (name.empty())
        {
            SetError(errorMessage, "Runtime target root has no directory name.");
            return false;
        }

        const std::filesystem::path staging = parent / (name + ".staging");
        const std::filesystem::path backup = parent / (name + ".backup");
        std::error_code error;
        std::filesystem::create_directories(parent, error);
        if (error)
        {
            SetError(errorMessage, error.message());
            return false;
        }
        std::filesystem::remove_all(staging, error);
        if (error)
        {
            SetError(errorMessage, "Could not clear runtime staging: " + error.message());
            return false;
        }

        if (!ApplyPackFile(basePack, staging, true, errorMessage))
        {
            std::filesystem::remove_all(staging, error);
            return false;
        }

        for (const RuntimePatchDescriptor& patch : patches.Patches)
        {
            if (!VerifyPatch(patch, errorMessage) ||
                !ApplyPackFile(patch.PackPath, staging, false, errorMessage) ||
                !ApplyDeletePaths(staging, patch.DeletePaths, errorMessage))
            {
                std::filesystem::remove_all(staging, error);
                return false;
            }
        }

        std::filesystem::remove_all(backup, error);
        if (error)
        {
            SetError(errorMessage, "Could not clear runtime backup: " + error.message());
            std::filesystem::remove_all(staging, error);
            return false;
        }

        const bool hadTarget = std::filesystem::exists(targetRoot, error);
        if (error)
        {
            SetError(errorMessage, "Could not inspect runtime target: " + error.message());
            std::filesystem::remove_all(staging, error);
            return false;
        }

        if (hadTarget)
        {
            std::filesystem::rename(targetRoot, backup, error);
            if (error)
            {
                SetError(errorMessage,
                    "Could not move old runtime root to backup: " + error.message());
                std::filesystem::remove_all(staging, error);
                return false;
            }
        }

        std::filesystem::rename(staging, targetRoot, error);
        if (error)
        {
            SetError(errorMessage,
                "Could not commit runtime root: " + error.message());
            if (hadTarget)
            {
                std::error_code restoreError;
                std::filesystem::rename(backup, targetRoot, restoreError);
            }
            std::filesystem::remove_all(staging, error);
            return false;
        }

        std::filesystem::remove_all(backup, error);
        if (error)
        {
            WT_CORE_WARN(
                "RuntimePatchManager: old runtime backup could not be pruned: {}",
                error.message());
        }
        return true;
    }

    bool RuntimePatchManager::RecoverInterruptedActivation(
        const std::filesystem::path& executableDirectory,
        std::string* errorMessage)
    {
        const std::filesystem::path updatesRoot =
            executableDirectory / "updates";
        const std::filesystem::path pending =
            updatesRoot / "activation.pending";
        if (!std::filesystem::is_regular_file(pending))
            return true;

        const std::filesystem::path active = updatesRoot / "active";
        const std::filesystem::path backupRoot = updatesRoot / "backup";
        std::string backupName;
        ReadKeyValueLine(pending, "backup", &backupName);
        if (!backupName.empty() && !IsSafeVersionToken(backupName))
        {
            SetError(errorMessage,
                "Interrupted activation contains an unsafe backup name.");
            return false;
        }

        if (!std::filesystem::exists(active) && !backupName.empty())
        {
            const std::filesystem::path backup = backupRoot / backupName;
            if (!std::filesystem::exists(backup))
            {
                SetError(errorMessage,
                    "Interrupted activation lost its previous patch set.");
                return false;
            }

            std::error_code error;
            std::filesystem::rename(backup, active, error);
            if (error)
            {
                SetError(errorMessage,
                    "Could not restore interrupted patch activation: " +
                    error.message());
                return false;
            }
        }

        std::error_code error;
        std::filesystem::remove(pending, error);
        if (error)
        {
            SetError(errorMessage,
                "Could not clear activation recovery marker: " +
                error.message());
            return false;
        }
        return true;
    }

    bool RuntimePatchManager::ActivateStagedPatchSet(
        const std::filesystem::path& executableDirectory,
        const std::string& version,
        std::string* errorMessage)
    {
        if (!IsSafeVersionToken(version))
        {
            SetError(errorMessage,
                "Patch activation version is unsafe: " + version);
            return false;
        }

        if (!RecoverInterruptedActivation(executableDirectory, errorMessage))
            return false;

        const std::filesystem::path updatesRoot =
            executableDirectory / "updates";
        const std::filesystem::path stagingRoot =
            updatesRoot / "staging" / version;
        const std::filesystem::path activeRoot = updatesRoot / "active";
        const std::filesystem::path backupRoot = updatesRoot / "backup";
        const std::filesystem::path manifestPath =
            FindManifestFile(stagingRoot);
        if (!std::filesystem::is_directory(stagingRoot) ||
            manifestPath.empty())
        {
            SetError(errorMessage,
                "Staged patch set is incomplete: " + stagingRoot.string());
            return false;
        }

        RuntimePatchSet candidate;
        candidate.UpdatesRoot = updatesRoot;
        std::string manifestError;
        if (!LoadManifestPatchSet(
            stagingRoot,
            stagingRoot,
            manifestPath,
            &candidate,
            &manifestError))
        {
            SetError(errorMessage,
                "Staged patch set was rejected: " + manifestError);
            return false;
        }

        std::error_code error;
        std::filesystem::create_directories(backupRoot, error);
        if (error)
        {
            SetError(errorMessage,
                "Could not create patch backup directory: " +
                error.message());
            return false;
        }

        std::string previousVersion;
        ReadFirstLine(updatesRoot / "active_version", &previousVersion);
        std::string backupName = previousVersion;
        if (backupName.empty() || !IsSafeVersionToken(backupName))
            backupName = "previous-" + MakeTimestampToken();

        std::filesystem::path backupPath = backupRoot / backupName;
        if (std::filesystem::exists(backupPath))
        {
            backupName += "-" + MakeTimestampToken();
            backupPath = backupRoot / backupName;
        }

        const std::filesystem::path pending =
            updatesRoot / "activation.pending";
        if (!WriteTextFile(
            pending,
            "backup=" + (std::filesystem::exists(activeRoot)
                ? backupName
                : std::string()) + "\n"
            "staging=" + version + "\n",
            errorMessage))
        {
            return false;
        }

        const bool hadActive = std::filesystem::exists(activeRoot);
        if (hadActive)
        {
            if (std::filesystem::is_symlink(activeRoot))
            {
                SetError(errorMessage,
                    "Refusing to activate over symlinked active patch directory.");
                std::filesystem::remove(pending, error);
                return false;
            }

            std::filesystem::rename(activeRoot, backupPath, error);
            if (error)
            {
                SetError(errorMessage,
                    "Could not preserve active patch set: " +
                    error.message());
                std::filesystem::remove(pending, error);
                return false;
            }
        }

        std::filesystem::rename(stagingRoot, activeRoot, error);
        if (error)
        {
            if (hadActive)
            {
                std::error_code restoreError;
                std::filesystem::rename(backupPath, activeRoot, restoreError);
            }
            SetError(errorMessage,
                "Could not activate staged patch set: " + error.message());
            std::filesystem::remove(pending, error);
            return false;
        }

        const std::string activeVersion =
            candidate.ContentVersion.empty()
            ? version
            : candidate.ContentVersion;
        if (!WriteTextFile(
            updatesRoot / "active_version",
            activeVersion + "\n",
            errorMessage))
        {
            WT_CORE_WARN(
                "RuntimePatchManager: active_version marker could not be updated.");
        }
        if (hadActive)
        {
            if (!WriteTextFile(
                updatesRoot / "previous_version",
                backupName + "\n",
                errorMessage))
            {
                WT_CORE_WARN(
                    "RuntimePatchManager: previous_version marker could not be updated.");
            }
        }

        std::filesystem::remove(pending, error);
        if (error)
        {
            WT_CORE_WARN(
                "RuntimePatchManager: activation marker could not be removed: {}",
                error.message());
        }
        return true;
    }

    bool RuntimePatchManager::RollbackActivePatches(
        const std::filesystem::path& executableDirectory,
        std::string* errorMessage)
    {
        if (!RecoverInterruptedActivation(executableDirectory, errorMessage))
            return false;

        const std::filesystem::path updatesRoot =
            executableDirectory / "updates";
        const std::filesystem::path activeRoot = updatesRoot / "active";
        const std::filesystem::path backupRoot = updatesRoot / "backup";

        std::string previousVersion;
        ReadFirstLine(updatesRoot / "previous_version", &previousVersion);
        if (!previousVersion.empty() && !IsSafeVersionToken(previousVersion))
        {
            SetError(errorMessage,
                "Previous patch version marker is unsafe.");
            return false;
        }

        std::filesystem::path previousRoot;
        if (!previousVersion.empty())
            previousRoot = backupRoot / previousVersion;

        if (previousRoot.empty() || !std::filesystem::is_directory(previousRoot))
        {
            std::vector<std::filesystem::path> candidates;
            std::error_code iterateError;
            if (std::filesystem::is_directory(backupRoot))
            {
                for (const auto& entry :
                    std::filesystem::directory_iterator(backupRoot, iterateError))
                {
                    if (iterateError)
                        break;
                    if (entry.is_directory() && !entry.is_symlink())
                        candidates.push_back(entry.path());
                }
            }
            if (!candidates.empty())
            {
                std::sort(candidates.begin(), candidates.end(),
                    [](const std::filesystem::path& left,
                        const std::filesystem::path& right)
                    {
                        std::error_code leftError;
                        std::error_code rightError;
                        return std::filesystem::last_write_time(left, leftError) >
                            std::filesystem::last_write_time(right, rightError);
                    });
                previousRoot = candidates.front();
                previousVersion = previousRoot.filename().string();
            }
        }

        if (previousRoot.empty() || !std::filesystem::is_directory(previousRoot))
        {
            SetError(errorMessage,
                "No previous patch set is available for rollback.");
            return false;
        }

        const std::string failedName = "failed-" + MakeTimestampToken();
        const std::filesystem::path failedRoot = backupRoot / failedName;
        std::error_code error;
        if (std::filesystem::exists(activeRoot))
        {
            if (std::filesystem::is_symlink(activeRoot))
            {
                SetError(errorMessage,
                    "Refusing to roll back through symlinked active directory.");
                return false;
            }
            std::filesystem::rename(activeRoot, failedRoot, error);
            if (error)
            {
                SetError(errorMessage,
                    "Could not preserve active patch set for rollback: " +
                    error.message());
                return false;
            }
        }

        std::filesystem::rename(previousRoot, activeRoot, error);
        if (error)
        {
            if (std::filesystem::exists(failedRoot))
            {
                std::error_code restoreError;
                std::filesystem::rename(failedRoot, activeRoot, restoreError);
            }
            SetError(errorMessage,
                "Could not restore previous patch set: " + error.message());
            return false;
        }

        std::string activeVersion = previousVersion;
        const std::filesystem::path manifest = FindManifestFile(activeRoot);
        if (!manifest.empty())
        {
            try
            {
                const YAML::Node root = YAML::LoadFile(manifest.string());
                const std::string contentVersion =
                    root["contentVersion"].as<std::string>("");
                if (!contentVersion.empty())
                    activeVersion = contentVersion;
            }
            catch (const YAML::Exception&)
            {
                // The next normal discovery will report the manifest error.
            }
        }
        WriteTextFile(
            updatesRoot / "active_version",
            activeVersion + "\n",
            errorMessage);
        WriteTextFile(updatesRoot / "previous_version", "\n", errorMessage);
        return true;
    }

    bool RuntimePatchManager::RecoverFailedLaunch(
        const std::filesystem::path& executableDirectory,
        std::string* errorMessage)
    {
        const std::filesystem::path state =
            executableDirectory / "updates" / "launch_state.txt";
        if (!std::filesystem::is_regular_file(state))
            return true;

        std::string pending;
        std::string failuresText;
        ReadKeyValueLine(state, "pending", &pending);
        ReadKeyValueLine(state, "failures", &failuresText);
        int failures = 0;
        try
        {
            failures = std::max(0, std::stoi(failuresText));
        }
        catch (...)
        {
            failures = 0;
        }

        if (pending != "1" || failures < 2)
            return true;

        if (!RollbackActivePatches(executableDirectory, errorMessage))
            return false;

        std::error_code error;
        std::filesystem::remove(state, error);
        if (error)
        {
            SetError(errorMessage,
                "Could not clear failed launch marker: " + error.message());
            return false;
        }
        return true;
    }

    bool RuntimePatchManager::BeginLaunch(
        const std::filesystem::path& executableDirectory,
        const std::string& contentFingerprint,
        std::string* errorMessage)
    {
        const std::filesystem::path updatesRoot =
            executableDirectory / "updates";
        const std::filesystem::path state =
            updatesRoot / "launch_state.txt";

        std::string pending;
        std::string failuresText;
        ReadKeyValueLine(state, "pending", &pending);
        ReadKeyValueLine(state, "failures", &failuresText);
        int failures = 0;
        try
        {
            failures = std::max(0, std::stoi(failuresText));
        }
        catch (...)
        {
            failures = 0;
        }
        if (pending != "1")
            failures = 1;
        else
            failures = std::max(1, failures + 1);

        return WriteTextFile(
            state,
            "pending=1\n"
            "failures=" + std::to_string(failures) + "\n"
            "fingerprint=" + contentFingerprint + "\n",
            errorMessage);
    }

    bool RuntimePatchManager::MarkLaunchHealthy(
        const std::filesystem::path& executableDirectory,
        std::string* errorMessage)
    {
        const std::filesystem::path state =
            executableDirectory / "updates" / "launch_state.txt";
        if (!std::filesystem::exists(state))
            return true;

        std::error_code error;
        std::filesystem::remove(state, error);
        if (error)
        {
            SetError(errorMessage,
                "Could not clear healthy launch marker: " + error.message());
            return false;
        }
        return true;
    }

} // namespace Wheatear
