#pragma once

#include "gui.h"
#include "modulation_map.h"

GuiPlotData build_simulation_view(
    const std::string& text,
    double snr,
    const ModulationSpec& modulation);

void simulation(double snr, const ModulationSpec& modulation);
