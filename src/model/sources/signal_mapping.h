// Model: how received CAN signals turn into dashboard values (binding, scaling,
// enum decoding).
#pragma once
#include <string>

#include "model/telemetry_frame.h"

// Describes one dashboard value in the mapping.
struct FieldInfo {
    const char* key;      // stable id used in the config file
    const char* label;    // human-readable name
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

// True when two binding strings name the same signal: equal, or one is a ".suffix" of the
// other ("SOC" and "BMS_Status.SOC").
bool SameSignal(const std::string& a, const std::string& b);
