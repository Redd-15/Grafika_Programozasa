#include "model/car_state.h"

const char* ToString(CarState s)
{
    switch (s) {
    case CarState::Startup: return "STARTUP";
    case CarState::Standby: return "STANDBY";
    case CarState::TSOn:    return "TS ON";
    case CarState::R2D:     return "R2D";
    case CarState::Error:   return "ERROR";
    }
    return "?";
}

const char* ToString(TSState s)
{
    switch (s) {
    case TSState::On:        return "TS ON";
    case TSState::Precharge: return "PRECHARGE";
    case TSState::Off:       return "TS OFF";
    case TSState::Error:     return "ERROR";
    }
    return "?";
}

const char* ToString(BenderState s)
{
    switch (s) {
    case BenderState::OffOrShort:       return "OFF/SHORT";
    case BenderState::Normal:           return "NORMAL";
    case BenderState::Undervoltage:     return "UNDERVOLT";
    case BenderState::StartMeasurement: return "MEASURING";
    case BenderState::DeviceError:      return "DEVICE ERR";
    case BenderState::ConnectionFault:  return "CONN FAULT";
    case BenderState::HardError:        return "HARD ERROR";
    }
    return "?";
}

const char* ToString(FrequencyProfile p)
{
    switch (p) {
    case FrequencyProfile::Auto:   return "AUTO";
    case FrequencyProfile::Low:    return "LOW";
    case FrequencyProfile::High:   return "HIGH";
    case FrequencyProfile::Manual: return "MANUAL";
    }
    return "?";
}

const char* DbcName(CarState s)
{
    static const char* n[] = {"STARTUP", "STANDBY", "TS_ON", "R2D", "ERROR"};
    return n[static_cast<int>(s)];
}

const char* DbcName(TSState s)
{
    static const char* n[] = {"TS_ON", "TS_PRECHARGE", "TS_OFF", "TS_ERROR"};
    return n[static_cast<int>(s)];
}

const char* DbcName(BenderState s)
{
    static const char* n[] = {"BENDER_IMD_OFF_OR_SHORT", "BENDER_NORMAL", "BENDER_UNDERVOLTAGE",
                              "BENDER_SPEED_START_MEASUREMENT", "BENDER_DEVICE_ERROR",
                              "BENDER_CONNECTION_FAULT", "BENDER_HARD_ERROR"};
    return n[static_cast<int>(s)];
}

const char* DbcName(FrequencyProfile p)
{
    static const char* n[] = {"FREQUENCY_PROFILE_AUTO", "FREQUENCY_PROFILE_LOW", "FREQUENCY_PROFILE_HIGH",
                              "FREQUENCY_PROFILE_MANUAL"};
    return n[static_cast<int>(p)];
}
