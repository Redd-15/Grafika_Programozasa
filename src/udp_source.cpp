#include "udp_source.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
using SocketHandle = SOCKET;
#  define FS_INVALID_SOCKET INVALID_SOCKET
#  define FS_CLOSE_SOCKET closesocket
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
using SocketHandle = int;
#  define FS_INVALID_SOCKET (-1)
#  define FS_CLOSE_SOCKET ::close
#endif

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;

namespace {

constexpr std::size_t kMaxRawPreview = 16 * 1024;

std::string Join(const std::string& prefix, const std::string& key)
{
    return prefix.empty() ? key : prefix + "." + key;
}

bool IsLeaf(const json& j) { return j.is_number() || j.is_boolean() || j.is_string(); }

SignalValue ToValue(const json& j)
{
    SignalValue v;
    if (j.is_boolean()) {
        v.number = j.get<bool>() ? 1.0 : 0.0;
    } else if (j.is_number()) {
        v.number = j.get<double>();
    } else {
        v.isNumber = false;
        v.text = j.get<std::string>();
    }
    return v;
}

// Name for an array element: its "name", else its CAN id in hex, else the index.
std::string ElementSegment(const json& el, std::size_t index)
{
    if (el.is_object()) {
        auto n = el.find("name");
        if (n != el.end() && n->is_string()) return n->get<std::string>();
        auto id = el.find("id");
        if (id != el.end() && id->is_number_integer()) {
            char buf[24];
            std::snprintf(buf, sizeof buf, "0x%llX", static_cast<unsigned long long>(id->get<std::int64_t>()));
            return buf;
        }
    }
    return std::to_string(index);
}

void Flatten(const json& j, const std::string& prefix, std::vector<std::pair<std::string, SignalValue>>& out)
{
    if (j.is_object()) {
        // {"name": "SOC", "value": 55, ...} is one signal.
        auto n = j.find("name");
        auto v = j.find("value");
        if (n != j.end() && n->is_string() && v != j.end() && IsLeaf(*v)) {
            out.emplace_back(Join(prefix, n->get<std::string>()), ToValue(*v));
            return;
        }
        for (auto it = j.begin(); it != j.end(); ++it) Flatten(it.value(), Join(prefix, it.key()), out);
    } else if (j.is_array()) {
        for (std::size_t i = 0; i < j.size(); ++i) {
            const json& el = j[i];
            const bool namedValue = el.is_object() && el.contains("name") && el.contains("value");
            if (namedValue) {
                Flatten(el, prefix, out);
            } else {
                // A named element (e.g. a CAN message): drop its own "name" leaf, it is in the path now.
                std::string seg = ElementSegment(el, i);
                if (el.is_object() && el.contains("name")) {
                    for (auto it = el.begin(); it != el.end(); ++it)
                        if (it.key() != "name") Flatten(it.value(), Join(Join(prefix, seg), it.key()), out);
                } else {
                    Flatten(el, Join(prefix, seg), out);
                }
            }
        }
    } else if (IsLeaf(j)) {
        out.emplace_back(prefix.empty() ? std::string("value") : prefix, ToValue(j));
    }
}

#ifdef _WIN32
struct WinsockInit {
    bool ok;
    WinsockInit() { WSADATA d; ok = WSAStartup(MAKEWORD(2, 2), &d) == 0; }
    ~WinsockInit() { if (ok) WSACleanup(); }
};
int LastSocketError() { return WSAGetLastError(); }
bool IsAddrInUse(int e) { return e == WSAEADDRINUSE; }
#else
int LastSocketError() { return errno; }
bool IsAddrInUse(int e) { return e == EADDRINUSE; }
#endif

} // namespace

void FlattenJson(const std::string& text, std::vector<std::pair<std::string, SignalValue>>& out, bool* ok)
{
    json j = json::parse(text, nullptr, false);
    if (!j.is_discarded()) {
        Flatten(j, "", out);
        if (ok) *ok = true;
        return;
    }
    // Fall back to newline-delimited JSON (several objects in one datagram).
    bool any = false, failed = false;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        if (line.find_first_not_of(" \t\r") != std::string::npos) {
            json lj = json::parse(line, nullptr, false);
            if (lj.is_discarded()) failed = true;
            else { Flatten(lj, "", out); any = true; }
        }
        start = end + 1;
    }
    if (ok) *ok = any && !failed;
}

// ---------------------------------------------------------------------------
// UdpSource
// ---------------------------------------------------------------------------
UdpSource::UdpSource(const UdpConfig& config, const Bindings& bindings) : config_(config), bindings_(bindings)
{
#ifdef _WIN32
    static WinsockInit winsock;
    if (!winsock.ok) { stats_.error = "Winsock initialisation failed"; return; }
#endif
    name_ = "UDP :" + std::to_string(config_.port);

    SocketHandle s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == FS_INVALID_SOCKET) { stats_.error = "Could not create socket"; return; }

    int yes = 1;
    ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof yes);
    int bufSize = 1 << 20;
    ::setsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&bufSize), sizeof bufSize);
    // Short receive timeout so the thread notices shutdown.
#ifdef _WIN32
    DWORD timeoutMs = 200;
    ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof timeoutMs);
