#include "project_store.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using audioforge::project::Manifest;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string read_text(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void write_text(const fs::path& path, const std::string& value) {
    std::ofstream output(path, std::ios::binary);
    output << value;
}

Manifest fixture() {
    std::ifstream input(PROJECT_MANIFEST_FIXTURE);
    return Manifest::parse(input);
}

void test_round_trip_and_references() {
    const auto source = fixture();
    const auto validation = audioforge::project::validate_manifest(source);
    require(validation.is_valid(), "synthetic v1 fixture must satisfy semantic invariants");

    const auto directory = fs::temp_directory_path() / "audioforge-project-persistence-roundtrip";
    fs::remove_all(directory);
    fs::create_directories(directory);
    const auto manifest_path = directory / "project.json";
    audioforge::project::save_manifest(manifest_path, source);
    const auto loaded = audioforge::project::load_manifest(manifest_path);
    require(loaded == source, "manifest fields, identifiers, references, and metadata must round-trip");

    auto invalid = source;
    invalid["tracks"][0]["activeAssetId"] = "00000000-0000-4000-8000-ffffffffffff";
    require(!audioforge::project::validate_manifest(invalid).is_valid(),
            "dangling active asset reference must be rejected");
    invalid = source;
    invalid["regions"][0]["span"]["endFrame"] = invalid["regions"][0]["span"]["startFrame"];
    require(!audioforge::project::validate_manifest(invalid).is_valid(),
            "empty frame span must be rejected");
    invalid = source;
    invalid["sourceAssets"][0].erase("sha256");
    require(!audioforge::project::validate_manifest(invalid).is_valid(),
            "manifest records missing required v1 fields must be rejected");
    fs::remove_all(directory);
}

void test_original_files_and_atomic_backup() {
    const auto directory = fs::temp_directory_path() / "audioforge-project-persistence-atomic";
    fs::remove_all(directory);
    fs::create_directories(directory);
    const auto original_path = directory / "original.wav";
    const std::string original_bytes("original-audio\0bytes", 20);
    write_text(original_path, original_bytes);

    auto manifest = fixture();
    manifest["sourceAssets"][0]["originalPath"] = original_path.string();
    const auto manifest_path = directory / "project.json";
    audioforge::project::save_manifest(manifest_path, manifest);
    require(read_text(original_path) == original_bytes, "source audio must remain byte-for-byte unchanged");

    const auto previous_manifest = read_text(manifest_path);
    manifest["tracks"][0]["userMetadata"]["title"] = "Updated metadata";
    audioforge::project::save_manifest(manifest_path, manifest);
    require(read_text(manifest_path) != previous_manifest, "successful save must replace the manifest");
    require(read_text(manifest_path.string() + ".bak") == previous_manifest,
            "replacement must retain the prior manifest as a recoverable backup");
    require(audioforge::project::load_manifest(manifest_path) == manifest,
            "replacement manifest must be readable after atomic save");

    const auto current_manifest = read_text(manifest_path);
    auto invalid = manifest;
    invalid["tracks"][0]["sourceAssetId"] = "missing";
    bool rejected = false;
    try {
        audioforge::project::save_manifest(manifest_path, invalid);
    } catch (const std::exception&) {
        rejected = true;
    }
    require(rejected, "invalid manifest must not be saved");
    require(read_text(manifest_path) == current_manifest,
            "failed save must preserve the original manifest bytes");
    require(read_text(original_path) == original_bytes, "failed save must not modify source audio");
    fs::remove_all(directory);
}

}  // namespace

int main() {
    try {
        test_round_trip_and_references();
        test_original_files_and_atomic_backup();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
