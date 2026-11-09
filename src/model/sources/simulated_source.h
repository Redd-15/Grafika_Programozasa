// Model: a simulated car that runs a start-up sequence, drives a lap-like speed
// profile and shuts down again, repeating every 150 s.
#pragma once
#include <memory>

#include "model/sources/telemetry_source.h"

std::unique_ptr<TelemetrySource> CreateSimulatedSource();
