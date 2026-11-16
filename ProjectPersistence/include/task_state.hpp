#pragma once

#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace audioforge::task {

using Record = nlohmann::json;

struct ReplayState {
    std::string state;
    int attempt = 0;
    std::size_t sequence = 0;
};

namespace detail {

inline void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

inline bool commit_hash(const Record& value) {
    if (!value.is_string()) return false;
    const auto text = value.get<std::string>();
    if (text.size() != 40 && text.size() != 64) return false;
    for (const auto character : text)
        if (!std::isxdigit(static_cast<unsigned char>(character)) || std::isupper(static_cast<unsigned char>(character)))
            return false;
    return true;
}

inline void evidence(const Record& record, const char* outcome) {
    require(record.contains("executionEvidence") && record["executionEvidence"].is_object(),
            "terminal transition requires execution evidence");
    const auto& value = record["executionEvidence"];
    require(value.contains("command") && value["command"].is_array() && !value["command"].empty(),
            "execution evidence requires a command");
    require(value.contains("result") && value["result"].is_object(),
            "execution evidence requires a result");
    require(value["result"].value("outcome", "") == outcome &&
                value["result"].value("detail", "") != "",
            "execution evidence outcome does not match the transition");
}

}  // namespace detail

inline ReplayState replay(const Record& records) {
    detail::require(records.is_array() && !records.empty(), "task transition history must be a non-empty array");

    ReplayState result;
    std::string task_id;
    Record prerequisites;
    for (std::size_t index = 0; index < records.size(); ++index) {
        const auto& record = records[index];
        detail::require(record.is_object(), "task transition record must be an object");
        detail::require(record.value("schemaVersion", 0) == 1, "unsupported task transition schema version");
        detail::require(record.value("taskId", "") != "", "task transition taskId is required");
        detail::require(record.value("sequence", 0) == index + 1, "task transition sequence is not contiguous");
        detail::require(record.contains("prerequisiteIds") && record["prerequisiteIds"].is_array(),
                        "task transition prerequisites are required");

        if (task_id.empty()) {
            task_id = record["taskId"].get<std::string>();
            prerequisites = record["prerequisiteIds"];
        } else {
            detail::require(record["taskId"] == task_id, "task transition history mixes task identifiers");
            detail::require(record["prerequisiteIds"] == prerequisites, "task prerequisites changed during replay");
        }

        const auto event = record.value("event", "");
        const auto from = record.contains("fromState") && record["fromState"].is_string()
            ? record["fromState"].get<std::string>() : std::string{};
        const auto to = record.value("toState", "");
        const auto attempt = record.value("attempt", -1);

        if (event == "created") {
            detail::require(index == 0 && record["fromState"].is_null() && to == "pending" && attempt == 0,
                            "created transition is invalid");
            result.state = "pending";
        } else if (event == "attempt_started") {
            detail::require(detail::commit_hash(record.value("inputCommit", "")),
                            "attempt must bind a lowercase hexadecimal input commit");
            const bool initial = result.state == "pending" && attempt == 0;
            const bool correction = result.state == "failed" && attempt >= 1 && attempt <= 3;
            detail::require(initial || correction, "attempt start is not valid for the current state");
            detail::require(from == result.state && to == "running", "attempt start transition is invalid");
            result.state = "running";
        } else if (event == "attempt_succeeded" || event == "attempt_failed") {
            detail::require(result.state == "running" && from == "running" && attempt == result.attempt,
                            "terminal attempt transition is invalid");
            detail::evidence(record, event == "attempt_succeeded" ? "success" : "failure");
            result.state = event == "attempt_succeeded" ? "succeeded" : "failed";
        } else if (event == "attempts_exhausted") {
            detail::require(result.state == "failed" && from == "failed" && to == "blocked" && attempt == 3,
                            "attempt exhaustion transition is invalid");
            result.state = "blocked";
        } else {
            throw std::runtime_error("unknown task transition event");
        }
        result.attempt = attempt;
        result.sequence = index + 1;
    }
    return result;
}

}  // namespace audioforge::task
