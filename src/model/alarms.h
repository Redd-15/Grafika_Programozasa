// Model: decides how serious a value is. The view only maps the result to colours.
#pragma once

#include "model/car_state.h"

enum class AlarmLevel { Ok = 0, Warning = 1, Critical = 2 };

AlarmLevel MotorTempAlarm(float degC);
AlarmLevel CellTempAlarm(float degC);
AlarmLevel InverterTempAlarm(float degC);
AlarmLevel LvVoltageAlarm(float volts);
AlarmLevel SocAlarm(float percent);
AlarmLevel UnbalanceAlarm(float percent);
AlarmLevel RpmAlarm(float rpm, float rpmLimit);
// Undervoltage and start measurement are normal while the TS is off, so only warnings.
AlarmLevel BenderAlarm(BenderState state);
