// Data-source configuration: which source feeds the dashboard and, for UDP,
// how received CAN signals map onto dashboard values. Persisted as JSON.
#pragma once
#include <array>
#include <memory>
#include <string>

#include "telemetry.h"

struct FieldInfo {
    const char* key;      // stable id used in the config file
    const char* label;    // shown in the UI
    const char* group;
    bool isEnum;          // accepts the state name as text, or its index as a number
};
const FieldInfo& GetFieldInfo(FrameField f);

// A value received for one signal: numeric (numbers / booleans) or text.
struct SignalValue {
    bool isNumber = true;
    double number = 0.0;
    std::string text;
};

struct SignalBinding {
    std::string signal;   // full flattened path or a suffix of it, e.g. "BMS.SOC"
    float scale = 1.0f;   // value = raw * scale + offset
    float offset = 0.0f;
};

// Writes the bound value into the frame. Returns false if it could not be converted.
bool ApplyField(FrameField field, const SignalValue& value, const SignalBinding& binding, TelemetryFrame& frame);
// Current value of a field, for showing next to the mapping.
std::string FormatField(FrameField field, const TelemetryFrame& frame);

struct UdpConfig {
    std::string bindAddress = "0.0.0.0";
    int port = 5005;
    double timeoutSec = 1.0;   // no packet for this long = "no data"
};

struct SourceConfig {
    enum class Kind { Simulated, Udp };
    Kind kind = Kind::Simulated;
    UdpConfig udp;
    std::array<SignalBinding, kFieldCount> bindings;

    std::string path;   // where load()/save() read and write; not stored in the file

    bool load(const std::string& file);
    bool save() const;
};

// True when two binding strings name the same signal: equal, or one is a ".suffix" of the
// other ("SOC" and "BMS_Status.SOC").
bool SameSignal(const std::string& a, const std::string& b);
// Binds `signal` to `field` and unbinds it from every other field, so one signal
// drives at most one dashboard value. Unbound fields get scale 1 / offset 0 again.
void AssignSignal(SourceConfig& config, FrameField field, const std::string& signal);
// Unbinds a field (empty signal, scale 1, offset 0).
void ClearSignal(SourceConfig& config, FrameField field);

std::unique_ptr<TelemetrySource> CreateSource(const SourceConfig& config);
