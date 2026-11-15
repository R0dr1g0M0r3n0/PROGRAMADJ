#include "project_store.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace audioforge::project {
namespace {

const std::regex kUuid{"^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"};

std::string text(const Manifest& value, const char* key) {
    return value.is_object() && value.contains(key) && value[key].is_string()
        ? value[key].get<std::string>() : std::string{};
}

void error(ValidationResult& result, const std::string& path, const std::string& message) {
    result.errors.push_back(path + ": " + message);
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

} // namespace

ValidationResult validate_manifest(const Manifest& manifest) {
    ValidationResult result;
    static const std::vector<std::string> arrays{
        "sourceAssets", "derivedAssets", "tracks", "regions", "history", "cues", "loops",
        "beatgrids", "phrases", "profiles", "validityClaims"};
    if (!manifest.is_object()) {
        error(result, "$", "manifest must be an object");
        return result;
    }
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
    std::unordered_map<std::string, std::string> kind_by_id;
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
            kind_by_id[id] = name;
            for (const auto* key : {"createdAt", "importedAt", "at", "assertedAt"})
                if (record.contains(key)) check_time(record, key, path);
        }
    }
    if (!result.errors.empty()) return result;

    for (const auto& source : manifest["sourceAssets"]) {
        const auto id = text(source, "id");
        root_by_asset[id] = id;
        if (text(source, "originalPath").empty()) error(result, "sourceAssets", "originalPath must be non-empty");
    }
    for (const auto& asset : manifest["derivedAssets"]) {
        const auto id = text(asset, "id");
        const auto root = text(asset, "rootSourceAssetId");
        const auto parent = text(asset, "parentAssetId");
        const std::filesystem::path relative(text(asset, "relativePath"));
        if (!contains(manifest, "sourceAssets", root))
            error(result, "derivedAssets", "rootSourceAssetId must identify a source asset");
        if (!contains(manifest, "sourceAssets", parent) && !contains(manifest, "derivedAssets", parent))
            error(result, "derivedAssets", "parentAssetId must identify an asset");
        if (relative.empty() || relative.is_absolute())
            error(result, "derivedAssets", "relativePath must be relative and non-empty");
        for (const auto& component : relative)
            if (component == "..") error(result, "derivedAssets", "relativePath cannot escape the project");
        if (!contains(manifest, "profiles", text(asset, "profileId")))
            error(result, "derivedAssets", "profileId must identify a processing profile");
        else for (const auto& profile : manifest["profiles"])
            if (text(profile, "id") == text(asset, "profileId") && text(profile, "kind") != "processing")
                error(result, "derivedAssets", "profileId must identify a processing profile");
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
    for (const auto& track : manifest["tracks"]) {
        const auto source = text(track, "sourceAssetId");
        const auto active = text(track, "activeAssetId");
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
        if (!contains(manifest, "tracks", text(entry, "trackId"))) error(result, "history", "trackId must resolve");
        const auto target = entry.value("target", Manifest::object());
        const auto target_array = subject_array(text(target, "kind"));
        if (target_array.empty() || !contains(manifest, target_array, text(target, "id")))
            error(result, "history", "target must resolve to its declared kind");
        if (entry.contains("inputAssetIds") && entry["inputAssetIds"].is_array())
            for (const auto& input : entry["inputAssetIds"])
                if (!input.is_string() || !root_by_asset.contains(input.get<std::string>()))
                    error(result, "history", "inputAssetIds must resolve to assets");
        if (entry.contains("outputAssetId") && !contains(manifest, "derivedAssets", text(entry, "outputAssetId")))
            error(result, "history", "outputAssetId must identify a derived asset");
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

Manifest load_manifest(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("unable to open project manifest: " + path.string());
    Manifest manifest;
    try {
        input >> manifest;
    } catch (const std::exception& exception) {
        throw std::runtime_error(std::string("invalid project manifest JSON: ") + exception.what());
    }
    const auto validation = validate_manifest(manifest);
    if (!validation.is_valid()) throw std::runtime_error("invalid project manifest: " + validation.errors.front());
    return manifest;
}

void save_manifest(const std::filesystem::path& path, const Manifest& manifest) {
    const auto validation = validate_manifest(manifest);
    if (!validation.is_valid()) throw std::runtime_error("invalid project manifest: " + validation.errors.front());
    if (path.empty() || !path.has_filename()) throw std::runtime_error("manifest path must name a file");
    const auto directory = path.parent_path().empty() ? std::filesystem::current_path() : path.parent_path();
    std::filesystem::create_directories(directory);
    static std::atomic<unsigned long long> sequence{0};
    const auto suffix = ".tmp." + std::to_string(sequence.fetch_add(1));
    const auto temporary = std::filesystem::path(path.string() + suffix);
    const auto backup = std::filesystem::path(path.string() + ".bak");
    const auto backup_temporary = std::filesystem::path(backup.string() + suffix);
    try {
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) throw std::runtime_error("unable to create temporary project manifest");
            output << manifest.dump(2) << '\n';
            output.flush();
            if (!output) throw std::runtime_error("unable to write temporary project manifest");
        }
        if (std::filesystem::exists(path)) {
            std::filesystem::copy_file(path, backup_temporary, std::filesystem::copy_options::overwrite_existing);
            std::filesystem::rename(backup_temporary, backup);
        }
        std::filesystem::rename(temporary, path);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        std::filesystem::remove(backup_temporary, ignored);
        throw;
    }
}

} // namespace audioforge::project
