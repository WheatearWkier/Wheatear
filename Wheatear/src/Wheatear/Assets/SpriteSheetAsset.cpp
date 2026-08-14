#include "wtpch.h"
#include "SpriteSheetAsset.h"

#include "Wheatear/Assets/AssetPath.h"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace Wheatear {

    namespace SpriteSheetAsset {

        namespace {

            struct CachedSheet
            {
                SpriteSheetData Data;
                Ref<Texture2D> Texture;
                std::filesystem::file_time_type WriteTime{};
                bool Loaded = false;
            };

            std::unordered_map<std::string, CachedSheet>& SheetCache()
            {
                static std::unordered_map<std::string, CachedSheet> cache;
                return cache;
            }

            const CachedSheet* GetCachedSheet(const std::string& sheetPath)
            {
                if (sheetPath.empty())
                    return nullptr;

                // Runtime data resolution handles both loose editor files and
                // the packaged content.wtpack (same convention as .vn/.wts).
                const std::filesystem::path resolved = AssetPath::ResolveRuntimeData(sheetPath);
                std::error_code error;
                const auto writeTime = std::filesystem::exists(resolved, error)
                    ? std::filesystem::last_write_time(resolved, error)
                    : std::filesystem::file_time_type{};

                CachedSheet& cached = SheetCache()[sheetPath];
                if (cached.Loaded && cached.WriteTime == writeTime)
                    return &cached;

                cached.Data = Load(resolved.generic_string());
                cached.Texture = cached.Data.TexturePath.empty()
                    ? nullptr : Texture2D::Create(cached.Data.TexturePath);
                cached.WriteTime = writeTime;
                cached.Loaded = true;
                return &cached;
            }

        } // namespace

        SpriteSheetData Load(const std::string& path)
        {
            SpriteSheetData data;
            try
            {
                const YAML::Node node = YAML::LoadFile(path);
                data.TexturePath = node["texture"].as<std::string>("");
                data.Columns = node["columns"].as<int>(1);
                data.Rows = node["rows"].as<int>(1);
            }
            catch (...)
            {
                // Malformed sheet: leave defaults so nothing crashes.
            }
            data.Columns = std::max(1, data.Columns);
            data.Rows = std::max(1, data.Rows);
            return data;
        }

        void Save(const std::string& path, const SpriteSheetData& data)
        {
            YAML::Emitter out;
            out << YAML::BeginMap;
            out << YAML::Key << "texture" << YAML::Value << data.TexturePath;
            out << YAML::Key << "columns" << YAML::Value << std::max(1, data.Columns);
            out << YAML::Key << "rows" << YAML::Value << std::max(1, data.Rows);
            out << YAML::EndMap;

            std::ofstream output(path);
            if (output.is_open())
                output << out.c_str();
        }

        int CellCount(const SpriteSheetData& data)
        {
            return std::max(1, data.Columns) * std::max(1, data.Rows);
        }

        bool IsValidCell(const SpriteSheetData& data, int cellIndex)
        {
            return cellIndex >= 0 && cellIndex < CellCount(data);
        }

        glm::vec2 CellUVMin(const SpriteSheetData& data, int cellIndex)
        {
            const int columns = std::max(1, data.Columns);
            const int rows = std::max(1, data.Rows);
            const int col = cellIndex % columns;
            const int row = cellIndex / columns;
            return { static_cast<float>(col) / columns,
                     static_cast<float>(row) / rows };
        }

        glm::vec2 CellUVMax(const SpriteSheetData& data, int cellIndex)
        {
            const int columns = std::max(1, data.Columns);
            const int rows = std::max(1, data.Rows);
            const int col = cellIndex % columns;
            const int row = cellIndex / columns;
            return { static_cast<float>(col + 1) / columns,
                     static_cast<float>(row + 1) / rows };
        }

        bool ResolveCell(const std::string& sheetPath, int cellIndex,
            Ref<Texture2D>& outTexture, glm::vec2& outUVMin, glm::vec2& outUVMax)
        {
            const CachedSheet* sheet = GetCachedSheet(sheetPath);
            if (!sheet || !sheet->Texture || !sheet->Loaded)
                return false;

            if (!IsValidCell(sheet->Data, cellIndex))
                return false;

            outTexture = sheet->Texture;
            outUVMin = CellUVMin(sheet->Data, cellIndex);
            outUVMax = CellUVMax(sheet->Data, cellIndex);
            return true;
        }

    } // namespace SpriteSheetAsset

} // namespace Wheatear
