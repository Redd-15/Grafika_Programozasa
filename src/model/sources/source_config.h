// Model: which source feeds the dashboard and, for UDP, how received CAN signals
// map onto dashboard values. Persisted as JSON.
#pragma once
#include <array>
#include <memory>
#include <string>

#include "model/sources/signal_mapping.h"
#include "model/sources/telemetry_source.h"

struct UdpConfig {
    std::string bindAddress = "0.0.0.0";
    int port = 5005;
    double timeoutSec = 1.0;   // no packet for this long = "no data"
};

using SignalBindings = std::array<SignalBinding, kFieldCount>;

struct SourceConfig {
    enum class Kind { Simulated, Udp };
    Kind kind = Kind::Simulated;
    UdpConfig udp;
    SignalBindings bindings;

    std::string path;   // where load()/save() read and write; not stored in the file

    bool load(const std::string& file);
    bool save() const;

    // Binds `signal` to `field` and unbinds it from every other field, so one signal
    // drives at most one dashboard value.
    void assignSignal(FrameField field, const std::string& signal);
    // Unbinds a field (empty signal, scale 1, offset 0).
    void clearSignal(FrameField field);
};

std::unique_ptr<TelemetrySource> CreateSource(const SourceConfig& config);
