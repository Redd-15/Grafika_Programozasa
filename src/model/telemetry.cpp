#include "model/telemetry.h"

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

void Telemetry::setSource(std::unique_ptr<TelemetrySource> source)
{
    source_ = std::move(source);
    frame_ = TelemetryFrame{};
    history_ = TelemetryHistory{};
    sampleAccum_ = 0.0f;
}
