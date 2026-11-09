// Model: the car's discrete states. The enum values equal the raw CAN values in the
// car's DBC (ERMe01 v2.4) and MPCU firmware, so a received number maps straight onto them.
#pragma once

// MPCU_VehicleState.VehicleState (Error = 4 is only listed for MPCU_StateRequestReq).
enum class CarState { Startup = 0, Standby = 1, TSOn = 2, R2D = 3, Error = 4 };
// AMS_Stat.TsState
enum class TSState { On = 0, Precharge = 1, Off = 2, Error = 3 };
// AMS_Stat.BenderState (insulation monitoring device)
enum class BenderState {
    OffOrShort = 0, Normal = 1, Undervoltage = 2, StartMeasurement = 3,
    DeviceError = 4, ConnectionFault = 5, HardError = 6
};
// MPCU setting 14 (enum frequency_profile in the MPCU firmware; newer than DBC v2.4).
enum class FrequencyProfile { Auto = 0, Low = 1, High = 2, Manual = 3 };   // Low = high torque
constexpr int kCarStateCount = 5, kTsStateCount = 4, kBenderStateCount = 7, kFrequencyProfileCount = 4;

// Display names.
const char* ToString(CarState s);
const char* ToString(TSState s);
const char* ToString(BenderState s);
const char* ToString(FrequencyProfile p);

// Value names from the DBC VAL_ tables / firmware enum (accepted when a signal arrives as text).
const char* DbcName(CarState s);
const char* DbcName(TSState s);
const char* DbcName(BenderState s);
const char* DbcName(FrequencyProfile p);
