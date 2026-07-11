#include "app_runner.h"

#include "cli_utils.h"
#include "gui.h"
#include "modulation_map.h"
#include "ofdm_radio.h"
#include "simulation.h"

#include <iostream>
#include <string>

namespace {

constexpr const char* kDefaultModulationName = "BPSK";
constexpr const char* kDefaultText = "QWERTYUIOP{}ASDFGHJKKL:ZXCVBNM<>?";

int fail_with_usage(const char* program_name, const std::string& message)
{
    if (!message.empty()) {
        std::cerr << message << "\n\n";
    }
    print_usage(program_name);
    return 1;
}

int run_realtime_submode(
    const std::string& submode,
    const char* device_uri,
    const ModulationSpec& modulation)
{
    if (submode == "TX") {
        return run_realtime_tx_gui(device_uri, kDefaultText, modulation.name);
    }

    if (submode == "RX") {
        return run_realtime_rx_gui(device_uri, modulation.name);
    }

    return -1;
}

}  // namespace

const ModulationSpec* load_default_modulation()
{
    const ModulationSpec* modulation = find_modulation_spec(kDefaultModulationName);
    if (modulation == nullptr) {
        std::cerr << "Error: Unknown modulation '" << kDefaultModulationName << "' in app_runner.cpp\n";
    }
    return modulation;
}

int run_realtime_cli(int argc, char* argv[], const ModulationSpec& modulation)
{
    if (argc != 4) {
        return fail_with_usage(
            argv[0],
            "Error: realtime mode requires exactly TX/RX submode and device argument"
        );
    }

    const std::string submode = argv[2];
    const int submode_result = run_realtime_submode(submode, argv[3], modulation);
    if (submode_result >= 0) {
        return submode_result;
    }

    return fail_with_usage(argv[0], "Error: Unknown realtime submode '" + submode + "'");
}

int run_radio_cli(
    const std::string& mode,
    int argc,
    char* argv[],
    const ModulationSpec& modulation)
{
    if (mode != "TX" && mode != "RX") {
        return fail_with_usage(argv[0], "Error: Unknown mode '" + mode + "'");
    }

    if (argc != 3) {
        return fail_with_usage(argv[0], "Error: " + mode + " requires only device argument");
    }

    if (mode == "TX") {
        return run_realtime_tx_gui(argv[2], kDefaultText, modulation.name);
    }

    return run_realtime_rx_gui(argv[2], modulation.name);
}
