// View: the "Data source" window (choose simulation or UDP, connection settings, signal
// mapping, live list of received signals). Every change goes through SourceController.
#pragma once
#include <string>

#include "model/sources/source_config.h"
#include "view/ui/style.h"

class SourceController;

class SourceWindow {
public:
    // Draws the window while *open is true; saves the configuration when it closes.
    void draw(bool* open, SourceController& controller, const UiFonts& fonts, float scale);

private:
    // UI-only state.
    bool wasOpen_ = false;
    UdpConfig edit_;          // connection settings being typed
    int selected_ = -1;       // mapping row a click on a received signal binds to (-1 = none)
    std::string filter_;      // received-signal filter
};
