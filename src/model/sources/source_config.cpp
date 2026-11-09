#include "model/sources/source_config.h"

#include "model/sources/simulated_source.h"
#include "model/sources/udp_source.h"

#include <cstdio>
#include <fstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {
const char* KindName(SourceConfig::Kind k) { return k == SourceConfig::Kind::Udp ? "udp" : "simulated"; }
} // namespace

bool SourceConfig::load(const std::string& file)
{
    path = file;
    std::ifstream in(file);
    if (!in) return false;
    try {
        json j = json::parse(in);
        kind = j.value("source", "simulated") == "udp" ? Kind::Udp : Kind::Simulated;
        if (j.contains("udp")) {
            const json& u = j["udp"];
            udp.bindAddress = u.value("bind_address", udp.bindAddress);
            udp.port = u.value("port", udp.port);
            udp.timeoutSec = u.value("timeout_s", udp.timeoutSec);
        }
        if (j.contains("signals")) {
            const json& m = j["signals"];
            for (int i = 0; i < kFieldCount; ++i) {
                const char* key = GetFieldInfo(static_cast<FrameField>(i)).key;
                if (!m.contains(key)) continue;
                const json& e = m[key];
                SignalBinding& b = bindings[i];
                if (e.is_string()) {
                    b.signal = e.get<std::string>();
                } else {
                    b.signal = e.value("signal", std::string());
                    b.scale = e.value("scale", 1.0f);
                    b.offset = e.value("offset", 0.0f);
                }
            }
        }
        // A signal may drive only one value: drop later duplicates (e.g. from a hand-edited file).
        for (int i = 0; i < kFieldCount; ++i)
            for (int k = 0; k < i; ++k)
                if (SameSignal(bindings[k].signal, bindings[i].signal)) {
                    std::fprintf(stderr, "%s: \"%s\" is already bound to %s, ignoring it for %s\n", file.c_str(),
                                 bindings[i].signal.c_str(), GetFieldInfo(static_cast<FrameField>(k)).key,
                                 GetFieldInfo(static_cast<FrameField>(i)).key);
                    bindings[i] = SignalBinding{};
                    break;
                }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Could not read %s: %s\n", file.c_str(), e.what());
        return false;
    }
    return true;
}

bool SourceConfig::save() const
{
    json j;
    j["source"] = KindName(kind);
    j["udp"] = {{"bind_address", udp.bindAddress}, {"port", udp.port}, {"timeout_s", udp.timeoutSec}};
    json m = json::object();
    for (int i = 0; i < kFieldCount; ++i) {
        const SignalBinding& b = bindings[i];
        if (b.signal.empty()) continue;
        m[GetFieldInfo(static_cast<FrameField>(i)).key] = {{"signal", b.signal}, {"scale", b.scale}, {"offset", b.offset}};
    }
    j["signals"] = m;
    std::ofstream out(path);
    if (!out) return false;
    out << j.dump(2) << '\n';
    return static_cast<bool>(out);
}

void SourceConfig::clearSignal(FrameField field)
{
    bindings[static_cast<int>(field)] = SignalBinding{};
}

void SourceConfig::assignSignal(FrameField field, const std::string& signal)
{
    const int target = static_cast<int>(field);
    for (int i = 0; i < kFieldCount; ++i)
        if (i != target && SameSignal(bindings[i].signal, signal)) clearSignal(static_cast<FrameField>(i));
    bindings[target].signal = signal;
}

std::unique_ptr<TelemetrySource> CreateSource(const SourceConfig& config)
{
    if (config.kind == SourceConfig::Kind::Udp) return std::make_unique<UdpSource>(config.udp, config.bindings);
    return CreateSimulatedSource();
}
