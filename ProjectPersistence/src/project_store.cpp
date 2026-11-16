#include "project_store.hpp"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iterator>
#include <random>
#include <regex>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace audioforge::project {
namespace {

const std::regex kUuid{"^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"};

const Manifest& schema_document() {
    static const Manifest schema = [] {
#ifndef PROJECT_MANIFEST_SCHEMA_PATH
#error "PROJECT_MANIFEST_SCHEMA_PATH must identify the project manifest schema"
#endif
        std::ifstream input(PROJECT_MANIFEST_SCHEMA_PATH);
        if (!input) throw std::runtime_error("unable to load project manifest JSON Schema");
        return Manifest::parse(input);
    }();
    return schema;
}

std::string text(const Manifest& value, const char* key) {
    return value.is_object() && value.contains(key) && value[key].is_string()
        ? value[key].get<std::string>() : std::string{};
}

void error(ValidationResult& result, const std::string& path, const std::string& message) {
    result.errors.push_back(path + ": " + message);
}

const Manifest* resolve_reference(const Manifest& root, const std::string& reference) {
    if (!reference.starts_with("#")) return nullptr;
    const auto fragment = reference.substr(1);
    const auto pointer = Manifest::json_pointer(fragment);
    try {
        return &root.at(pointer);
    } catch (const std::exception&) {
        return nullptr;
    }
}

bool schema_type_matches(const Manifest& value, const std::string& type) {
    if (type == "object") return value.is_object();
    if (type == "array") return value.is_array();
    if (type == "string") return value.is_string();
    if (type == "boolean") return value.is_boolean();
    if (type == "integer") return value.is_number_integer();
    if (type == "number") return value.is_number();
    if (type == "null") return value.is_null();
    return false;
}

std::size_t utf8_length(const std::string& value) {
    return static_cast<std::size_t>(std::count_if(value.begin(), value.end(), [](unsigned char character) {
        return (character & 0xC0) != 0x80;
    }));
}

void validate_schema_node(const Manifest& value, const Manifest& rule, const Manifest& root,
                          const std::string& path, ValidationResult& result) {
    if (!rule.is_object()) {
        error(result, path, "invalid schema node");
        return;
    }
    if (rule.contains("$ref")) {
        const auto* referenced = resolve_reference(root, rule["$ref"].get<std::string>());
        if (referenced == nullptr) {
            error(result, path, "schema reference cannot be resolved");
            return;
        }
        validate_schema_node(value, *referenced, root, path, result);
    }
    if (rule.contains("const") && value != rule["const"])
        error(result, path, "must equal the schema constant");
    if (rule.contains("enum")) {
        const auto& allowed = rule["enum"];
        if (!allowed.is_array() || std::find(allowed.begin(), allowed.end(), value) == allowed.end())
            error(result, path, "value is outside the schema enumeration");
    }
    if (rule.contains("type")) {
        const auto& type = rule["type"];
        bool matches = false;
        if (type.is_string()) matches = schema_type_matches(value, type.get<std::string>());
        else if (type.is_array())
            for (const auto& option : type)
                if (option.is_string() && schema_type_matches(value, option.get<std::string>())) matches = true;
        if (!matches) {
            error(result, path, "has the wrong JSON type");
            return;
        }
    }
    if (rule.contains("pattern") && value.is_string()) {
        try {
            if (!std::regex_match(value.get<std::string>(), std::regex(rule["pattern"].get<std::string>())))
                error(result, path, "does not match the schema pattern");
        } catch (const std::regex_error&) {
            error(result, path, "schema contains an unsupported regular expression");
        }
    }
    if (rule.contains("minLength") && value.is_string() &&
        utf8_length(value.get<std::string>()) < rule["minLength"].get<std::size_t>())
        error(result, path, "is shorter than the schema minimum");
    if (rule.contains("minimum") && value.is_number() && value.get<double>() < rule["minimum"].get<double>())
        error(result, path, "is below the schema minimum");
    if (rule.contains("minItems") && value.is_array() && value.size() < rule["minItems"].get<std::size_t>())
        error(result, path, "has fewer items than the schema minimum");
    if (rule.value("uniqueItems", false) && value.is_array()) {
        std::unordered_set<std::string> unique_values;
        for (const auto& item : value)
            if (!unique_values.insert(item.dump()).second) error(result, path, "items must be unique");
    }
    if (value.is_object()) {
        if (rule.contains("required") && rule["required"].is_array())
            for (const auto& required : rule["required"])
                if (required.is_string() && !value.contains(required.get<std::string>()))
                    error(result, path + "." + required.get<std::string>(), "required property is missing");
        if (rule.value("additionalProperties", true) == false) {
            const auto properties = rule.value("properties", Manifest::object());
            for (auto iterator = value.begin(); iterator != value.end(); ++iterator)
                if (!properties.contains(iterator.key())) error(result, path + "." + iterator.key(), "additional property is not allowed");
        }
        if (rule.contains("properties") && rule["properties"].is_object())
            for (auto iterator = rule["properties"].begin(); iterator != rule["properties"].end(); ++iterator)
                if (value.contains(iterator.key()))
                    validate_schema_node(value[iterator.key()], iterator.value(), root, path + "." + iterator.key(), result);
    }
    if (value.is_array() && rule.contains("items"))
        for (std::size_t index = 0; index < value.size(); ++index)
            validate_schema_node(value[index], rule["items"], root, path + "[" + std::to_string(index) + "]", result);
}

bool timestamp(const std::string& value) {
    if (value.size() != 24 || value[4] != '-' || value[7] != '-' || value[10] != 'T' ||
        value[13] != ':' || value[16] != ':' || value[19] != '.' || value[23] != 'Z') return false;
    int year_value = 0, month_value = 0, day_value = 0, hour = 0, minute = 0, second = 0, millis = 0;
    char tail = 0;
    if (std::sscanf(value.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d.%3dZ%c", &year_value, &month_value, &day_value,
                    &hour, &minute, &second, &millis, &tail) != 7) return false;
    using namespace std::chrono;
    const year_month_day date{year{year_value}, month{static_cast<unsigned>(month_value)}, day{static_cast<unsigned>(day_value)}};
    return date.ok() && hour < 24 && minute < 60 && second < 60;
}

bool contains(const Manifest& manifest, const std::string& array, const std::string& id) {
    if (!manifest.contains(array) || !manifest[array].is_array()) return false;
    for (const auto& record : manifest[array]) if (text(record, "id") == id) return true;
    return false;
}

std::string subject_array(const std::string& kind) {
    static const std::unordered_map<std::string, std::string> arrays{
        {"track", "tracks"}, {"region", "regions"}, {"cue", "cues"}, {"loop", "loops"},
        {"beatgrid", "beatgrids"}, {"phrase", "phrases"}, {"derived_asset", "derivedAssets"}};
    const auto found = arrays.find(kind);
    return found == arrays.end() ? std::string{} : found->second;
}

bool valid_span(const Manifest& span) {
    return span.is_object() && span.contains("startFrame") && span.contains("endFrame") &&
        span["startFrame"].is_number_unsigned() && span["endFrame"].is_number_unsigned() &&
        span["startFrame"].get<std::uint64_t>() < span["endFrame"].get<std::uint64_t>();
}

bool path_is_within(const std::filesystem::path& candidate, const std::filesystem::path& root) {
    auto child = candidate.begin();
    for (auto parent = root.begin(); parent != root.end(); ++parent, ++child)
        if (child == candidate.end() || *child != *parent) return false;
    return child != candidate.end();
}

void validate_derived_path(const std::string& raw, const std::filesystem::path& project_directory,
                           ValidationResult& result) {
    const std::filesystem::path relative(raw);
    if (raw.empty() || raw.find('\\') != std::string::npos || relative.is_absolute() ||
        relative.lexically_normal() != relative || relative.generic_string() != raw ||
        relative.begin() == relative.end() || *relative.begin() != "derived" ||
        std::distance(relative.begin(), relative.end()) < 2) {
        error(result, "derivedAssets.relativePath", "must be a normalized path beneath derived/");
        return;
    }
    for (const auto& component : relative)
        if (component == "." || component == "..") {
            error(result, "derivedAssets.relativePath", "dot segments are not permitted");
            return;
        }
    if (project_directory.empty()) return;

    std::error_code filesystem_error;
    const auto project_root = std::filesystem::weakly_canonical(project_directory, filesystem_error);
    if (filesystem_error) {
        error(result, "derivedAssets.relativePath", "project directory cannot be resolved safely");
        return;
    }
    auto current = project_root;
    const auto derived_root = project_root / "derived";
    for (const auto& component : relative) {
        current /= component;
        const auto status = std::filesystem::symlink_status(current, filesystem_error);
        if (!filesystem_error && std::filesystem::is_symlink(status)) {
            error(result, "derivedAssets.relativePath", "symlink components are not permitted");
            return;
        }
        filesystem_error.clear();
    }
    const auto candidate = std::filesystem::weakly_canonical(project_root / relative, filesystem_error);
    if (filesystem_error || !path_is_within(candidate, derived_root))
        error(result, "derivedAssets.relativePath", "resolved path must remain inside the derived asset tree");
}

std::string io_error(const std::string& operation) {
    return operation + ": " + std::strerror(errno);
}

std::string read_all(int descriptor) {
    std::string content;
    char buffer[8192];
    while (true) {
        const auto bytes = ::read(descriptor, buffer, sizeof(buffer));
        if (bytes == 0) return content;
        if (bytes < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error(io_error("read failed"));
        }
        content.append(buffer, static_cast<std::size_t>(bytes));
    }
}

void write_all(int descriptor, std::string_view content) {
    std::size_t written = 0;
    while (written < content.size()) {
        const auto bytes = ::write(descriptor, content.data() + written, content.size() - written);
        if (bytes < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error(io_error("write failed"));
        }
        if (bytes == 0) throw std::runtime_error("write made no progress");
        written += static_cast<std::size_t>(bytes);
    }
    if (::fsync(descriptor) != 0) throw std::runtime_error(io_error("fsync failed"));
}

std::pair<int, std::string> create_unique_file(int directory, const std::string& prefix) {
    static std::atomic<unsigned long long> sequence{0};
    std::random_device random;
    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto name = prefix + ".tmp." + std::to_string(::getpid()) + "." +
            std::to_string(sequence.fetch_add(1)) + "." + std::to_string(random());
        const int descriptor = ::openat(directory, name.c_str(),
            O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (descriptor >= 0) return {descriptor, name};
        if (errno != EEXIST) throw std::runtime_error(io_error("secure temporary creation failed"));
    }
    throw std::runtime_error("unable to allocate a unique temporary file");
}

bool regular_file_at(int directory, const std::string& name, struct stat& status) {
    if (::fstatat(directory, name.c_str(), &status, AT_SYMLINK_NOFOLLOW) == 0) {
        if (!S_ISREG(status.st_mode)) throw std::runtime_error("refusing to replace a non-regular file: " + name);
        return true;
    }
    if (errno == ENOENT) return false;
    throw std::runtime_error(io_error("unable to inspect destination"));
}

class ScopedDescriptor {
public:
    explicit ScopedDescriptor(int descriptor) : descriptor_(descriptor) {}
    ~ScopedDescriptor() { if (descriptor_ >= 0) ::close(descriptor_); }
    ScopedDescriptor(const ScopedDescriptor&) = delete;
    ScopedDescriptor& operator=(const ScopedDescriptor&) = delete;
    [[nodiscard]] int get() const noexcept { return descriptor_; }
    int release() noexcept { const int result = descriptor_; descriptor_ = -1; return result; }

private:
    int descriptor_;
};

} // namespace

