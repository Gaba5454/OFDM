#include "cli_utils.h"

#include <iostream>

void print_usage(const char* prog_name)
{
    std::cout << "Usage:\n"
              << "  " << prog_name << " simulation\n"
              << "  " << prog_name << " realtime TX <device>\n"
              << "  " << prog_name << " realtime RX <device>\n"
              << "  " << prog_name << " TX <device>\n"
              << "  " << prog_name << " RX <device>\n"
              << "\nExamples:\n"
              << "  " << prog_name << " simulation\n"
              << "  " << prog_name << " realtime TX usb:1.10.5\n"
              << "  " << prog_name << " realtime RX usb:1.9.5\n"
              << "  " << prog_name << " TX usb:1.10.5\n"
              << "  " << prog_name << " RX usb:1.9.5\n";
}
