#include "model/telemetry_history.h"

#include <cmath>

void TelemetryHistory::push(const TelemetryFrame& f)
{
    // Missing values are stored as NaN so charts leave a gap instead of drawing 0.
    auto v = [&](FrameField field, float value) { return f.has(field) ? value : std::nanf(""); };
    apps.push(v(FrameField::Apps, f.appsPct));
    bps.push(v(FrameField::BpsBar, f.bpsBar));
    steering.push(v(FrameField::SteeringDeg, f.steeringDeg));
    motorTemp.push(v(FrameField::MotorTemp, f.motorTemp));
    cellTemp.push(v(FrameField::MaxCellTemp, f.maxCellTemp));
    inverterTemp.push(v(FrameField::InverterTemp, f.inverterTemp));
    packV.push(v(FrameField::PackV, f.packV));
    lvVoltage.push(v(FrameField::LvVoltage, f.lvVoltage));
    speed.push(v(FrameField::SpeedKmh, f.speedKmh));
    rpm.push(v(FrameField::MotorRpm, f.motorRpm));
}
