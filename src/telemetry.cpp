#include "telemetry.h"

#include <algorithm>
#include <cmath>
#include <random>

const char* ToString(CarState s)
{
    switch (s) {
    case CarState::Off:     return "OFF";
    case CarState::Startup: return "STARTUP";
    case CarState::TSOn:    return "TSON";
    case CarState::R2D:     return "R2D";
    }
    return "?";
}

const char* ToString(TSState s)
{
    switch (s) {
    case TSState::Idle:      return "IDLE";
    case TSState::Precharge: return "PRECHARGE";
    case TSState::Active:    return "ACTIVE";
    case TSState::Error:     return "ERROR";
    }
    return "?";
}

const char* ToString(BenderState s)
{
    switch (s) {
    case BenderState::Normal:  return "NORMAL";
    case BenderState::Warning: return "WARNING";
    case BenderState::Fault:   return "FAULT";
    }
    return "?";
}

const char* ToString(CoolingMode m)
{
    switch (m) {
    case CoolingMode::Auto: return "AUTO";
    case CoolingMode::Off:  return "OFF";
    case CoolingMode::Low:  return "LOW";
    case CoolingMode::Mid:  return "MID";
    case CoolingMode::Max:  return "MAX";
    }
    return "?";
}

void TelemetryHistory::push(const TelemetryFrame& f)
{
    apps.push(f.appsPct);
    bps.push(f.bpsBar);
    steering.push(f.steeringDeg);
    motorTemp.push(f.motorTemp);
    cellTemp.push(f.maxCellTemp);
    inverterTemp.push(f.inverterTemp);
    packV.push(f.packV);
    lvVoltage.push(f.lvVoltage);
    speed.push(f.speedKmh);
    rpm.push(f.motorRpm);
}

Telemetry::Telemetry(std::unique_ptr<TelemetrySource> source) : source_(std::move(source)) {}

void Telemetry::update(double time, float dt)
{
    source_->update(time, dt, frame_);
    sampleAccum_ += dt;
    const float period = 1.0f / kHistoryRateHz;
    // Cap catch-up so a stalled frame does not flood the traces.
    int pushes = 0;
    while (sampleAccum_ >= period && pushes < 10) {
        history_.push(frame_);
        sampleAccum_ -= period;
        ++pushes;
    }
    if (pushes == 10) sampleAccum_ = 0.0f;
}

