#include "app_runner.h"
#include "cli_utils.h"
#include "simulation.h"

#include <string>

int main(int argc, char* argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const ModulationSpec* modulation = load_default_modulation();
    if (modulation == nullptr) {
        return 1;
    }

    const std::string mode = argv[1];
    if (mode == "simulation") {
        if (argc != 2) {
            print_usage(argv[0]);
            return 1;
        }
        simulation(30.0, *modulation);
        return 0;
    }

    if (mode == "realtime") {
        return run_realtime_cli(argc, argv, *modulation);
    }

    if (mode == "TX" || mode == "RX") {
        return run_radio_cli(mode, argc, argv, *modulation);
    }

    print_usage(argv[0]);
    return 1;
}
