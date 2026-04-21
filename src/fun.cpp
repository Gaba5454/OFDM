// fun.cpp
#include <iostream>
#include <fftw3.h>
#include <math.h>
#include <complex>
#include <vector>
#include <bitset>
#include <fstream>
#include <random>
#include <algorithm>
#include <cmath>

// Библиотеки для GUI
#include "imgui.h"
#include "implot.h"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include "imgui_impl_sdl2.h"        
#include "imgui_impl_opengl3.h"    

#define FFT_SIZE 128
#define CP_LEN   10
#define TOTAL_LEN (FFT_SIZE + CP_LEN) // 138

using ComplexSymbol = std::complex<double>;

// === Глобальные данные ===
struct OFDMData {
    std::vector<int8_t> bits;
    std::vector<ComplexSymbol> symbols;
    std::vector<ComplexSymbol> ofdm_time;
    std::vector<ComplexSymbol> with_cp;
    std::vector<ComplexSymbol> after_channel;
    std::vector<double> correlation; // Результат корреляции
};

static OFDMData g_data;

// === Функции обработки сигнала ===

std::vector<int8_t> string_to_bits(const std::string& text) {
    std::vector<int8_t> bits;
    bits.reserve(text.size() * 8);
    for (unsigned char c : text) {  
        for (int i = 7; i >= 0; --i) {
            bits.push_back((c >> i) & 1);
        }
    }
    return bits;
}

std::vector<ComplexSymbol> QPSK(const std::vector<int8_t> &in_bits) {
    size_t num_symbols = in_bits.size() / 2;
    std::vector<ComplexSymbol> out_sym;
    out_sym.reserve(num_symbols);

    for(size_t i = 0; i < in_bits.size(); i+=2) {
        int8_t bit0 = in_bits[i];
        int8_t bit1 = in_bits[i+1];
        double i_component = (bit0 == 0) ? -1.0 : 1.0;
        double q_component = (bit1 == 0) ? -1.0 : 1.0;
        out_sym.emplace_back(i_component, q_component);
    }
    return out_sym;
}

std::vector<ComplexSymbol> IFFT_LTE(const std::vector<ComplexSymbol>& in_sym) {
    fftw_complex* in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);
    fftw_complex* out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);

    for(size_t i = 0; i < FFT_SIZE; ++i) {
        in[i][0] = 0.0; in[i][1] = 0.0;
    }

    size_t sym_idx = 0; 
    for (size_t i = 28; i <= 63 && sym_idx < in_sym.size(); ++i) {
        in[i][0] = in_sym[sym_idx].real();
        in[i][1] = in_sym[sym_idx].imag();
        ++sym_idx; 
    }
    for (size_t i = 65; i <= 100 && sym_idx < in_sym.size(); ++i) {
        in[i][0] = in_sym[sym_idx].real();
        in[i][1] = in_sym[sym_idx].imag();
        ++sym_idx;
    }

    fftw_plan plan = fftw_plan_dft_1d(FFT_SIZE, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_execute(plan);

    std::vector<ComplexSymbol> out_sig(FFT_SIZE);
    for(size_t i = 0; i < FFT_SIZE; ++i){
        out_sig[i] = ComplexSymbol(out[i][0] / FFT_SIZE, out[i][1] / FFT_SIZE); 
    }

    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);
    return out_sig;
}

std::vector<ComplexSymbol> cyclicPrefix(const std::vector<ComplexSymbol>& symbol, size_t cp_len) {
    if(cp_len >= symbol.size()){
        throw std::invalid_argument("Длина должна быть меньше размера символа.");
    }
    std::vector<ComplexSymbol> output;
    output.reserve(symbol.size()+cp_len);
    output.insert(output.end(), symbol.end() - cp_len, symbol.end());
    output.insert(output.end(), symbol.begin(), symbol.end());
    return output; 
}

std::vector<ComplexSymbol> channelSimulation(const std::vector<ComplexSymbol>& signal, double noise_stddev = 1.0) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::normal_distribution<double> dist(0.0, noise_stddev);

    std::vector<ComplexSymbol> result;
    result.reserve(signal.size());
    
    for (const auto& sample : signal) {
        double noise_re = dist(gen);
        double noise_im = dist(gen);
        result.emplace_back(sample.real() + noise_re, sample.imag() + noise_im);
    }
    return result;
}

