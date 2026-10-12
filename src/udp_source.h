// Telemetry over UDP: each datagram carries a JSON object with the CAN messages
// and signals decoded by the car's telemetry unit. The JSON is flattened into
// "path -> value" pairs (see FlattenJson) that the user binds to dashboard values.
#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "data_source.h"

class UdpSource final : public TelemetrySource {
public:
    using Bindings = std::array<SignalBinding, kFieldCount>;

    UdpSource(const UdpConfig& config, const Bindings& bindings);
    ~UdpSource() override;
    UdpSource(const UdpSource&) = delete;
    UdpSource& operator=(const UdpSource&) = delete;

    const char* name() const override { return name_.c_str(); }
    bool connected() const override;
    void update(double time, float dt, TelemetryFrame& frame) override;

    void setBindings(const Bindings& bindings);
    const UdpConfig& config() const { return config_; }

    struct Stats {
        bool listening = false;
        std::string error;
        std::uint64_t packets = 0, bytes = 0, parseErrors = 0;
        double lastPacketAge = -1.0;   // seconds, < 0 = nothing received yet
        double packetsPerSec = 0.0;
        std::string lastSender;
    };
    Stats stats() const;

    struct SignalRow {
        std::string path;
        SignalValue value;
        double age;    // seconds since last update
    };
    std::vector<SignalRow> signals() const;   // sorted by path
    std::string lastPacket() const;

    // Per dashboard field: which received path the binding resolved to, and whether it applied.
    enum class BindState { Unbound, Ok, Missing, BadValue };
    struct FieldStatus { BindState state = BindState::Unbound; std::string path; };
    FieldStatus fieldStatus(FrameField field) const;

private:
    struct Entry {
        SignalValue value;
        std::chrono::steady_clock::time_point time;
    };

    void run();
    void handlePacket(const char* data, int size, const std::string& sender);
    const std::string* resolve(int field);

    UdpConfig config_;
    std::string name_;
    std::intptr_t socket_ = -1;
    std::thread thread_;
    std::atomic<bool> running_{false};

    mutable std::mutex mutex_;
    std::map<std::string, Entry> signals_;
    std::uint64_t mapVersion_ = 0;   // bumps when a new path appears
    Bindings bindings_;
    std::array<FieldStatus, kFieldCount> status_;
    std::array<std::uint64_t, kFieldCount> resolvedVersion_{};
    Stats stats_;
    std::chrono::steady_clock::time_point lastPacketTime_;
    std::string lastPacket_;

    // Rate measurement (update thread only).
    std::uint64_t ratePackets_ = 0;
    double rateTime_ = -1.0;
};

// Flattens a JSON document into dotted paths. Arrays of objects use the
// element's "name" (or hex "id") as the path segment; {"name": n, "value": v}
// pairs become a single leaf "<parent>.n".
void FlattenJson(const std::string& text, std::vector<std::pair<std::string, SignalValue>>& out, bool* ok);
