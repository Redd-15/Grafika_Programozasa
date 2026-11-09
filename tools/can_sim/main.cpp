// can_sim: sends simulated car data over UDP in the same JSON format as the car's
// Node-RED flow: {"<DBC message>": {"<DBC signal>": value, ...}, ...}, with physical
// values (DBC factors already applied), raw counts where the car sends raw counts
// (steering) and enum numbers from the DBC value tables.
//
// The values come from the dashboard's own simulator (model/sources/simulated_source),
// so the dashboard should show the same lap as in SIMULATION mode, but going through the
// full UDP -> signal mapping path.
//
//   can_sim [--host 127.0.0.1] [--port 7777] [--rate 10] [--no-tsac] [--duration 0]
#include "model/sources/simulated_source.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
using SocketHandle = SOCKET;
#  define CLOSE_SOCKET closesocket
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
using SocketHandle = int;
#  define INVALID_SOCKET (-1)
#  define CLOSE_SOCKET ::close
#endif

using json = nlohmann::json;

namespace {

struct Options {
    std::string host = "127.0.0.1";
    int port = 7777;
    double rateHz = 10.0;
    bool tsac = true;        // false = accumulator not in the car: no AMS messages
    double duration = 0.0;   // seconds, 0 = run until closed
};

void PrintUsage()
{
    std::puts("Usage: can_sim [--host IP] [--port N] [--rate HZ] [--no-tsac] [--duration S]\n"
              "  --host      receiver address              (default 127.0.0.1)\n"
              "  --port      receiver UDP port             (default 7777)\n"
              "  --rate      packets per second            (default 10)\n"
              "  --no-tsac   leave out AMS messages, like the car without its accumulator\n"
              "  --duration  stop after S seconds          (default: run until Ctrl+C)");
}

bool ParseArgs(int argc, char** argv, Options& o)
{
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) { std::fprintf(stderr, "%s needs a value\n", name); return nullptr; }
            return argv[++i];
        };
        if (a == "--host")          { const char* v = next("--host"); if (!v) return false; o.host = v; }
        else if (a == "--port")     { const char* v = next("--port"); if (!v) return false; o.port = std::atoi(v); }
        else if (a == "--rate")     { const char* v = next("--rate"); if (!v) return false; o.rateHz = std::atof(v); }
        else if (a == "--duration") { const char* v = next("--duration"); if (!v) return false; o.duration = std::atof(v); }
        else if (a == "--no-tsac")  { o.tsac = false; }
        else if (a == "--help" || a == "-h") { PrintUsage(); std::exit(0); }
        else { std::fprintf(stderr, "Unknown option %s\n", a.c_str()); return false; }
    }
    if (o.port < 1 || o.port > 65535 || o.rateHz <= 0.0) {
        std::fprintf(stderr, "Invalid port or rate\n");
        return false;
    }
    return true;
}

// Rounds to the resolution of the DBC signal (its factor), like a real decoded value.
double Res(double v, double factor) { return std::round(v / factor) * factor; }

// Steering sensor: 420..1670 raw counts = -90..+90 deg (FSU_Data.SteeringAngle is raw).
int SteeringRaw(float deg)
{
    const double raw = (deg + 150.48) / 0.144;
    return static_cast<int>(std::lround(std::fmin(1670.0, std::fmax(420.0, raw))));
}

