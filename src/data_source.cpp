#include "data_source.h"

#include "udp_source.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <type_traits>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

const FieldInfo kFields[kFieldCount] = {
    {"apps",             "APPS %",             "Driver inputs", false},
    {"steering_deg",     "Steering \xC2\xB0",  "Driver inputs", false},
    {"bps_bar",          "BPS bar",            "Driver inputs", false},
    {"motor_temp",       "Motor temp \xC2\xB0""C",    "Temperatures", false},
    {"max_cell_temp",    "Max cell temp \xC2\xB0""C", "Temperatures", false},
    {"inverter_temp",    "Inverter temp \xC2\xB0""C", "Temperatures", false},
    {"ts_state",         "TS state",           "Accumulator", true},
    {"bender",           "Bender",             "Accumulator", true},
    {"pack_v",           "Pack V",             "Accumulator", false},
    {"unbalance_pct",    "Unbalance %",        "Accumulator", false},
    {"soc_pct",          "SOC %",              "Accumulator", false},
    {"motor_current_a",  "Motor current A",    "Accumulator", false},
    {"lv_voltage",       "LV voltage V",       "Vehicle", false},
    {"car_state",        "Car state",          "Vehicle", true},
    {"speed_kmh",        "Speed km/h",         "Vehicle", false},
    {"motor_rpm",        "Motor rpm",          "Vehicle", false},
    {"power_limit_kw",   "Power limit kW",     "Settings", false},
    {"rpm_limit",        "RPM limit",          "Settings", false},
    {"motor_cooling",    "Motor cooling",      "Settings", true},
    {"inverter_cooling", "Inverter cooling",   "Settings", true},
    {"wheel_size_mm",    "Wheel size mm",      "Settings", false},
    {"regen_max_a",      "Regen max A",        "Settings", false},
};

// "TS_ON", "ts on" and "TSON" all compare equal.
std::string Normalize(const std::string& s)
{
    std::string out;
    for (char ch : s)
        if (std::isalnum(static_cast<unsigned char>(ch))) out += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return out;
}

// Resolves an enum from either its name or its numeric index.
template <typename E>
bool ToEnum(const SignalValue& v, const SignalBinding& b, int count, E& out)
{
    if (!v.isNumber) {
        const std::string want = Normalize(v.text);
        for (int i = 0; i < count; ++i) {
            if (Normalize(ToString(static_cast<E>(i))) == want) {
                out = static_cast<E>(i);
                return true;
            }
        }
        if constexpr (std::is_same_v<E, CarState>) {
            if (want == "READYTODRIVE") { out = CarState::R2D; return true; }
        }
        char* end = nullptr;
        double d = std::strtod(v.text.c_str(), &end);
        if (end == v.text.c_str()) return false;
        SignalValue num;
        num.number = d;
        return ToEnum(num, b, count, out);
    }
    const long idx = std::lround(v.number * b.scale + b.offset);
    if (idx < 0 || idx >= count) return false;
    out = static_cast<E>(idx);
    return true;
}

bool ToNumber(const SignalValue& v, const SignalBinding& b, float& out)
{
    double raw = v.number;
    if (!v.isNumber) {
        char* end = nullptr;
        raw = std::strtod(v.text.c_str(), &end);
        if (end == v.text.c_str()) return false;
    }
    out = static_cast<float>(raw * b.scale + b.offset);
    return std::isfinite(out);
}

const char* KindName(SourceConfig::Kind k) { return k == SourceConfig::Kind::Udp ? "udp" : "simulated"; }

} // namespace

const FieldInfo& GetFieldInfo(FrameField f) { return kFields[static_cast<int>(f)]; }

