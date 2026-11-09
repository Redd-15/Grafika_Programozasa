// Controller: every action the user can take on the data source (switch source,
// reconnect, edit the signal mapping, save). The data-source window calls these
// instead of changing the model itself.
#pragma once
#include <string>

#include "model/sources/source_config.h"
#include "model/sources/udp_source.h"
#include "model/telemetry.h"

class SourceController {
public:
    SourceController(SourceConfig& config, Telemetry& telemetry);

    // Read access for the view.
    const SourceConfig& config() const { return config_; }
    const Telemetry& telemetry() const { return telemetry_; }
    const UdpSource* udp() const;   // null while the simulator is active

    // Source selection
    void useSimulation();
    void useUdp(const UdpConfig& udp);   // switches to UDP, or reconnects with new settings

    // Signal mapping (applied to a running UDP source immediately)
    void setSignalText(FrameField field, const std::string& signal);   // while typing; may duplicate
    void assignSignal(FrameField field, const std::string& signal);    // final; unbinds it elsewhere
    void clearSignal(FrameField field);
    void setScaleOffset(FrameField field, float scale, float offset);

    bool save() const { return config_.save(); }

private:
    void restartSource();
    void pushBindings();

    SourceConfig& config_;
    Telemetry& telemetry_;
};
