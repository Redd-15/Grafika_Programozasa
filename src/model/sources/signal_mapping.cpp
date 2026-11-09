#include "model/sources/signal_mapping.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

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
    {"motor_cooling",    "Motor pump override %",    "Settings", false},
    {"inverter_cooling", "Inverter pump override %", "Settings", false},
    {"wheel_size_mm",    "Wheel size mm",      "Settings", false},
    {"max_recuperation_pct", "Max recuperation %", "Settings", false},
    {"frequency_profile",    "Frequency profile",  "Settings", true},
};

// "TS_ON", "ts on" and "TSON" all compare equal.
std::string Normalize(const std::string& s)
{
    std::string out;
    for (char ch : s)
        if (std::isalnum(static_cast<unsigned char>(ch))) out += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return out;
}

// Resolves an enum from its numeric CAN value, its DBC value name or its display name.
template <typename E>
bool ToEnum(const SignalValue& v, const SignalBinding& b, int count, E& out)
{
    if (!v.isNumber) {
        const std::string want = Normalize(v.text);
        for (int i = 0; i < count; ++i) {
            const E e = static_cast<E>(i);
            if (Normalize(DbcName(e)) == want || Normalize(ToString(e)) == want) {
                out = e;
                return true;
            }
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

} // namespace

const FieldInfo& GetFieldInfo(FrameField f) { return kFields[static_cast<int>(f)]; }

bool ApplyField(FrameField field, const SignalValue& v, const SignalBinding& b, TelemetryFrame& f)
{
    float x = 0.0f;
    CarSettings& s = f.settings;
    switch (field) {
    case FrameField::TsState:          return ToEnum(v, b, kTsStateCount, f.tsState);
    case FrameField::Bender:           return ToEnum(v, b, kBenderStateCount, f.bender);
    case FrameField::CarState:         return ToEnum(v, b, kCarStateCount, f.carState);
    case FrameField::FrequencyProfile: return ToEnum(v, b, kFrequencyProfileCount, s.frequencyProfile);
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
    case FrameField::MotorCooling:    s.motorCooling = static_cast<int>(std::lround(x)); break;
    case FrameField::InverterCooling: s.inverterCooling = static_cast<int>(std::lround(x)); break;
    case FrameField::WheelSizeMm:     s.wheelSizeMm = std::max(1, static_cast<int>(std::lround(x))); break;
    case FrameField::MaxRecuperationPct: s.maxRecuperationPct = static_cast<int>(std::lround(x)); break;
    default: return false;
    }
    return true;
}

bool SameSignal(const std::string& a, const std::string& b)
{
    if (a.empty() || b.empty()) return false;
    if (a == b) return true;
    const std::string& longer = a.size() > b.size() ? a : b;
    const std::string& shorter = a.size() > b.size() ? b : a;
    return longer.size() > shorter.size() + 1 && longer[longer.size() - shorter.size() - 1] == '.' &&
           longer.compare(longer.size() - shorter.size(), shorter.size(), shorter) == 0;
}
