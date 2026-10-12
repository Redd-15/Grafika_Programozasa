// Telemetry data model, history buffers and data sources.
#pragma once
#include <array>
#include <bitset>
#include <cstddef>
#include <memory>

// The enum values equal the raw CAN values in the car's DBC (ERMe01 v2.4), so a
// received number maps straight onto them.

// MPCU_VehicleState.VehicleState (Error = 4 is only listed for MPCU_StateRequestReq).
enum class CarState { Startup = 0, Standby = 1, TSOn = 2, R2D = 3, Error = 4 };
// AMS_Stat.TsState
enum class TSState { On = 0, Precharge = 1, Off = 2, Error = 3 };
// AMS_Stat.BenderState (insulation monitoring device)
enum class BenderState {
    OffOrShort = 0, Normal = 1, Undervoltage = 2, StartMeasurement = 3,
    DeviceError = 4, ConnectionFault = 5, HardError = 6
};
// MPCU setting 14 (enum frequency_profile in the MPCU firmware; newer than DBC v2.4).
enum class FrequencyProfile { Auto = 0, Low = 1, High = 2, Manual = 3 };   // Low = high torque
constexpr int kCarStateCount = 5, kTsStateCount = 4, kBenderStateCount = 7, kFrequencyProfileCount = 4;

// 0 = OK, 1 = warning, 2 = fault
int Severity(BenderState s);

const char* ToString(CarState s);
const char* ToString(TSState s);
const char* ToString(BenderState s);
const char* ToString(FrequencyProfile p);

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

// Fixed-size ring buffer of samples, read oldest -> newest. NaN marks "no data".
template <std::size_t N>
class History {
public:
    void push(float v) { data_[head_] = v; head_ = (head_ + 1) % N; if (count_ < N) ++count_; }
    std::size_t size() const { return count_; }
    static constexpr std::size_t capacity() { return N; }
    float operator[](std::size_t i) const { return data_[(head_ + N - count_ + i) % N]; }
    float latest() const { return count_ ? (*this)[count_ - 1] : 0.0f; }
private:
    std::array<float, N> data_{};
    std::size_t head_ = 0, count_ = 0;
};

constexpr float kHistoryRateHz = 20.0f;
constexpr std::size_t kHistorySamples = 600;   // 30 s window
using Trace = History<kHistorySamples>;

struct TelemetryHistory {
    Trace apps, bps, steering;
    Trace motorTemp, cellTemp, inverterTemp;
    Trace packV, lvVoltage, speed, rpm;

    void push(const TelemetryFrame& f);
};

// Anything that can produce frames: simulator / UDP.
class TelemetrySource {
public:
    virtual ~TelemetrySource() = default;
    virtual const char* name() const = 0;
    virtual bool connected() const = 0;
    // Advance by dt seconds and write the newest values into `frame`.
    virtual void update(double time, float dt, TelemetryFrame& frame) = 0;
};

std::unique_ptr<TelemetrySource> CreateSimulatedSource();

// Owns the source, the latest frame and the history traces.
class Telemetry {
public:
    explicit Telemetry(std::unique_ptr<TelemetrySource> source);
    void update(double time, float dt);

    const TelemetryFrame& frame() const { return frame_; }
    const TelemetryHistory& history() const { return history_; }
    const TelemetrySource& source() const { return *source_; }
    TelemetrySource& source() { return *source_; }
    // Swaps the data source and clears the frame and history.
    void setSource(std::unique_ptr<TelemetrySource> source);

private:
    std::unique_ptr<TelemetrySource> source_;
    TelemetryFrame frame_;
    TelemetryHistory history_;
    float sampleAccum_ = 0.0f;
};
