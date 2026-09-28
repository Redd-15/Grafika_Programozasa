// Alarm thresholds and bar ranges, mirroring the car dash config (<consts>).
#pragma once

namespace limits {

// Temperature thresholds in degC (tile turns amber at warn, red at crit).
constexpr float bat_temp_warn = 50;
constexpr float bat_temp_crit = 58;
constexpr float inv_temp_warn = 65;
constexpr float inv_temp_crit = 75;
constexpr float mot_temp_warn = 90;
constexpr float mot_temp_crit = 115;

// HV state of charge in % (tile turns amber below warn, red below crit).
constexpr float hv_soc_warn = 20;
constexpr float hv_soc_crit = 10;

// Motor current range in A (bar is symmetrical around 0; negative = regen).
// Named hv_current_* in the dash config.
constexpr float hv_current_min = -340;
constexpr float hv_current_max = 340;

// LV battery voltage in V (config stores 0.1 V units: 100 / 145 / 120 / 110).
constexpr float lv_voltage_min = 10.0f;
constexpr float lv_voltage_max = 14.5f;
constexpr float lv_voltage_warn = 12.0f;
constexpr float lv_voltage_crit = 11.0f;

} // namespace limits