// === АВТОКОРРЕЛЯЦИЯ для поиска ЦП ===
// Сравниваем sample[n] с sample[n + FFT_SIZE]
// Если это ЦП, они должны быть похожи (высокая корреляция)
std::vector<double> detect_cp_correlation(const std::vector<ComplexSymbol>& rx_signal) {
    std::vector<double> metric(TOTAL_LEN, 0.0);
    
    for (int n = 0; n < CP_LEN; ++n) {
        // Корреляция между началом (CP) и концом символа
        ComplexSymbol cp_sample = rx_signal[n];
        ComplexSymbol end_sample = rx_signal[n + FFT_SIZE];
        
        // Нормализованная корреляция
        double corr = std::real(std::conj(cp_sample) * end_sample);
        double energy = std::norm(cp_sample) + std::norm(end_sample);
        
        if (energy > 1e-10) {
            metric[n] = corr / energy; // Значение от -1 до 1
        }
    }
    return metric;
}


// === GUI ===

bool init_gui(SDL_Window*& window, SDL_GLContext& gl_context) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return false;
    }

    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_WindowFlags flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    window = SDL_CreateWindow("LTE OFDM Simulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 900, flags);
    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        return false;
    }

    gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); 

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "GLEW Init Error" << std::endl;
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.Fonts->AddFontDefault();

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    return true;
}

void cleanup_gui(SDL_Window* window, SDL_GLContext gl_context) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

