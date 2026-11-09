// Model: anything that can produce telemetry frames (simulator, UDP, ...).
#pragma once

#include "model/telemetry_frame.h"

class TelemetrySource {
public:
    virtual ~TelemetrySource() = default;
    virtual const char* name() const = 0;
    virtual bool connected() const = 0;
    // Advance by dt seconds and write the newest values into `frame`.
    virtual void update(double time, float dt, TelemetryFrame& frame) = 0;
};
