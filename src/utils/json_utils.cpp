#include <nlohmann/json.hpp>
#include <string>

// ============================================================
// json_utils — shared JSON helpers
// ============================================================

namespace json_utils {

// Compact serialise (no whitespace) — minimises token usage
std::string compact(const nlohmann::json& j) {
    return j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

// Pretty serialise — for debug/logging only
std::string pretty(const nlohmann::json& j, int indent = 2) {
    return j.dump(indent);
}

// Safe string extraction from a json object
std::string get_string(const nlohmann::json& j,
                       const std::string& key,
                       const std::string& def = "") {
    if (j.contains(key) && j[key].is_string())
        return j[key].get<std::string>();
    return def;
}

// Safe int extraction
int64_t get_int(const nlohmann::json& j, const std::string& key, int64_t def = 0) {
    if (j.contains(key) && j[key].is_number())
        return j[key].get<int64_t>();
    return def;
}

// Rough token estimate: 1 token ≈ 4 chars
int64_t estimate_tokens(const std::string& s) {
    return static_cast<int64_t>((s.size() + 3) / 4);
}

} // namespace json_utils