ValidationResult validate_manifest(const Manifest& manifest, const std::filesystem::path& project_directory) {
    ValidationResult result;
    static const std::vector<std::string> arrays{
        "sourceAssets", "derivedAssets", "tracks", "regions", "history", "cues", "loops",
        "beatgrids", "phrases", "profiles", "validityClaims"};
    if (!manifest.is_object()) {
        error(result, "$", "manifest must be an object");
        return result;
    }
    try {
        const auto& schema = schema_document();
        validate_schema_node(manifest, schema, schema, "$", result);
    } catch (const std::exception& exception) {
        error(result, "$", std::string("unable to validate manifest schema: ") + exception.what());
    }
    if (!result.errors.empty()) return result;
    if (!manifest.contains("schemaVersion") || !manifest["schemaVersion"].is_number_integer() ||
        manifest["schemaVersion"] != 1) {
        error(result, "schemaVersion", "only manifest version 1 is supported");
        return result;
    }
    for (const auto* key : {"projectId", "createdAt", "updatedAt"})
        if (text(manifest, key).empty()) error(result, key, "required string is missing");
    for (const auto& name : arrays)
        if (!manifest.contains(name) || !manifest[name].is_array()) error(result, name, "required array is missing");
    if (!result.errors.empty()) return result;

    std::unordered_set<std::string> ids;
    std::unordered_map<std::string, std::string> root_by_asset;
    const auto project_id = text(manifest, "projectId");
    if (!std::regex_match(project_id, kUuid)) error(result, "projectId", "must be a lowercase UUID");
    ids.insert(project_id);
    const auto check_time = [&](const Manifest& record, const char* key, const std::string& path) {
        if (!timestamp(text(record, key))) error(result, path + "." + key, "must be a calendar-valid UTC timestamp");
    };
    check_time(manifest, "createdAt", "$");
    check_time(manifest, "updatedAt", "$");
    if (text(manifest, "updatedAt") < text(manifest, "createdAt"))
        error(result, "updatedAt", "cannot precede createdAt");

    for (const auto& name : arrays) {
        static const std::unordered_map<std::string, std::vector<std::string>> required_fields{
            {"sourceAssets", {"id", "kind", "originalPath", "sha256", "importedAt"}},
            {"derivedAssets", {"id", "rootSourceAssetId", "parentAssetId", "relativePath", "sha256", "profileId", "createdAt"}},
            {"tracks", {"id", "sourceAssetId", "activeAssetId", "createdAt", "userMetadata"}},
            {"regions", {"id", "trackId", "assetId", "span", "kind"}},
            {"history", {"id", "trackId", "at", "operation", "target", "inputAssetIds"}},
            {"cues", {"id", "trackId", "assetId", "frame", "label"}},
            {"loops", {"id", "trackId", "assetId", "span"}},
            {"beatgrids", {"id", "trackId", "assetId", "anchors"}},
            {"phrases", {"id", "trackId", "assetId", "span", "label"}},
            {"profiles", {"id", "kind", "name", "revision", "parameters", "createdAt"}},
            {"validityClaims", {"id", "subject", "status", "assertedAt", "provenance"}}};
        std::size_t index = 0;
        for (const auto& record : manifest[name]) {
            const auto path = name + "[" + std::to_string(index++) + "]";
            if (!record.is_object()) {
                error(result, path, "record must be an object");
                continue;
            }
            for (const auto& field : required_fields.at(name))
                if (!record.contains(field)) error(result, path + "." + field, "required field is missing");
            const auto id = text(record, "id");
            if (!std::regex_match(id, kUuid)) error(result, path + ".id", "must be a lowercase UUID");
            if (!ids.insert(id).second) error(result, path + ".id", "must be globally unique");
            for (const auto* key : {"createdAt", "importedAt", "at", "assertedAt"})
                if (record.contains(key)) check_time(record, key, path);
        }
    }
    if (!result.errors.empty()) return result;

    std::unordered_map<std::string, std::string> source_kinds;
    for (const auto& source : manifest["sourceAssets"]) {
        const auto id = text(source, "id");
        root_by_asset[id] = id;
        source_kinds[id] = text(source, "kind");
        if (text(source, "originalPath").empty()) error(result, "sourceAssets", "originalPath must be non-empty");
    }
    std::unordered_set<std::string> derived_paths;
    for (const auto& asset : manifest["derivedAssets"]) {
        const auto id = text(asset, "id");
        const auto root = text(asset, "rootSourceAssetId");
        const auto parent = text(asset, "parentAssetId");
        const auto relative_text = text(asset, "relativePath");
        if (!contains(manifest, "sourceAssets", root))
            error(result, "derivedAssets", "rootSourceAssetId must identify a source asset");
        if (!contains(manifest, "sourceAssets", parent) && !contains(manifest, "derivedAssets", parent))
            error(result, "derivedAssets", "parentAssetId must identify an asset");
        validate_derived_path(relative_text, project_directory, result);
        if (!derived_paths.insert(relative_text).second)
            error(result, "derivedAssets", "relativePath must be unique");
        const auto output_path = (project_directory / relative_text).lexically_normal();
        for (const auto& source : manifest["sourceAssets"]) {
            const auto original_path = std::filesystem::path(text(source, "originalPath"));
            const auto source_path = (original_path.is_absolute() ? original_path : project_directory / original_path).lexically_normal();
            if (!project_directory.empty()) {
                std::error_code source_error;
                std::error_code output_error;
                const auto resolved_source = std::filesystem::weakly_canonical(source_path, source_error);
                const auto resolved_output = std::filesystem::weakly_canonical(output_path, output_error);
                if (source_error || output_error)
                    error(result, "derivedAssets", "source and output paths cannot be resolved safely");
                else if (resolved_source == resolved_output)
                    error(result, "derivedAssets", "generated asset cannot overwrite an original source file");
            }
        }
        if (!contains(manifest, "profiles", text(asset, "profileId")))
            error(result, "derivedAssets", "profileId must identify a processing profile");
        else for (const auto& profile : manifest["profiles"])
            if (text(profile, "id") == text(asset, "profileId") && text(profile, "kind") != "processing")
                error(result, "derivedAssets", "profileId must identify a processing profile");
        if (source_kinds.contains(root) && source_kinds[root] != "audio")
            error(result, "derivedAssets", "root source must be an audio asset");
        root_by_asset[id] = root;
    }
    for (const auto& asset : manifest["derivedAssets"]) {
        const auto id = text(asset, "id");
        const auto root = text(asset, "rootSourceAssetId");
        const auto parent = text(asset, "parentAssetId");
        if (root_by_asset.contains(parent) && root_by_asset[parent] != root)
            error(result, "derivedAssets", "parent and child must share the same root source");
        std::unordered_set<std::string> visited;
        auto cursor = id;
        while (contains(manifest, "derivedAssets", cursor)) {
            if (!visited.insert(cursor).second) {
                error(result, "derivedAssets", "asset ancestry must be acyclic");
                break;
            }
            for (const auto& candidate : manifest["derivedAssets"])
                if (text(candidate, "id") == cursor) cursor = text(candidate, "parentAssetId");
        }
        if (cursor != root) error(result, "derivedAssets", "asset ancestry must terminate at rootSourceAssetId");
    }
    std::unordered_map<std::string, std::string> track_sources;
    for (const auto& track : manifest["tracks"]) {
        const auto source = text(track, "sourceAssetId");
        const auto active = text(track, "activeAssetId");
        track_sources[text(track, "id")] = source;
        if (!contains(manifest, "sourceAssets", source)) error(result, "tracks", "sourceAssetId must identify a source asset");
        else for (const auto& record : manifest["sourceAssets"])
            if (text(record, "id") == source && text(record, "kind") != "audio")
                error(result, "tracks", "sourceAssetId must identify an audio source");
        if (!root_by_asset.contains(active) || root_by_asset[active] != source)
            error(result, "tracks", "activeAssetId must be in the source asset lineage");
    }

    const auto check_annotation = [&](const Manifest& record, const std::string& array, bool has_span) {
        const auto track = text(record, "trackId");
        const auto asset = text(record, "assetId");
        if (!contains(manifest, "tracks", track)) error(result, array, "trackId must identify a track");
        if (!root_by_asset.contains(asset)) error(result, array, "assetId must identify an audio asset");
        if (contains(manifest, "tracks", track) && root_by_asset.contains(asset))
            for (const auto& item : manifest["tracks"])
                if (text(item, "id") == track && root_by_asset[asset] != text(item, "sourceAssetId"))
                    error(result, array, "assetId must be in the track's source lineage");
        if (has_span && (!record.contains("span") || !valid_span(record["span"])))
            error(result, array, "span must be a non-empty half-open interval");
    };
    for (const auto& item : manifest["regions"]) check_annotation(item, "regions", true);
    for (const auto& item : manifest["cues"]) check_annotation(item, "cues", false);
    for (const auto& item : manifest["loops"]) check_annotation(item, "loops", true);
    for (const auto& item : manifest["phrases"]) check_annotation(item, "phrases", true);
    for (const auto& grid : manifest["beatgrids"]) {
        check_annotation(grid, "beatgrids", false);
        if (!grid.contains("anchors") || !grid["anchors"].is_array() || grid["anchors"].size() < 2) {
            error(result, "beatgrids", "requires at least two anchors");
            continue;
        }
        std::uint64_t last_frame = 0;
        std::int64_t last_beat = -1;
        bool first = true;
        for (const auto& anchor : grid["anchors"]) {
            if (!anchor.is_object() || !anchor.contains("frame") || !anchor["frame"].is_number_unsigned() ||
                !anchor.contains("beatIndex") || !anchor["beatIndex"].is_number_integer()) {
                error(result, "beatgrids", "anchor frame and beatIndex must be integers");
                continue;
            }
            const auto frame = anchor["frame"].get<std::uint64_t>();
            const auto beat = anchor["beatIndex"].get<std::int64_t>();
            if (!first && (frame <= last_frame || beat <= last_beat))
                error(result, "beatgrids", "anchor frames and beatIndex values must strictly increase");
            first = false;
            last_frame = frame;
            last_beat = beat;
        }
    }
    for (const auto& entry : manifest["history"]) {
        const auto track_id = text(entry, "trackId");
        if (!track_sources.contains(track_id)) error(result, "history", "trackId must resolve");
        const auto target = entry.value("target", Manifest::object());
        const auto target_array = subject_array(text(target, "kind"));
        if (target_array.empty() || !contains(manifest, target_array, text(target, "id")))
            error(result, "history", "target must resolve to its declared kind");
        if (entry.contains("inputAssetIds") && entry["inputAssetIds"].is_array())
            for (const auto& input : entry["inputAssetIds"])
                if (!input.is_string() || !root_by_asset.contains(input.get<std::string>()) ||
                    !track_sources.contains(track_id) || root_by_asset[input.get<std::string>()] != track_sources[track_id])
                    error(result, "history", "inputAssetIds must belong to the track's asset lineage");
        if (entry.contains("outputAssetId")) {
            const auto output = text(entry, "outputAssetId");
            if (!contains(manifest, "derivedAssets", output) || !root_by_asset.contains(output) ||
                !track_sources.contains(track_id) || root_by_asset[output] != track_sources[track_id])
                error(result, "history", "outputAssetId must be a derived asset in the track's lineage");
        }
        if (entry.contains("profileId") && !contains(manifest, "profiles", text(entry, "profileId")))
            error(result, "history", "profileId must identify a profile");
    }

    std::unordered_map<std::string, std::string> claim_subjects;
    std::unordered_map<std::string, std::size_t> children;
    for (const auto& claim : manifest["validityClaims"]) {
        const auto subject = claim.value("subject", Manifest::object());
        const auto array = subject_array(text(subject, "kind"));
        const auto id = text(subject, "id");
        if (array.empty() || !contains(manifest, array, id))
            error(result, "validityClaims", "subject must resolve to its declared kind");
        claim_subjects[text(claim, "id")] = text(subject, "kind") + ":" + id;
        const auto provenance = claim.value("provenance", Manifest::object());
        if (provenance.contains("assetIds") && provenance["assetIds"].is_array())
            for (const auto& asset : provenance["assetIds"])
                if (!asset.is_string() || !root_by_asset.contains(asset.get<std::string>()))
                    error(result, "validityClaims", "provenance assetIds must resolve");
        if (provenance.contains("profileId") && !contains(manifest, "profiles", text(provenance, "profileId")))
            error(result, "validityClaims", "provenance profileId must resolve");
        if (provenance.contains("historyId") && !contains(manifest, "history", text(provenance, "historyId")))
            error(result, "validityClaims", "provenance historyId must resolve");
        if (text(provenance, "origin") == "import") {
            bool source_cited = false;
            if (provenance.contains("assetIds") && provenance["assetIds"].is_array())
                for (const auto& asset : provenance["assetIds"])
                    if (asset.is_string() && contains(manifest, "sourceAssets", asset.get<std::string>())) source_cited = true;
            if (!source_cited) error(result, "validityClaims", "import claim must cite a source asset");
        }
        if (text(provenance, "origin") == "analysis" &&
            !provenance.contains("profileId") && !provenance.contains("historyId"))
            error(result, "validityClaims", "analysis claim must identify a profile or history entry");
    }
    for (const auto& claim : manifest["validityClaims"]) {
        if (!claim.contains("supersedesClaimId")) continue;
        const auto parent = text(claim, "supersedesClaimId");
        const auto id = text(claim, "id");
        if (!contains(manifest, "validityClaims", parent) || claim_subjects[parent] != claim_subjects[id])
            error(result, "validityClaims", "supersedesClaimId must identify a claim for the same subject");
        if (++children[parent] > 1) error(result, "validityClaims", "claim history must be a linear chain");
        std::unordered_set<std::string> visited;
        auto cursor = parent;
        while (contains(manifest, "validityClaims", cursor)) {
            if (!visited.insert(cursor).second || cursor == id) {
                error(result, "validityClaims", "claim history must be acyclic");
                break;
            }
            std::string next;
            for (const auto& candidate : manifest["validityClaims"])
                if (text(candidate, "id") == cursor) next = text(candidate, "supersedesClaimId");
            cursor = next;
        }
    }
    std::unordered_map<std::string, std::size_t> terminals;
    for (const auto& [id, subject] : claim_subjects)
        if (!children.contains(id)) ++terminals[subject];
    for (const auto& [subject, count] : terminals)
        if (count > 1) error(result, "validityClaims", "subject has multiple terminal claims: " + subject);
    return result;
}

