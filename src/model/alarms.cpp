#include "model/alarms.h"

#include "model/display_limits.h"

namespace {

AlarmLevel Above(float v, float warn, float crit)
{
    return v >= crit ? AlarmLevel::Critical : v >= warn ? AlarmLevel::Warning : AlarmLevel::Ok;
}

AlarmLevel Below(float v, float warn, float crit)
{
    return v < crit ? AlarmLevel::Critical : v < warn ? AlarmLevel::Warning : AlarmLevel::Ok;
}

} // namespace

AlarmLevel MotorTempAlarm(float t) { return Above(t, limits::mot_temp_warn, limits::mot_temp_crit); }
AlarmLevel CellTempAlarm(float t) { return Above(t, limits::bat_temp_warn, limits::bat_temp_crit); }
AlarmLevel InverterTempAlarm(float t) { return Above(t, limits::inv_temp_warn, limits::inv_temp_crit); }
AlarmLevel LvVoltageAlarm(float v) { return Below(v, limits::lv_voltage_warn, limits::lv_voltage_crit); }
AlarmLevel SocAlarm(float p) { return Below(p, limits::hv_soc_warn, limits::hv_soc_crit); }
AlarmLevel UnbalanceAlarm(float p) { return Above(p, limits::unbalance_warn, limits::unbalance_crit); }

AlarmLevel RpmAlarm(float rpm, float limit)
{
    return Above(rpm, limit * limits::rpm_warn_fraction, limit * limits::rpm_crit_fraction);
}

AlarmLevel BenderAlarm(BenderState s)
{
    switch (s) {
    case BenderState::Normal:           return AlarmLevel::Ok;
    case BenderState::Undervoltage:
    case BenderState::StartMeasurement: return AlarmLevel::Warning;
    default:                            return AlarmLevel::Critical;
    }
}
