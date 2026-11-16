#include "project_store.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include <csignal>
#include <sys/resource.h>

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
    const auto temporary_sentinel = directory / "temporary-sentinel.txt";
    write_text(temporary_sentinel, "sentinel bytes");
    fs::create_symlink(temporary_sentinel, manifest_path.string() + ".tmp.0");
    audioforge::project::save_manifest(manifest_path, source);
    require(read_text(temporary_sentinel) == "sentinel bytes",
            "save must not follow a preexisting predictable temporary symlink");
    require(fs::is_symlink(manifest_path.string() + ".tmp.0"), "save must not replace an unowned temporary symlink");
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
    invalid = source;
    invalid["sourceAssets"][0]["sha256"] = "not-a-digest";
    require(!audioforge::project::validate_manifest(invalid).is_valid(), "invalid digest must be rejected");
    invalid = source;
    invalid["sourceAssets"][0]["kind"] = "unknown";
    require(!audioforge::project::validate_manifest(invalid).is_valid(), "invalid enum must be rejected");
    invalid = source;
    invalid["sourceAssets"][0]["extra"] = true;
    require(!audioforge::project::validate_manifest(invalid).is_valid(), "schema disallows extra properties");
    invalid = source;
    invalid["tracks"][0]["userMetadata"] = "wrong type";
    require(!audioforge::project::validate_manifest(invalid).is_valid(), "invalid field type must be rejected");
    invalid = source;
    invalid["derivedAssets"][0]["relativePath"] = "../outside.wav";
    require(!audioforge::project::validate_manifest(invalid).is_valid(), "derived path traversal must be rejected");
    invalid = source;
    invalid["derivedAssets"][0]["relativePath"] = "other/file.wav";
    require(!audioforge::project::validate_manifest(invalid).is_valid(), "derived path must be under derived/");
    invalid = source;
    invalid["history"][0]["inputAssetIds"][0] = source["sourceAssets"][1]["id"];
    require(!audioforge::project::validate_manifest(invalid).is_valid(),
            "history input must belong to the referenced track lineage");
    invalid = source;
    invalid["derivedAssets"][0]["rootSourceAssetId"] = source["sourceAssets"][1]["id"];
    invalid["derivedAssets"][0]["parentAssetId"] = source["sourceAssets"][1]["id"];
    require(!audioforge::project::validate_manifest(invalid).is_valid(), "derived root source must be audio");
    invalid = source;
    invalid["sourceAssets"].push_back({
        {"id", "00000000-0000-4000-8000-000000000016"}, {"kind", "audio"},
        {"originalPath", "/synthetic/audio/second.wav"},
        {"sha256", "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"},
        {"importedAt", "2026-01-01T12:00:00.000Z"}});
    invalid["tracks"].push_back({
        {"id", "00000000-0000-4000-8000-000000000017"},
        {"sourceAssetId", "00000000-0000-4000-8000-000000000016"},
        {"activeAssetId", "00000000-0000-4000-8000-000000000016"},
        {"createdAt", "2026-01-01T12:00:00.000Z"}, {"userMetadata", Manifest::object()}});
    invalid["history"].push_back({
        {"id", "00000000-0000-4000-8000-000000000018"},
        {"trackId", "00000000-0000-4000-8000-000000000017"},
        {"at", "2026-01-01T12:01:00.000Z"}, {"operation", "invalid-output"},
        {"target", {{"kind", "track"}, {"id", "00000000-0000-4000-8000-000000000017"}}},
        {"inputAssetIds", {"00000000-0000-4000-8000-000000000016"}},
        {"outputAssetId", "00000000-0000-4000-8000-000000000003"}});
    require(!audioforge::project::validate_manifest(invalid).is_valid(),
            "history output must belong to the referenced track lineage");

    const auto project_directory = fs::temp_directory_path() / "audioforge-project-persistence-symlink";
    fs::remove_all(project_directory);
    fs::create_directories(project_directory / "derived");
    const auto outside_file = project_directory / "outside.wav";
    write_text(outside_file, "do not overwrite");
    fs::create_symlink(outside_file, project_directory / "derived/example-v1.wav");
    require(!audioforge::project::validate_manifest(source, project_directory).is_valid(),
            "symlink escapes in derived asset tree must be rejected");
    fs::remove(project_directory / "derived/example-v1.wav");
    write_text(project_directory / "derived/example-v1.wav", "derived output");
    fs::create_symlink(project_directory / "derived/example-v1.wav", project_directory / "original-alias.wav");
    auto alias_collision = source;
    alias_collision["sourceAssets"][0]["originalPath"] = (project_directory / "original-alias.wav").string();
    require(!audioforge::project::validate_manifest(alias_collision, project_directory).is_valid(),
            "derived output must not alias an original source through a symlink");
    fs::remove(project_directory / "derived/example-v1.wav");
    fs::create_symlink(outside_file, project_directory / "derived/example-v1.wav");
    const auto prior_directory = fs::current_path();
    fs::current_path(project_directory);
    bool relative_save_rejected = false;
    bool relative_save_succeeded = false;
    bool relative_load_rejected = false;
    try {
        audioforge::project::save_manifest("project.json", source);
    } catch (const std::exception&) {
        relative_save_rejected = true;
    }
    relative_save_succeeded = !relative_save_rejected;
    if (relative_save_succeeded) {
        try {
            (void)audioforge::project::load_manifest("project.json");
        } catch (const std::exception&) {
            relative_load_rejected = true;
        }
    }
    fs::current_path(prior_directory);
    require(relative_save_rejected,
            relative_save_succeeded && relative_load_rejected
                ? "bare-filename save accepted a symlinked derived asset that load then rejected"
                : "saving a bare filename from the project directory must reject a symlinked derived asset");
    require(!fs::exists(project_directory / "project.json"),
            "rejected relative-path save must not create a manifest");
    fs::remove_all(project_directory);
    fs::remove_all(directory);
}

