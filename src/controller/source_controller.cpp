#include "controller/source_controller.h"

SourceController::SourceController(SourceConfig& config, Telemetry& telemetry) : config_(config), telemetry_(telemetry) {}

const UdpSource* SourceController::udp() const
{
    return dynamic_cast<const UdpSource*>(&telemetry_.source());
}

void SourceController::restartSource()
{
    telemetry_.setSource(CreateSource(config_));
    config_.save();
}

void SourceController::useSimulation()
{
    if (config_.kind == SourceConfig::Kind::Simulated) return;
    config_.kind = SourceConfig::Kind::Simulated;
    restartSource();
}

void SourceController::useUdp(const UdpConfig& udp)
{
    config_.kind = SourceConfig::Kind::Udp;
    config_.udp = udp;
    restartSource();
}

void SourceController::pushBindings()
{
    if (auto* udp = dynamic_cast<UdpSource*>(&telemetry_.source())) udp->setBindings(config_.bindings);
}

void SourceController::setSignalText(FrameField field, const std::string& signal)
{
    config_.bindings[static_cast<int>(field)].signal = signal;
    pushBindings();
}

void SourceController::assignSignal(FrameField field, const std::string& signal)
{
    config_.assignSignal(field, signal);
    pushBindings();
}

void SourceController::clearSignal(FrameField field)
{
    config_.clearSignal(field);
    pushBindings();
}

void SourceController::setScaleOffset(FrameField field, float scale, float offset)
{
    SignalBinding& b = config_.bindings[static_cast<int>(field)];
    b.scale = scale;
    b.offset = offset;
    pushBindings();
}
