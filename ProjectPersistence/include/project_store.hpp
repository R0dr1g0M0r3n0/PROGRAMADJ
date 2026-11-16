#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace audioforge::project {

using Manifest = nlohmann::json;

struct ValidationResult {
    std::vector<std::string> errors;

    [[nodiscard]] bool is_valid() const noexcept { return errors.empty(); }
};

struct AssetLink {
    std::string id;
    std::string original_path;
    std::string sha256;
    std::optional<std::uint64_t> byte_size;
    std::string root_source_asset_id;
    std::optional<std::string> parent_asset_id;
    std::optional<std::string> relative_path;
};

[[nodiscard]] ValidationResult validate_manifest(
    const Manifest& manifest, const std::filesystem::path& project_directory = {});
[[nodiscard]] std::optional<AssetLink> resolve_asset_link(
    const Manifest& manifest, const std::string& asset_id);
[[nodiscard]] Manifest migrate_manifest(const Manifest& manifest);
[[nodiscard]] Manifest load_manifest(const std::filesystem::path& path);
void save_manifest(const std::filesystem::path& path, const Manifest& manifest);

}  // namespace audioforge::project
