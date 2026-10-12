// Modal window for choosing and configuring the telemetry data source.
#pragma once

class Telemetry;
struct SourceConfig;
struct UiFonts;

// Draws the window while *open is true. Changes apply immediately (the
// telemetry source is swapped/reconnected) and are saved when the window closes.
void DrawSourceWindow(bool* open, SourceConfig& config, Telemetry& telemetry, const UiFonts& fonts, float scale);
