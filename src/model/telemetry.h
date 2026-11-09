// Model: owns the active data source, the latest frame and the history traces.
#pragma once
#include <memory>

#include "model/sources/telemetry_source.h"
#include "model/telemetry_frame.h"
#include "model/telemetry_history.h"

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
