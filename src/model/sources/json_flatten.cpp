#include "model/sources/json_flatten.h"

#include <cstdint>
#include <cstdio>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

std::string Join(const std::string& prefix, const std::string& key)
{
    return prefix.empty() ? key : prefix + "." + key;
}

bool IsLeaf(const json& j) { return j.is_number() || j.is_boolean() || j.is_string(); }

SignalValue ToValue(const json& j)
{
    SignalValue v;
    if (j.is_boolean()) {
        v.number = j.get<bool>() ? 1.0 : 0.0;
    } else if (j.is_number()) {
        v.number = j.get<double>();
    } else {
        v.isNumber = false;
        v.text = j.get<std::string>();
    }
    return v;
}

// Name for an array element: its "name", else its CAN id in hex, else the index.
std::string ElementSegment(const json& el, std::size_t index)
{
    if (el.is_object()) {
        auto n = el.find("name");
        if (n != el.end() && n->is_string()) return n->get<std::string>();
        auto id = el.find("id");
        if (id != el.end() && id->is_number_integer()) {
            char buf[24];
            std::snprintf(buf, sizeof buf, "0x%llX", static_cast<unsigned long long>(id->get<std::int64_t>()));
            return buf;
        }
    }
    return std::to_string(index);
}

void Flatten(const json& j, const std::string& prefix, std::vector<std::pair<std::string, SignalValue>>& out)
{
    if (j.is_object()) {
        // {"name": "SOC", "value": 55, ...} is one signal.
        auto n = j.find("name");
        auto v = j.find("value");
        if (n != j.end() && n->is_string() && v != j.end() && IsLeaf(*v)) {
            out.emplace_back(Join(prefix, n->get<std::string>()), ToValue(*v));
            return;
        }
        for (auto it = j.begin(); it != j.end(); ++it) Flatten(it.value(), Join(prefix, it.key()), out);
    } else if (j.is_array()) {
        for (std::size_t i = 0; i < j.size(); ++i) {
            const json& el = j[i];
            const bool namedValue = el.is_object() && el.contains("name") && el.contains("value");
            if (namedValue) {
                Flatten(el, prefix, out);
            } else {
                // A named element (e.g. a CAN message): drop its own "name" leaf, it is in the path now.
                std::string seg = ElementSegment(el, i);
                if (el.is_object() && el.contains("name")) {
                    for (auto it = el.begin(); it != el.end(); ++it)
                        if (it.key() != "name") Flatten(it.value(), Join(Join(prefix, seg), it.key()), out);
                } else {
                    Flatten(el, Join(prefix, seg), out);
                }
            }
        }
    } else if (IsLeaf(j)) {
        out.emplace_back(prefix.empty() ? std::string("value") : prefix, ToValue(j));
    }
}

} // namespace

void FlattenJson(const std::string& text, std::vector<std::pair<std::string, SignalValue>>& out, bool* ok)
{
    json j = json::parse(text, nullptr, false);
    if (!j.is_discarded()) {
        Flatten(j, "", out);
        if (ok) *ok = true;
        return;
    }
    // Fall back to newline-delimited JSON (several objects in one datagram).
    bool any = false, failed = false;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        if (line.find_first_not_of(" \t\r") != std::string::npos) {
            json lj = json::parse(line, nullptr, false);
            if (lj.is_discarded()) failed = true;
            else { Flatten(lj, "", out); any = true; }
        }
        start = end + 1;
    }
    if (ok) *ok = any && !failed;
}
