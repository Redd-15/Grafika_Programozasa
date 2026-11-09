// Model: the last 30 s of selected values, for the charts.
#pragma once
#include <array>
#include <cstddef>

#include "model/telemetry_frame.h"

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