#else
    timeval tv{0, 200000};
    ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<unsigned short>(config_.port));
    if (config_.bindAddress.empty() || ::inet_pton(AF_INET, config_.bindAddress.c_str(), &addr.sin_addr) != 1) {
        if (!config_.bindAddress.empty() && config_.bindAddress != "0.0.0.0") {
            stats_.error = "Invalid bind address \"" + config_.bindAddress + "\"";
            FS_CLOSE_SOCKET(s);
            return;
        }
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) {
        const int e = LastSocketError();
        stats_.error = IsAddrInUse(e) ? "Port " + std::to_string(config_.port) + " is already in use"
                                      : "Could not bind " + config_.bindAddress + ":" + std::to_string(config_.port) +
                                            " (error " + std::to_string(e) + ")";
        FS_CLOSE_SOCKET(s);
        return;
    }

    socket_ = static_cast<std::intptr_t>(s);
    stats_.listening = true;
    running_ = true;
    thread_ = std::thread(&UdpSource::run, this);
}

UdpSource::~UdpSource()
{
    running_ = false;
    if (thread_.joinable()) thread_.join();
    if (socket_ != -1) FS_CLOSE_SOCKET(static_cast<SocketHandle>(socket_));
}

void UdpSource::run()
{
    std::vector<char> buffer(65536);
    const auto s = static_cast<SocketHandle>(socket_);
    while (running_) {
        sockaddr_in from{};
#ifdef _WIN32
        int fromLen = sizeof from;
#else
        socklen_t fromLen = sizeof from;
#endif
        const int n = static_cast<int>(::recvfrom(s, buffer.data(), static_cast<int>(buffer.size()), 0,
                                                  reinterpret_cast<sockaddr*>(&from), &fromLen));
        if (n <= 0) continue;   // timeout, or an ICMP error reported by Windows
        char ip[INET_ADDRSTRLEN] = "?";
        ::inet_ntop(AF_INET, &from.sin_addr, ip, sizeof ip);
        handlePacket(buffer.data(), n, std::string(ip) + ":" + std::to_string(ntohs(from.sin_port)));
    }
}

void UdpSource::handlePacket(const char* data, int size, const std::string& sender)
{
    const std::string text(data, static_cast<std::size_t>(size));
    std::vector<std::pair<std::string, SignalValue>> parsed;
    bool ok = false;
    FlattenJson(text, parsed, &ok);   // parse outside the lock

    const auto now = Clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.packets;
    stats_.bytes += static_cast<std::uint64_t>(size);
    if (!ok) ++stats_.parseErrors;
    stats_.lastSender = sender;
    lastPacketTime_ = now;
    lastPacket_ = text.size() > kMaxRawPreview ? text.substr(0, kMaxRawPreview) + "\n..." : text;
    for (auto& [path, value] : parsed) {
        auto [it, inserted] = signals_.try_emplace(path);
        it->second.value = std::move(value);
        it->second.time = now;
        if (inserted) ++mapVersion_;
    }
}

bool UdpSource::connected() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!stats_.listening || stats_.packets == 0) return false;
    return std::chrono::duration<double>(Clock::now() - lastPacketTime_).count() < config_.timeoutSec;
}

// Exact path first, otherwise the first path ending in ".<signal>". Cached until new paths arrive.
const std::string* UdpSource::resolve(int field)
{
    const std::string& want = bindings_[field].signal;
    FieldStatus& st = status_[field];
    if (resolvedVersion_[field] == mapVersion_) return st.path.empty() ? nullptr : &st.path;   // paths are never removed
    resolvedVersion_[field] = mapVersion_;
    st.path.clear();
    if (signals_.count(want)) {
        st.path = want;
    } else {
        const std::string suffix = "." + want;
        for (const auto& kv : signals_) {
            const std::string& p = kv.first;
            if (p.size() > suffix.size() && p.compare(p.size() - suffix.size(), suffix.size(), suffix) == 0) {
                st.path = p;
                break;
            }
        }
    }
    return st.path.empty() ? nullptr : &st.path;
}

void UdpSource::update(double time, float, TelemetryFrame& frame)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (int i = 0; i < kFieldCount; ++i) {
        FieldStatus& st = status_[i];
        if (bindings_[i].signal.empty()) {
            st = FieldStatus{};
            continue;
        }
        const std::string* path = resolve(i);
        if (!path) { st.state = BindState::Missing; continue; }
        st.state = ApplyField(static_cast<FrameField>(i), signals_[*path].value, bindings_[i], frame) ? BindState::Ok
                                                                                                      : BindState::BadValue;
    }

    // Packets per second over ~0.5 s windows.
    if (rateTime_ < 0.0) { rateTime_ = time; ratePackets_ = stats_.packets; }
    if (time - rateTime_ >= 0.5) {
        stats_.packetsPerSec = (stats_.packets - ratePackets_) / (time - rateTime_);
        rateTime_ = time;
        ratePackets_ = stats_.packets;
    }
}

void UdpSource::setBindings(const Bindings& bindings)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (int i = 0; i < kFieldCount; ++i) {
        if (bindings_[i].signal != bindings[i].signal) {
            resolvedVersion_[i] = ~0ull;   // force re-resolve
            status_[i].path.clear();
        }
    }
    bindings_ = bindings;
}

UdpSource::Stats UdpSource::stats() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    Stats s = stats_;
    if (s.packets) s.lastPacketAge = std::chrono::duration<double>(Clock::now() - lastPacketTime_).count();
    return s;
}

std::vector<UdpSource::SignalRow> UdpSource::signals() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = Clock::now();
    std::vector<SignalRow> rows;
    rows.reserve(signals_.size());
    for (const auto& [path, e] : signals_)
        rows.push_back({path, e.value, std::chrono::duration<double>(now - e.time).count()});
    return rows;
}

std::string UdpSource::lastPacket() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return lastPacket_;
}

UdpSource::FieldStatus UdpSource::fieldStatus(FrameField field) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return status_[static_cast<int>(field)];
}