// ---------------------------------------------------------------------------
// Simulated car: runs a start-up sequence, drives a lap-like speed profile and
// shuts down again, repeating every kCycle seconds.
// ---------------------------------------------------------------------------
namespace {

float Smoothstep(float e0, float e1, float x)
{
    float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

class SimulatedSource final : public TelemetrySource {
public:
    const char* name() const override { return "SIMULATION"; }
    bool connected() const override { return true; }

    void update(double time, float dt, TelemetryFrame& f) override
    {
        constexpr float kCycle = 150.0f;
        const float t = static_cast<float>(std::fmod(time, kCycle));

        if (!initialised_) {
            f.motorTemp = f.inverterTemp = f.maxCellTemp = 24.0f;
            f.lvVoltage = 12.6f;
            initialised_ = true;
        }

        // --- State machine -------------------------------------------------
        if (t < 3.0f)            f.carState = CarState::Off;
        else if (t < 6.0f)       f.carState = CarState::Startup;
        else if (t < 9.0f)       f.carState = CarState::TSOn;
        else if (t < 138.0f)     f.carState = CarState::R2D;
        else if (t < 144.0f)     f.carState = CarState::TSOn;
        else                     f.carState = CarState::Off;

        switch (f.carState) {
        case CarState::Off:     f.tsState = TSState::Idle; break;
        case CarState::Startup: f.tsState = TSState::Precharge; break;
        default:                f.tsState = TSState::Active; break;
        }
        // A short insulation warning mid-run to exercise the alarm styling.
        f.bender = (t > 95.0f && t < 99.0f) ? BenderState::Warning : BenderState::Normal;

        // --- Driving profile ------------------------------------------------
        const float drive = Smoothstep(9.0f, 13.0f, t) * (1.0f - Smoothstep(130.0f, 137.0f, t));
        const float ts = t - 9.0f;
        const float vTarget = 58.0f + 34.0f * std::sin(0.35f * ts) + 12.0f * std::sin(0.9f * ts + 1.0f);
        const float accel = 34.0f * 0.35f * std::cos(0.35f * ts) + 12.0f * 0.9f * std::cos(0.9f * ts + 1.0f);

        f.speedKmh = std::max(0.0f, vTarget * drive);
        if (drive > 0.01f) {
            f.appsPct = std::clamp(25.0f + accel * 5.0f + noise(2.0f), 0.0f, 100.0f) * drive;
            f.bpsBar = accel < -3.0f ? std::clamp(-accel * 3.2f + noise(1.0f), 0.0f, 80.0f) * drive : 0.0f;
            if (f.bpsBar > 0.0f) f.appsPct = 0.0f;
        } else {
            f.appsPct = 0.0f;
            f.bpsBar = f.carState == CarState::Off ? 0.0f : 12.0f;   // holding the brake
        }
        f.steeringDeg = drive * (85.0f * std::sin(0.42f * ts) + 30.0f * std::sin(1.3f * ts + 0.5f)) + noise(0.8f);
        f.steeringDeg = std::clamp(f.steeringDeg, -120.0f, 120.0f);

        // --- Settings (the driver changes one now and then) ---------------
        CarSettings& s = f.settings;
        const int changeSlot = static_cast<int>(t / 20.0f);
        if (changeSlot != lastChangeSlot_) {
            lastChangeSlot_ = changeSlot;
            switch (changeSlot % 4) {
            case 0: s.powerLimitKw = 80.0f; s.motorCooling = CoolingMode::Auto; break;
            case 1: s.powerLimitKw = 60.0f; break;
            case 2: s.motorCooling = CoolingMode::Max; s.regenMaxA = 300; break;
            case 3: s.regenMaxA = 340; break;
            }
        }

        const float wheelCirc = 3.14159265f * s.wheelSizeMm / 1000.0f;
        const float gearRatio = 4.44f;
        f.motorRpm = std::min(f.speedKmh / 3.6f / wheelCirc * 60.0f * gearRatio, static_cast<float>(s.rpmLimit));

        // --- Thermal model (first-order lag towards a load-dependent target)
        const float load = f.appsPct / 100.0f;
        const float coolBoost = s.motorCooling == CoolingMode::Max ? 8.0f : 0.0f;
        lag(f.motorTemp,    28.0f + 78.0f * load - coolBoost, 18.0f, dt);
        lag(f.inverterTemp, 27.0f + 50.0f * load, 12.0f, dt);
        lag(f.maxCellTemp,  26.0f + 30.0f * load, 40.0f, dt);

        // --- Electrical --------------------------------------------------------
        const bool hv = f.tsState == TSState::Active || f.tsState == TSState::Precharge;
        // Drains fast enough to reach the SOC alarms within a few cycles; "recharged" while off.
        soc_ = std::max(0.03f, soc_ - load * dt * 0.0022f);
        if (f.carState == CarState::Off && soc_ < 0.08f) soc_ = 0.95f;
        f.socPct = soc_ * 100.0f;
        // Motor current: positive while driving, negative (regen) while the car decelerates
        // without throttle, limited by the REGEN MAX setting.
        float motorCurrent = load * 330.0f * (s.powerLimitKw / 80.0f);
        if (f.appsPct <= 0.0f && accel < 0.0f && f.speedKmh > 5.0f)
            motorCurrent = -std::min<float>(s.regenMaxA, -accel * 22.0f) * drive;
        f.motorCurrentA = hv ? std::clamp(motorCurrent, -340.0f, 340.0f) : 0.0f;
        const float ocv = 432.0f + 168.0f * soc_;
        float targetV = hv ? ocv - 55.0f * load : 0.0f;
        if (f.tsState == TSState::Precharge) targetV = ocv * Smoothstep(3.0f, 5.5f, t);
        lag(f.packV, targetV, 0.25f, dt);
        f.unbalancePct = hv ? 1.1f + 0.5f * load + 0.2f * std::sin(time * 0.3) : 0.0f;

        const float lvTarget = f.carState == CarState::Off ? 12.6f : 13.4f - 0.5f * load;
        lag(f.lvVoltage, lvTarget + noise(0.03f), 0.6f, dt);
    }

private:
    float noise(float amp) { return std::uniform_real_distribution<float>(-amp, amp)(rng_); }
    static void lag(float& v, float target, float tau, float dt) { v += (target - v) * std::min(1.0f, dt / tau); }

    std::mt19937 rng_{1234};
    float soc_ = 0.95f;
    int lastChangeSlot_ = -1;
    bool initialised_ = false;
};

} // namespace

std::unique_ptr<TelemetrySource> CreateSimulatedSource()
{
    return std::make_unique<SimulatedSource>();
}

void Telemetry::setSource(std::unique_ptr<TelemetrySource> source)
{
    source_ = std::move(source);
    frame_ = TelemetryFrame{};
    history_ = TelemetryHistory{};
    sampleAccum_ = 0.0f;
}