Manifest migrate_manifest(const Manifest& manifest) {
    if (!manifest.is_object() || !manifest.contains("schemaVersion") ||
        !manifest["schemaVersion"].is_number_integer()) {
        throw std::runtime_error("project manifest must declare an integer schemaVersion");
    }

    if (manifest["schemaVersion"].is_number_unsigned()) {
        const auto version = manifest["schemaVersion"].get<std::uint64_t>();
        if (version > 1) throw std::runtime_error("unsupported future project manifest version");
        if (version < 1) throw std::runtime_error("unsupported older project manifest version");
    } else {
        const auto version = manifest["schemaVersion"].get<std::int64_t>();
        if (version > 1) throw std::runtime_error("unsupported future project manifest version");
        if (version < 1) throw std::runtime_error("unsupported older project manifest version");
    }

    const auto validation = validate_manifest(manifest);
    if (!validation.is_valid())
        throw std::runtime_error("invalid v1 project manifest: " + validation.errors.front());

    // Version 1 is the current contract. No older on-disk contract is defined yet.
    return manifest;
}

Manifest load_manifest(const std::filesystem::path& path) {
    if (path.empty() || !path.has_filename()) throw std::runtime_error("manifest path must name a file");
    const auto directory_path = std::filesystem::canonical(
        path.parent_path().empty() ? std::filesystem::current_path() : path.parent_path());
    ScopedDescriptor directory(::open(directory_path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (directory.get() < 0) throw std::runtime_error(io_error("unable to open project directory"));
    const auto filename = path.filename().string();
    struct stat before {};
    if (!regular_file_at(directory.get(), filename, before))
        throw std::runtime_error("project manifest does not exist: " + path.string());
    ScopedDescriptor input(::openat(directory.get(), filename.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC));
    if (input.get() < 0) throw std::runtime_error(io_error("unable to open project manifest without following symlinks"));
    const auto bytes = read_all(input.get());
    const auto manifest = Manifest::parse(bytes);
    auto migrated = migrate_manifest(manifest);
    const auto validation = validate_manifest(migrated, directory_path);
    if (!validation.is_valid()) throw std::runtime_error("invalid project manifest: " + validation.errors.front());
    return migrated;
}

void save_manifest(const std::filesystem::path& path, const Manifest& manifest) {
    if (path.empty() || !path.has_filename()) throw std::runtime_error("manifest path must name a file");
    const auto directory_path = std::filesystem::canonical(
        path.parent_path().empty() ? std::filesystem::current_path() : path.parent_path());
    const auto validation = validate_manifest(manifest, directory_path);
    if (!validation.is_valid()) throw std::runtime_error("invalid project manifest: " + validation.errors.front());
    ScopedDescriptor directory(::open(directory_path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (directory.get() < 0) throw std::runtime_error(io_error("unable to open project directory"));
    const auto filename = path.filename().string();
    const auto backup_name = filename + ".bak";
    struct stat original_status {};
    struct stat backup_status {};
    bool had_original = false;
    bool temp_created = false;
    bool backup_temp_created = false;
    std::string temporary_name;
    std::string backup_temp_name;
    try {
        had_original = regular_file_at(directory.get(), filename, original_status);
        std::string original_bytes;
        if (had_original) {
            ScopedDescriptor original(::openat(directory.get(), filename.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC));
            if (original.get() < 0) throw std::runtime_error(io_error("unable to open prior manifest safely"));
            struct stat opened_status {};
            if (::fstat(original.get(), &opened_status) != 0 || opened_status.st_dev != original_status.st_dev ||
                opened_status.st_ino != original_status.st_ino) {
                throw std::runtime_error("manifest changed while it was being opened");
            }
            original_bytes = read_all(original.get());
            const auto previous = Manifest::parse(original_bytes);
            const auto previous_validation = validate_manifest(previous, directory_path);
            if (!previous_validation.is_valid())
                throw std::runtime_error("refusing to overwrite a file that is not a valid project manifest");
        }
        const bool had_backup = regular_file_at(directory.get(), backup_name, backup_status);
        auto [temp_descriptor, temp_name] = create_unique_file(directory.get(), filename);
        temporary_name = std::move(temp_name);
        temp_created = true;
        const auto new_bytes = manifest.dump(2) + "\n";
        try {
            write_all(temp_descriptor, new_bytes);
        } catch (...) {
            ::close(temp_descriptor);
            throw;
        }
        if (::close(temp_descriptor) != 0) throw std::runtime_error(io_error("unable to close temporary manifest"));

        if (had_original) {
            auto [backup_descriptor, backup_temp] = create_unique_file(directory.get(), backup_name);
            backup_temp_name = std::move(backup_temp);
            backup_temp_created = true;
            try {
                write_all(backup_descriptor, original_bytes);
            } catch (...) {
                ::close(backup_descriptor);
                throw;
            }
            if (::close(backup_descriptor) != 0) throw std::runtime_error(io_error("unable to close manifest backup"));
            struct stat current_backup_status {};
            const bool backup_exists = regular_file_at(directory.get(), backup_name, current_backup_status);
            if (backup_exists != had_backup || (had_backup &&
                (current_backup_status.st_dev != backup_status.st_dev || current_backup_status.st_ino != backup_status.st_ino)))
                throw std::runtime_error("manifest backup changed during save");
            if (::renameat(directory.get(), backup_temp_name.c_str(), directory.get(), backup_name.c_str()) != 0)
                throw std::runtime_error(io_error("unable to atomically preserve previous manifest"));
            backup_temp_created = false;
        }

        struct stat current_status {};
        const bool target_exists = regular_file_at(directory.get(), filename, current_status);
        if (target_exists != had_original ||
            (had_original && (current_status.st_dev != original_status.st_dev || current_status.st_ino != original_status.st_ino)))
            throw std::runtime_error("manifest destination changed during save");
        if (::renameat(directory.get(), temporary_name.c_str(), directory.get(), filename.c_str()) != 0)
            throw std::runtime_error(io_error("atomic manifest replacement failed"));
        temp_created = false;
        (void)::fsync(directory.get());
    } catch (...) {
        if (temp_created) (void)::unlinkat(directory.get(), temporary_name.c_str(), 0);
        if (backup_temp_created) (void)::unlinkat(directory.get(), backup_temp_name.c_str(), 0);
        throw;
    }
}

} // namespace audioforge::project
