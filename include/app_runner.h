#pragma once

#include "modulation_map.h"

#include <string>

const ModulationSpec* load_default_modulation();
int run_realtime_cli(int argc, char* argv[], const ModulationSpec& modulation);
int run_radio_cli(const std::string& mode, int argc, char* argv[], const ModulationSpec& modulation);
