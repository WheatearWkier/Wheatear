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

                if (auto trims = node["trims"])
                {
                    for (auto t : trims)
                    {
                        SpriteSheetData::CellTrim trim;
                        trim.Left = t["left"].as<int>(0);
                        trim.Top = t["top"].as<int>(0);
                        trim.Width = t["width"].as<int>(0);
                        trim.Height = t["height"].as<int>(0);
                        if (auto c = t["collider"])
                        {
                            trim.HasCollider = true;
                            trim.ColliderLeft = c["left"].as<int>(0);
                            trim.ColliderTop = c["top"].as<int>(0);
                            trim.ColliderWidth = c["width"].as<int>(0);
                            trim.ColliderHeight = c["height"].as<int>(0);
                        }
                        data.Trims.push_back(trim);
                    }
                }
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
            if (!data.Trims.empty())
            {
                out << YAML::Key << "trims" << YAML::Value << YAML::BeginSeq;
                for (const auto& t : data.Trims)
                {
                    out << YAML::BeginMap;
                    out << YAML::Key << "left" << YAML::Value << t.Left;
                    out << YAML::Key << "top" << YAML::Value << t.Top;
                    out << YAML::Key << "width" << YAML::Value << t.Width;
                    out << YAML::Key << "height" << YAML::Value << t.Height;
                    if (t.HasCollider)
                    {
                        out << YAML::Key << "collider" << YAML::Value << YAML::BeginMap;
                        out << YAML::Key << "left" << YAML::Value << t.ColliderLeft;
                        out << YAML::Key << "top" << YAML::Value << t.ColliderTop;
                        out << YAML::Key << "width" << YAML::Value << t.ColliderWidth;
                        out << YAML::Key << "height" << YAML::Value << t.ColliderHeight;
                        out << YAML::EndMap;
                    }
                    out << YAML::EndMap;
                }
                out << YAML::EndSeq;
            }
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
            // Renderer UV convention (v=0 = texture bottom, matching the
            // legacy picker and SpriteRenderer component): row 0 = image top.
            return { static_cast<float>(col) / columns,
                     1.0f - static_cast<float>(row + 1) / rows };
        }

        glm::vec2 CellUVMax(const SpriteSheetData& data, int cellIndex)
        {
            const int columns = std::max(1, data.Columns);
            const int rows = std::max(1, data.Rows);
            const int col = cellIndex % columns;
            const int row = cellIndex / columns;
            return { static_cast<float>(col + 1) / columns,
                     1.0f - static_cast<float>(row) / rows };
        }

        bool ResolveCell(const std::string& sheetPath, int cellIndex, ResolvedCell& out)
        {
            const CachedSheet* sheet = GetCachedSheet(sheetPath);
            if (!sheet || !sheet->Texture || !sheet->Loaded)
                return false;

            if (!IsValidCell(sheet->Data, cellIndex))
                return false;

            const SpriteSheetData& data = sheet->Data;
            const int columns = std::max(1, data.Columns);
            const int rows = std::max(1, data.Rows);
            const int texWidth = sheet->Texture->GetWidth();
            const int texHeight = sheet->Texture->GetHeight();
            const int cellWidth = std::max(1, texWidth / columns);
            const int cellHeight = std::max(1, texHeight / rows);
            const int col = cellIndex % columns;
            const int row = cellIndex / columns;

            // Content rect defaults to the full cell; a per-cell trim narrows
            // it (clamped inside the cell, never empty).
            int left = col * cellWidth;
            int top = row * cellHeight;
            int width = cellWidth;
            int height = cellHeight;

            const SpriteSheetData::CellTrim* trim = nullptr;
            if (cellIndex < static_cast<int>(data.Trims.size()))
            {
                const auto& t = data.Trims[cellIndex];
                if (t.Width > 0 && t.Height > 0)
                    trim = &t;
            }
            if (trim)
            {
                left = std::clamp(col * cellWidth + trim->Left, col * cellWidth, col * cellWidth + cellWidth - 1);
                top = std::clamp(row * cellHeight + trim->Top, row * cellHeight, row * cellHeight + cellHeight - 1);
                width = std::clamp(trim->Width, 1, col * cellWidth + cellWidth - left);
                height = std::clamp(trim->Height, 1, row * cellHeight + cellHeight - top);
            }

            out.Texture = sheet->Texture;
            // Renderer UV convention (v=0 = texture bottom): the top-left
            // content rect maps to uvMin = (left, 1 - bottom), uvMax = (right, 1 - top).
            out.UVMin = { static_cast<float>(left) / texWidth,
                          1.0f - static_cast<float>(top + height) / texHeight };
            out.UVMax = { static_cast<float>(left + width) / texWidth,
                          1.0f - static_cast<float>(top) / texHeight };
            out.ContentX = left;
            out.ContentY = top;
            out.ContentWidth = width;
            out.ContentHeight = height;

            if (trim && trim->HasCollider && trim->ColliderWidth > 0 && trim->ColliderHeight > 0)
            {
                out.HasCollider = true;
                out.ColliderLeft = std::clamp(left + trim->ColliderLeft, left, left + width - 1);
                out.ColliderTop = std::clamp(top + trim->ColliderTop, top, top + height - 1);
                out.ColliderWidth = std::clamp(trim->ColliderWidth, 1, left + width - out.ColliderLeft);
                out.ColliderHeight = std::clamp(trim->ColliderHeight, 1, top + height - out.ColliderTop);
            }

            return true;
        }

    } // namespace SpriteSheetAsset

} // namespace Wheatear
