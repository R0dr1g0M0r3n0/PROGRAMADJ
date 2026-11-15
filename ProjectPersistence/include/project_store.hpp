#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace audioforge::project {

using Manifest = nlohmann::json;

struct ValidationResult {
    std::vector<std::string> errors;

    [[nodiscard]] bool is_valid() const noexcept { return errors.empty(); }
};

[[nodiscard]] ValidationResult validate_manifest(const Manifest& manifest);
[[nodiscard]] Manifest load_manifest(const std::filesystem::path& path);
void save_manifest(const std::filesystem::path& path, const Manifest& manifest);

}  // namespace audioforge::project