// One packet: every message the car's telemetry forwards, encoded per DBC ERMe01 v2.4
// (plus the newer MPCU settings 13/14 from the firmware).
json Encode(const TelemetryFrame& f, double t, bool tsac)
{
    const CarSettings& s = f.settings;
    const double wheelRpm = f.speedKmh / 3.6 / (3.14159265 * s.wheelSizeMm / 1000.0) * 60.0;
    const double wobble = std::sin(t * 7.0);   // sensor noise

    json j;
    j["APPS_NormalData"] = {{"AppsData", Res(f.appsPct, 0.1)},
                            {"Apps1Interpolated", Res(f.appsPct + 0.2 * wobble, 0.1)},
                            {"Apps2Interpolated", Res(f.appsPct - 0.2 * wobble, 0.1)}};

    j["MPCU_VehicleState"] = {{"VehicleState", static_cast<int>(f.carState)}};

    // 1 = ON, 3 = COMERROR (the AMS is unreachable without the accumulator).
    j["MPCU_SystemStatues"] = {{"AppsState", 1}, {"AmsState", tsac ? 1 : 3}, {"RsuState", 1}, {"FsuState", 1},
                               {"TelemetryState", 1}, {"InverterState", 1}, {"DashState", 1}};

    const bool hv = f.tsState == TSState::On || f.tsState == TSState::Precharge;
    j["MPCU_Values"] = {{"InverterTemperature", Res(f.inverterTemp, 0.1)},
                        {"MotorTemperature", Res(f.motorTemp, 0.1)},
                        {"MotorRpm", std::lround(f.motorRpm)},
                        {"LemCurrent", 900 + std::lround(5 * wobble)},   // raw sensor value
                        {"Fan1Speed", 40}, {"Fan2Speed", 40}, {"Fan3Speed", 40},
                        {"MotorPumpSpeed", s.motorCooling < 0 ? 60 : s.motorCooling},
                        {"InverterPumpSpeed", s.inverterCooling < 0 ? 60 : s.inverterCooling},
                        {"InverterErrorStates", 0},
                        {"InverterWarningStates", 0},
                        {"BusVoltage", hv ? std::lround(f.packV) : 0},
                        {"SpeedKmph", Res(f.speedKmh, 0.1)}};

    j["MPCU_Settings"] = {{"PowerLimiter", std::lround(s.powerLimitKw * 1000.0f)},   // W
                          {"MaxMotorRpm", s.rpmLimit},
                          {"MotorPumpOverride", s.motorCooling},                   // -1 = auto
                          {"InverterPumpOverride", s.inverterCooling},
                          {"WheelDiameter", s.wheelSizeMm},
                          {"MaxRecuperationRate", s.maxRecuperationPct},
                          {"FrequencyProfile", static_cast<int>(s.frequencyProfile)}};

    j["FSU_Data"] = {{"LWheelRpm", std::lround(wheelRpm)}, {"RWheelRpm", std::lround(wheelRpm)},
                     {"LSuspension", 1085 + std::lround(4 * wobble)}, {"RSuspension", 1152 - std::lround(4 * wobble)},
                     {"SteeringAngle", SteeringRaw(f.steeringDeg)}};

    j["RSU_Bps"] = {{"Pressure", Res(f.bpsBar, 0.01)}};
    j["Dash_Status"] = {{"SwheelConnected", 1}};

    if (tsac) {
        const double cell = f.packV > 1.0f ? f.packV / 144.0 : 3.9;   // 144 cells in series
        j["AMS_Stat"] = {{"MinTemp", Res(f.maxCellTemp - 4.0, 0.1)},
                         {"AvgTemp", Res(f.maxCellTemp - 2.0, 0.1)},
                         {"MaxTemp", Res(f.maxCellTemp, 0.1)},
                         {"MinVoltage", Res(cell - 0.01, 0.0001)},
                         {"AvgVoltage", Res(cell, 0.0001)},
                         {"MaxVoltage", Res(cell + 0.01, 0.0001)},
                         {"SumVoltage", Res(f.packV, 0.0001)},
                         {"IsaVoltage", Res(f.packV, 0.001)},
                         {"IsaCurrent", Res(f.motorCurrentA, 0.001)},
                         {"IsaPower", std::lround(f.packV * f.motorCurrentA)},
                         {"Soc", Res(f.socPct, 0.1)},
                         {"TsState", static_cast<int>(f.tsState)},
                         {"BenderState", static_cast<int>(f.bender)},
                         {"UnbalancedRate", Res(f.unbalancePct, 0.1)},
                         {"KsVoltage", Res(f.lvVoltage, 0.1)}};
    }
    return j;
}

} // namespace

int main(int argc, char** argv)
{
    Options opt;
    if (!ParseArgs(argc, argv, opt)) {
        PrintUsage();
        return 1;
    }

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::fprintf(stderr, "Winsock initialisation failed\n");
        return 1;
    }
#endif
    SocketHandle sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(static_cast<unsigned short>(opt.port));
    if (sock == INVALID_SOCKET || ::inet_pton(AF_INET, opt.host.c_str(), &to.sin_addr) != 1) {
        std::fprintf(stderr, "Cannot send to %s\n", opt.host.c_str());
        return 1;
    }

    std::printf("Sending simulated CAN data to %s:%d at %.1f Hz%s. Ctrl+C to stop.\n", opt.host.c_str(), opt.port,
                opt.rateHz, opt.tsac ? "" : " (no TSAC: AMS messages left out)");

    auto source = CreateSimulatedSource();
    TelemetryFrame frame;
    const auto start = std::chrono::steady_clock::now();
    const auto period = std::chrono::duration<double>(1.0 / opt.rateHz);
    auto nextSend = start;
    double last = 0.0, lastPrint = -1.0;
    unsigned long long packets = 0;

    for (;;) {
        const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (opt.duration > 0.0 && t >= opt.duration) break;
        source->update(t, static_cast<float>(t - last), frame);
        last = t;

        const std::string payload = Encode(frame, t, opt.tsac).dump();
        if (::sendto(sock, payload.data(), static_cast<int>(payload.size()), 0, reinterpret_cast<sockaddr*>(&to),
                     sizeof to) < 0)
            std::fprintf(stderr, "send failed\n");
        ++packets;

        if (t - lastPrint >= 1.0) {   // one status line per second
            lastPrint = t;
            std::printf("\r%6.1f s  %-8s %5.1f km/h  steer %+6.1f deg (raw %4d)  %llu packets   ", t,
                        ToString(frame.carState), frame.speedKmh, frame.steeringDeg, SteeringRaw(frame.steeringDeg),
                        packets);
            std::fflush(stdout);
        }

        nextSend += std::chrono::duration_cast<std::chrono::steady_clock::duration>(period);
        std::this_thread::sleep_until(nextSend);
    }
    std::printf("\nSent %llu packets.\n", packets);
    CLOSE_SOCKET(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