// === Отрисовка с коррелятором ===
// === Отрисовка с коррелятором (ИСПРАВЛЕННАЯ) ===
void show_signal_windows() {
    ImGui::Begin("OFDM Signal Processing");
    
    // === ГЛАВНОЕ: КОРРЕЛЯЦИЯ для поиска ЦП ===
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "CP Detection via Autocorrelation");
    ImGui::Text("Pik korrelyatsii = nayden CP (pervye %d otschyotov)", CP_LEN);
    
    if (ImPlot::BeginPlot("CP Correlation", ImVec2(-1, 150))) {
        ImPlot::SetupAxes("Sample Index", "Correlation");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, TOTAL_LEN - 1, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -0.2, 1.2, ImGuiCond_Always);
        
        static std::vector<double> x_corr, y_corr;
        x_corr.clear(); y_corr.clear();
        for (size_t i = 0; i < g_data.correlation.size(); ++i) {
            x_corr.push_back((double)i);
            y_corr.push_back(g_data.correlation[i]);
        }
        
        ImPlot::PlotBars("Correlation", x_corr.data(), y_corr.data(), x_corr.size(), 0.0);
        ImPlot::EndPlot();
    }
    
    ImGui::Separator();
    
    // --- 1. Биты ---
    ImGui::Text("1. Bits (%zu bits)", g_data.bits.size());
    if (ImPlot::BeginPlot("Bits", ImVec2(-1, 120))) {
        ImPlot::SetupAxes("Bit Index", "Value");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, std::max((size_t)100, g_data.bits.size()), ImGuiCond_FirstUseEver);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -0.5, 1.5, ImGuiCond_Always);
        
        static std::vector<double> x_bits, y_bits;
        x_bits.clear(); y_bits.clear();
        for (size_t i = 0; i < g_data.bits.size(); ++i) {
            x_bits.push_back((double)i);
            y_bits.push_back((double)g_data.bits[i]);
        }
        ImPlot::PlotStems("Bits", x_bits.data(), y_bits.data(), x_bits.size());
        ImPlot::EndPlot();
    }
    
    // --- 2. OFDM во времени ---
    ImGui::Text("2. OFDM Time Domain - %zu samples", g_data.ofdm_time.size());
    if (ImPlot::BeginPlot("OFDM Time Domain", ImVec2(-1, 120))) {
        ImPlot::SetupAxes("Sample", "Amplitude");
        
        static std::vector<double> x_t, y_re, y_im;
        x_t.clear(); y_re.clear(); y_im.clear();
        for (size_t i = 0; i < g_data.ofdm_time.size(); ++i) {
            x_t.push_back((double)i);
            y_re.push_back(g_data.ofdm_time[i].real());
            y_im.push_back(g_data.ofdm_time[i].imag());
        }
        ImPlot::PlotLine("Real", x_t.data(), y_re.data(), x_t.size());
        ImPlot::PlotLine("Imag", x_t.data(), y_im.data(), x_t.size());
        ImPlot::EndPlot();
    }

    // --- 3. С циклическим префиксом ---
    ImGui::Text("3. With CP - %zu samples | CP: 0..%d", g_data.with_cp.size(), CP_LEN-1);
    if (ImPlot::BeginPlot("With CP", ImVec2(-1, 150))) {
        ImPlot::SetupAxes("Sample", "Amplitude");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, TOTAL_LEN - 1, ImGuiCond_Always);
        
        static std::vector<double> x_cp, y_cp_re, y_cp_im;
        x_cp.clear(); y_cp_re.clear(); y_cp_im.clear();
        for (size_t i = 0; i < g_data.with_cp.size(); ++i) {
            x_cp.push_back((double)i);
            y_cp_re.push_back(g_data.with_cp[i].real());
            y_cp_im.push_back(g_data.with_cp[i].imag());
        }
        ImPlot::PlotLine("Real", x_cp.data(), y_cp_re.data(), x_cp.size());
        ImPlot::PlotLine("Imag", x_cp.data(), y_cp_im.data(), x_cp.size());
        ImPlot::EndPlot();
    }

    // --- 4. После канала ---
    ImGui::Text("4. After Channel - %zu samples | CP: 0..%d", g_data.after_channel.size(), CP_LEN-1);
    if (ImPlot::BeginPlot("Received Signal", ImVec2(-1, 150))) {
        ImPlot::SetupAxes("Sample", "Amplitude");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, TOTAL_LEN - 1, ImGuiCond_Always);
        
        static std::vector<double> x_ch, y_ch_re, y_ch_im;
        x_ch.clear(); y_ch_re.clear(); y_ch_im.clear();
        for (size_t i = 0; i < g_data.after_channel.size(); ++i) {
            x_ch.push_back((double)i);
            y_ch_re.push_back(g_data.after_channel[i].real());
            y_ch_im.push_back(g_data.after_channel[i].imag());
        }
        ImPlot::PlotLine("Real (Noisy)", x_ch.data(), y_ch_re.data(), x_ch.size());
        ImPlot::PlotLine("Imag (Noisy)", x_ch.data(), y_ch_im.data(), x_ch.size());
        ImPlot::EndPlot();
    }

    // --- 5. Созвездие ---
    ImGui::Text("5. QPSK Constellation (%zu symbols)", g_data.symbols.size());
    if (ImPlot::BeginPlot("Constellation", ImVec2(300, 300))) {
        ImPlot::SetupAxes("I (Real)", "Q (Imag)");
        
        double max_val = 2.0;
        if (!g_data.symbols.empty()) {
            double re_max = -1e9, re_min = 1e9, im_max = -1e9, im_min = 1e9;
            for (const auto& s : g_data.symbols) {
                re_max = std::max(re_max, s.real()); re_min = std::min(re_min, s.real());
                im_max = std::max(im_max, s.imag()); im_min = std::min(im_min, s.imag());
            }
            max_val = std::max({std::abs(re_max), std::abs(re_min), std::abs(im_max), std::abs(im_min)}) + 0.5;
        }
        
        ImPlot::SetupAxisLimits(ImAxis_X1, -max_val, max_val, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -max_val, max_val, ImGuiCond_Always);
        
        static std::vector<double> re, im;
        re.clear(); im.clear();
        for (const auto& sym : g_data.symbols) {
            re.push_back(sym.real());
            im.push_back(sym.imag());
        }
        ImPlot::PlotScatter("QPSK", re.data(), im.data(), re.size());
        ImPlot::EndPlot();
    }

    ImGui::End();
}

// === Main ===
int main() {
    // 1. Подготовка данных
    std::string text = "BUREAU1440thebest!";
    
    g_data.bits = string_to_bits(text);
    g_data.symbols = QPSK(g_data.bits);
    g_data.ofdm_time = IFFT_LTE(g_data.symbols);
    
    const size_t CP_LENGTH = 10;
    g_data.with_cp = cyclicPrefix(g_data.ofdm_time, CP_LENGTH);
    
    // Шум поменьше, чтобы ЦП был лучше виден (SNR ~10dB)
    g_data.after_channel = channelSimulation(g_data.with_cp, 0.3);
    
    // 2. Вычисляем корреляцию для поиска ЦП
    g_data.correlation = detect_cp_correlation(g_data.after_channel);

    // 3. Инициализация GUI
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    if (!init_gui(window, gl_context)) return 1;

    // 4. Цикл отрисовки
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                running = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        show_signal_windows();

        ImGui::Render();
        glClearColor(0.15f, 0.15f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    cleanup_gui(window, gl_context);
    return 0;
}