void test_migration_dispatch() {
    const auto current = fixture();
    const auto first_pass = audioforge::project::migrate_manifest(current);
    const auto second_pass = audioforge::project::migrate_manifest(first_pass);
    require(first_pass == current && second_pass == current,
            "current v1 manifests must migrate idempotently without changing fields");

    auto future = current;
    future["schemaVersion"] = 2;
    bool future_rejected = false;
    try {
        (void)audioforge::project::migrate_manifest(future);
    } catch (const std::exception&) {
        future_rejected = true;
    }
    require(future_rejected, "future manifest versions must be rejected");
    require(future["schemaVersion"] == 2, "future manifest input must remain unchanged");

    auto older = current;
    older["schemaVersion"] = 0;
    bool older_rejected = false;
    try {
        (void)audioforge::project::migrate_manifest(older);
    } catch (const std::exception&) {
        older_rejected = true;
    }
    require(older_rejected, "older versions without a defined migration contract must be rejected");
    require(older["schemaVersion"] == 0, "unsupported older manifest input must remain unchanged");

    auto malformed = current;
    malformed["schemaVersion"] = "1";
    bool malformed_rejected = false;
    try {
        (void)audioforge::project::migrate_manifest(malformed);
    } catch (const std::exception&) {
        malformed_rejected = true;
    }
    require(malformed_rejected, "non-integer schema versions must be rejected");

    const auto directory = fs::temp_directory_path() / "audioforge-project-persistence-migration-dispatch";
    fs::remove_all(directory);
    fs::create_directories(directory);
    const auto manifest_path = directory / "project.json";
    const auto backup_path = fs::path(manifest_path.string() + ".bak");
    const auto source_path = directory / "original.wav";
    const auto related_path = directory / "related.txt";
    const std::string backup_bytes = "previous manifest bytes";
    const std::string source_bytes("original audio\0bytes", 20);
    const std::string related_bytes = "related project data";
    write_text(backup_path, backup_bytes);
    write_text(source_path, source_bytes);
    write_text(related_path, related_bytes);

    const auto assert_file_rejection_preserves_state = [&](const Manifest& candidate, const std::string& label) {
        const auto original_bytes = candidate.dump(2) + "\n";
        write_text(manifest_path, original_bytes);
        bool rejected = false;
        try {
            (void)audioforge::project::load_manifest(manifest_path);
        } catch (const std::exception&) {
            rejected = true;
        }
        require(rejected, label + " schemaVersion must be rejected by file loading");
        require(read_text(manifest_path) == original_bytes,
                label + " rejection must preserve the original manifest bytes");
        require(read_text(backup_path) == backup_bytes,
                label + " rejection must preserve the backup bytes");
        require(read_text(source_path) == source_bytes,
                label + " rejection must preserve related source audio");
        require(read_text(related_path) == related_bytes,
                label + " rejection must preserve related project data");
    };

    assert_file_rejection_preserves_state(future, "future");
    assert_file_rejection_preserves_state(older, "older");
    assert_file_rejection_preserves_state(malformed, "malformed");
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
    const auto sentinel_path = directory / "temporary-target.txt";
    write_text(sentinel_path, "outside sentinel");
    const auto before_symlink_save = read_text(sentinel_path);
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

    manifest["tracks"][0]["userMetadata"]["title"] = "Secure replacement";
    audioforge::project::save_manifest(manifest_path, manifest);
    const auto symlink_destination = directory / "manifest-link.json";
    fs::create_symlink(sentinel_path, symlink_destination);
    bool symlink_rejected = false;
    try {
        audioforge::project::save_manifest(symlink_destination, manifest);
    } catch (const std::exception&) {
        symlink_rejected = true;
    }
    require(symlink_rejected && read_text(sentinel_path) == before_symlink_save,
            "save must reject symlink destinations without changing their targets");

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

    struct rlimit original_limit {};
    require(::getrlimit(RLIMIT_FSIZE, &original_limit) == 0, "read file-size limit for write-failure test");
    struct rlimit failing_limit = original_limit;
    failing_limit.rlim_cur = 0;
    const auto original_handler = std::signal(SIGXFSZ, SIG_IGN);
    require(::setrlimit(RLIMIT_FSIZE, &failing_limit) == 0, "enable deterministic temporary-write failure");
    auto changed = manifest;
    changed["tracks"][0]["userMetadata"]["title"] = "Must not replace original";
    bool write_failed = false;
    try {
        audioforge::project::save_manifest(manifest_path, changed);
    } catch (const std::exception&) {
        write_failed = true;
    }
    const auto restore_limit_result = ::setrlimit(RLIMIT_FSIZE, &original_limit);
    std::signal(SIGXFSZ, original_handler);
    require(restore_limit_result == 0, "restore process file-size limit");
    require(write_failed, "filesystem write failure must be surfaced");
    require(read_text(manifest_path) == current_manifest,
            "real temporary-write failure must leave the prior manifest byte-for-byte intact");

    const auto blocked_path = directory / "not-a-file.json";
    fs::create_directory(blocked_path);
    write_text(blocked_path / "sentinel", "preserve directory");
    bool replacement_failed = false;
    try {
        audioforge::project::save_manifest(blocked_path, manifest);
    } catch (const std::exception&) {
        replacement_failed = true;
    }
    require(replacement_failed, "save over a directory must fail at the filesystem boundary");
    require(fs::is_directory(blocked_path) && read_text(blocked_path / "sentinel") == "preserve directory",
            "failed filesystem replacement must preserve the existing target");
    fs::remove_all(directory);
}

}  // namespace

int main() {
    try {
        test_round_trip_and_references();
        test_migration_dispatch();
        test_original_files_and_atomic_backup();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
