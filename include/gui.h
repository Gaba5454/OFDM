#pragma once

#include "const.h"
#include <vector>
#include <string>
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <imgui.h>
#include <implot.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <iostream>
#include <numeric> 
#include <algorithm> 

void run_gui(
    const std::string& original_text,
    const std::vector<uint8_t>& raw_bits,
    const std::vector<CF>& qpsk_symbols,
    const std::vector<CF>& pss_signal,
    const std::vector<CF>& ofdm_symbols,
    const std::vector<CF>& ofdm_with_cp,
    const std::vector<CF>& tx_array,
    const std::vector<double>& correlation_map,
    size_t peak_position,
    const std::vector<CF>& data_after_pss,
    const std::string& recovered_text,             
    const std::vector<CF>& received_constellation  
);