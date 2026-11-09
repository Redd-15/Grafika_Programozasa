// Model: one snapshot of everything the dashboard shows.
#pragma once
#include <bitset>
#include <cstddef>

#include "model/car_state.h"

// Every value on the dashboard. Used for the UDP signal mapping and to track
// which values are actually being received.
enum class FrameField {
    Apps, SteeringDeg, BpsBar,
    MotorTemp, MaxCellTemp, InverterTemp,
    TsState, Bender, PackV, UnbalancePct, SocPct, MotorCurrentA,
    LvVoltage, CarState, SpeedKmh, MotorRpm,
    PowerLimitKw, RpmLimit, MotorCooling, InverterCooling, WheelSizeMm, MaxRecuperationPct,
    FrequencyProfile,
    Count
};
constexpr int kFieldCount = static_cast<int>(FrameField::Count);

// Values set on the car's dashboard (shown read-only).
struct CarSettings {
    float powerLimitKw = 80.0f;
    int rpmLimit = 5500;
    int motorCooling = -1;          // pump override in %, -1 = AUTO (MPCU_Settings)
    int inverterCooling = -1;
    int wheelSizeMm = 450;          // tyre outer diameter
    int maxRecuperationPct = 100;   // MPCU setting 13, 0..100 %
    FrequencyProfile frequencyProfile = FrequencyProfile::Auto;

    static constexpr int kCount = 7;
};

struct TelemetryFrame {
    // Driver inputs
    float appsPct = 0.0f;           // accelerator pedal position sensor, 0..100 %
    float steeringDeg = 0.0f;       // steering wheel angle, + = right
    float bpsBar = 0.0f;            // brake pressure

    // Temperatures (degC)
    float motorTemp = 0.0f;
    float maxCellTemp = 0.0f;
    float inverterTemp = 0.0f;

    // Accumulator
    TSState tsState = TSState::Off;
    BenderState bender = BenderState::Normal;
    float packV = 0.0f;
    float unbalancePct = 0.0f;
    float socPct = 0.0f;            // HV state of charge
    float motorCurrentA = 0.0f;     // + = driving, - = regen

    // Vehicle
    float lvVoltage = 0.0f;         // low-voltage system, 10..15 V
    CarState carState = CarState::Startup;
    float speedKmh = 0.0f;
    float motorRpm = 0.0f;

    CarSettings settings;

    // Which values hold real data. Values the source does not provide (unbound,
    // not received or timed out) are shown as "--" and never raise alarms.
    std::bitset<kFieldCount> valid;
    bool has(FrameField f) const { return valid[static_cast<std::size_t>(f)]; }
};