bool ApplyField(FrameField field, const SignalValue& v, const SignalBinding& b, TelemetryFrame& f)
{
    float x = 0.0f;
    CarSettings& s = f.settings;
    switch (field) {
    case FrameField::TsState:         return ToEnum(v, b, 4, f.tsState);
    case FrameField::Bender:          return ToEnum(v, b, 3, f.bender);
    case FrameField::CarState:        return ToEnum(v, b, 4, f.carState);
    case FrameField::MotorCooling:    return ToEnum(v, b, 5, s.motorCooling);
    case FrameField::InverterCooling: return ToEnum(v, b, 5, s.inverterCooling);
    default: break;
    }
    if (!ToNumber(v, b, x)) return false;
    switch (field) {
    case FrameField::Apps:            f.appsPct = x; break;
    case FrameField::SteeringDeg:     f.steeringDeg = x; break;
    case FrameField::BpsBar:          f.bpsBar = x; break;
    case FrameField::MotorTemp:       f.motorTemp = x; break;
    case FrameField::MaxCellTemp:     f.maxCellTemp = x; break;
    case FrameField::InverterTemp:    f.inverterTemp = x; break;
    case FrameField::PackV:           f.packV = x; break;
    case FrameField::UnbalancePct:    f.unbalancePct = x; break;
    case FrameField::SocPct:          f.socPct = x; break;
    case FrameField::MotorCurrentA:   f.motorCurrentA = x; break;
    case FrameField::LvVoltage:       f.lvVoltage = x; break;
    case FrameField::SpeedKmh:        f.speedKmh = x; break;
    case FrameField::MotorRpm:        f.motorRpm = x; break;
    case FrameField::PowerLimitKw:    s.powerLimitKw = x; break;
    case FrameField::RpmLimit:        s.rpmLimit = static_cast<int>(std::lround(x)); break;
    case FrameField::WheelSizeMm:     s.wheelSizeMm = std::max(1, static_cast<int>(std::lround(x))); break;
    case FrameField::RegenMaxA:       s.regenMaxA = static_cast<int>(std::lround(x)); break;
    default: return false;
    }
    return true;
}

std::string FormatField(FrameField field, const TelemetryFrame& f)
{
    char buf[48];
    const CarSettings& s = f.settings;
    auto num = [&](double v, const char* fmt = "%.2f") { std::snprintf(buf, sizeof buf, fmt, v); return std::string(buf); };
    switch (field) {
    case FrameField::Apps:            return num(f.appsPct, "%.1f");
    case FrameField::SteeringDeg:     return num(f.steeringDeg, "%.1f");
    case FrameField::BpsBar:          return num(f.bpsBar, "%.1f");
    case FrameField::MotorTemp:       return num(f.motorTemp, "%.1f");
    case FrameField::MaxCellTemp:     return num(f.maxCellTemp, "%.1f");
    case FrameField::InverterTemp:    return num(f.inverterTemp, "%.1f");
    case FrameField::TsState:         return ToString(f.tsState);
    case FrameField::Bender:          return ToString(f.bender);
    case FrameField::PackV:           return num(f.packV, "%.1f");
    case FrameField::UnbalancePct:    return num(f.unbalancePct);
    case FrameField::SocPct:          return num(f.socPct, "%.1f");
    case FrameField::MotorCurrentA:   return num(f.motorCurrentA, "%.1f");
    case FrameField::LvVoltage:       return num(f.lvVoltage);
    case FrameField::CarState:        return ToString(f.carState);
    case FrameField::SpeedKmh:        return num(f.speedKmh, "%.1f");
    case FrameField::MotorRpm:        return num(f.motorRpm, "%.0f");
    case FrameField::PowerLimitKw:    return num(s.powerLimitKw, "%.0f");
    case FrameField::RpmLimit:        return std::to_string(s.rpmLimit);
    case FrameField::MotorCooling:    return ToString(s.motorCooling);
    case FrameField::InverterCooling: return ToString(s.inverterCooling);
    case FrameField::WheelSizeMm:     return std::to_string(s.wheelSizeMm);
    case FrameField::RegenMaxA:       return std::to_string(s.regenMaxA);
    default: return {};
    }
}

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
                const char* key = kFields[i].key;
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
        m[kFields[i].key] = {{"signal", b.signal}, {"scale", b.scale}, {"offset", b.offset}};
    }
    j["signals"] = m;
    std::ofstream out(path);
    if (!out) return false;
    out << j.dump(2) << '\n';
    return static_cast<bool>(out);
}

std::unique_ptr<TelemetrySource> CreateSource(const SourceConfig& config)
{
    if (config.kind == SourceConfig::Kind::Udp) return std::make_unique<UdpSource>(config.udp, config.bindings);
    return CreateSimulatedSource();
}
