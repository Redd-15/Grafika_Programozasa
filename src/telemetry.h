// Telemetry data model, history buffers and data sources.
#pragma once
#include <array>
#include <cstddef>
#include <memory>

enum class CarState { Off, Startup, TSOn, R2D };
enum class TSState { Idle, Precharge, Active, Error };
enum class BenderState { Normal, Warning, Fault };       // insulation monitor (IMD)
enum class CoolingMode { Auto, Off, Low, Mid, Max };

const char* ToString(CarState s);
const char* ToString(TSState s);
const char* ToString(BenderState s);
const char* ToString(CoolingMode m);

// Values set on the car's dashboard (shown read-only).
struct CarSettings {
    float powerLimitKw = 80.0f;
    int rpmLimit = 5500;
    CoolingMode motorCooling = CoolingMode::Auto;
    CoolingMode inverterCooling = CoolingMode::Auto;
    int wheelSizeMm = 450;          // tyre outer diameter
    int regenMaxA = 340;

    static constexpr int kCount = 6;
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
    TSState tsState = TSState::Idle;
    BenderState bender = BenderState::Normal;
    float packV = 0.0f;
    float unbalancePct = 0.0f;
    float socPct = 0.0f;            // HV state of charge
    float motorCurrentA = 0.0f;     // + = driving, - = regen

    // Vehicle
    float lvVoltage = 0.0f;         // low-voltage system, 10..15 V
    CarState carState = CarState::Off;
    float speedKmh = 0.0f;
    float motorRpm = 0.0f;

    CarSettings settings;
};

// Fixed-size ring buffer of samples, read oldest -> newest.
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
