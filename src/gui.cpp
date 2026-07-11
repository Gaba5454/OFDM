#include "gui.h"
#include "frame_builder.h"
#include "frame_layout.h"
#include "modulation_map.h"
#include "ofdm_radio.h"
#include "receive.h"
#include "simulation.h"
#include "training_decoder.h"
#include "translate.h"

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <imgui.h>
#include <implot.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <thread>

namespace {

struct PreparedGuiData {
    std::vector<double> bits_x;
    std::vector<double> bits_y;
    std::vector<double> symbol_i;
    std::vector<double> symbol_q;
    std::vector<double> ideal_i;
    std::vector<double> ideal_q;
    std::vector<double> ofdm_time_real;
    std::vector<double> ofdm_time_imag;
    std::vector<double> ofdm_cp_real;
    std::vector<double> pss_real;
    std::vector<double> pss_imag;
    std::vector<double> tx_real;
    std::vector<double> tx_imag;
};

struct LinePlotData {
    std::vector<double> x;
    std::vector<double> y0;
    std::vector<double> y1;
    double x_min = 0.0;
    double x_max = 0.0;
    double y_min = -1.0;
    double y_max = 1.0;
    bool show_secondary = false;
    bool available = false;
};

struct ScatterPlotData {
    std::vector<double> x;
    std::vector<double> y;
    std::vector<double> ideal_x;
    std::vector<double> ideal_y;
    double axis_limit = 1.5;
    bool available = false;
};

struct ChannelPlotData {
    std::vector<double> x;
    std::vector<double> magnitude;
    std::vector<double> phase;
    bool available = false;
};

struct FrameViewData {
    LinePlotData frame_time;
    ScatterPlotData constellation;
    ChannelPlotData channel;
    std::string recovered_text;
    bool crc_ok = false;
    int spectrum_mode = 0;
    int subcarrier_shift = 0;
    int data_order = 0;
    double cfo = 0.0;
    bool available = false;
};

struct LiveViewData {
    LinePlotData rx_time;
    ScatterPlotData constellation;
    ChannelPlotData channel;
    size_t symbol_start = SIZE_MAX;
    double cp_score = 0.0;
    int spectrum_mode = 0;
    int subcarrier_shift = 0;
    bool available = false;
};

struct TxViewData {
    ScatterPlotData constellation;
    LinePlotData frame_time;
    LinePlotData training_time;
    size_t frame_samples = 0;
};

void complex_to_vectors(
    const std::vector<CF>& in,
    std::vector<double>& out_real,
    std::vector<double>& out_imag)
{
    out_real.resize(in.size());
    out_imag.resize(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        out_real[i] = in[i].real();
        out_imag[i] = in[i].imag();
    }
}

bool init_gui(SDL_Window*& window, SDL_GLContext& gl_context)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cout << "Error: SDL_Init failed\n";
        return false;
    }

    window = SDL_CreateWindow(
        "OFDM Visualization",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1920,
        1080,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    if (window == nullptr) {
        SDL_Quit();
        return false;
    }

    gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cout << "Error: GLEW init failed\n";
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImPlot::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");
    return true;
}

void shutdown_gui(SDL_Window* window, SDL_GLContext gl_context)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

PreparedGuiData prepare_gui_data(const GuiPlotData& data)
{
    PreparedGuiData prepared;

    prepared.bits_x.reserve(data.raw_bits.size() * 2);
    prepared.bits_y.reserve(data.raw_bits.size() * 2);
    for (size_t i = 0; i < data.raw_bits.size(); ++i) {
        prepared.bits_x.push_back(static_cast<double>(i));
        prepared.bits_x.push_back(static_cast<double>(i + 1));
        prepared.bits_y.push_back(static_cast<double>(data.raw_bits[i]));
        prepared.bits_y.push_back(static_cast<double>(data.raw_bits[i]));
    }

    prepared.symbol_i.reserve(data.modulated_symbols.size());
    prepared.symbol_q.reserve(data.modulated_symbols.size());
    for (const CF& symbol : data.modulated_symbols) {
        prepared.symbol_i.push_back(symbol.real());
        prepared.symbol_q.push_back(symbol.imag());
    }

    prepared.ideal_i.reserve(data.ideal_constellation.size());
    prepared.ideal_q.reserve(data.ideal_constellation.size());
    for (const CF& symbol : data.ideal_constellation) {
        prepared.ideal_i.push_back(symbol.real());
        prepared.ideal_q.push_back(symbol.imag());
    }

    complex_to_vectors(data.ofdm_symbols, prepared.ofdm_time_real, prepared.ofdm_time_imag);
    complex_to_vectors(data.pss_signal, prepared.pss_real, prepared.pss_imag);
    complex_to_vectors(data.tx_array, prepared.tx_real, prepared.tx_imag);

    prepared.ofdm_cp_real.reserve(data.ofdm_with_cp.size());
    for (const CF& symbol : data.ofdm_with_cp) {
        prepared.ofdm_cp_real.push_back(symbol.real());
    }

    return prepared;
}

void render_monitor(
    const GuiPlotData& data,
    const PreparedGuiData& prepared,
    bool& show_grid,
    int& plot_height,
    const std::function<void()>& draw_left_extras)
{
    ImGui::Begin("OFDM System Monitor");

    const float avail_width = ImGui::GetContentRegionAvail().x;
    const float left_w = avail_width * 0.25f;
    const float right_w = avail_width * 0.75f;

    ImGui::BeginChild("LeftPanel", ImVec2(left_w, 0), true);
    if (draw_left_extras) {
        draw_left_extras();
        ImGui::Spacing();
        ImGui::Separator();
    }

    const bool match = data.crc_ok && data.recovered_text == data.original_text;

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "INFORMATION");
    ImGui::Separator();
    ImGui::Text("Input text: %s", data.original_text.c_str());
    ImGui::Text("Recovered text: %s", data.recovered_text.c_str());
    ImGui::Text("CRC: %s", data.crc_ok ? "OK" : "FAIL");
    ImGui::Text("Match: %s", match ? "YES" : "NO");
    ImGui::Text("SNR: %.1f dB", data.snr);
    ImGui::Text("%s symbols: %zu", data.modulation_name.c_str(), data.modulated_symbols.size());
    ImGui::Text("Ideal constellation points: %zu", data.ideal_constellation.size());
    ImGui::Text("Raw bits: %zu", data.raw_bits.size());
    ImGui::Text("PSS length: %zu", data.pss_signal.size());
    ImGui::Text("OFDM symbol length: %zu", data.ofdm_symbols.size());
    ImGui::Text("CP length: %zu", data.ofdm_with_cp.size() - data.ofdm_symbols.size());
    if (data.max_text_bytes > 0) {
        ImGui::Text("Max text bytes: %zu", data.max_text_bytes);
    }
    if (data.text_was_truncated) {
        ImGui::TextColored(
            ImVec4(1.0f, 0.5f, 0.2f, 1.0f),
            "Text was truncated to fit the frame");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "SYNCHRONIZATION");
    if (data.peak_position != SIZE_MAX) {
        ImGui::Text("PSS Peak Pos: %zu", data.peak_position);
        if (data.peak_position < data.correlation_map.size()) {
            ImGui::Text("Peak Value: %.3f", data.correlation_map[data.peak_position]);
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "PSS peak not found");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "VIEW");
    ImGui::Checkbox("Show Grid", &show_grid);
    ImGui::SliderInt("Plot Height", &plot_height, 100, 500);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("RightPanel", ImVec2(right_w, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "SIGNAL ANALYSIS");
    ImGui::Separator();

    ImGui::Text("1. Bit Stream");
    if (ImPlot::BeginPlot("##Bits", ImVec2(-1, plot_height))) {
        if (show_grid) {
            ImPlot::SetupAxisLimits(ImAxis_Y1, -0.2, 1.2, ImGuiCond_Always);
        }
        if (!prepared.bits_x.empty()) {
            ImPlot::PlotStairs("Bits", prepared.bits_x.data(), prepared.bits_y.data(), prepared.bits_x.size());
        }
        ImPlot::EndPlot();
    }
    ImGui::Spacing();

    ImGui::Text("2. %s Constellation", data.modulation_name.c_str());
    if (ImPlot::BeginPlot("##Constellation", ImVec2(-1, plot_height))) {
        double max_abs = 1.5;
        for (const CF& symbol : data.ideal_constellation) {
            max_abs = std::max(
                max_abs,
                static_cast<double>(std::max(std::abs(symbol.real()), std::abs(symbol.imag()))) * 1.4
            );
        }
        ImPlot::SetupAxisLimits(ImAxis_X1, -max_abs, max_abs);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -max_abs, max_abs);
        if (!prepared.ideal_i.empty()) {
            ImPlot::PlotScatter(
                "Ideal Grid",
                prepared.ideal_i.data(),
                prepared.ideal_q.data(),
                prepared.ideal_i.size()
            );
        }
        if (!prepared.symbol_i.empty()) {
            ImPlot::PlotScatter("Symbols", prepared.symbol_i.data(), prepared.symbol_q.data(), prepared.symbol_i.size());
        }
        ImPlot::EndPlot();
    }
    ImGui::Spacing();

    ImGui::Text("3. PSS Signal");
    if (ImPlot::BeginPlot("##PSS", ImVec2(-1, plot_height))) {
        std::vector<double> x_pss(prepared.pss_real.size());
        std::iota(x_pss.begin(), x_pss.end(), 0.0);
        if (!x_pss.empty()) {
            ImPlot::PlotLine("Real", x_pss.data(), prepared.pss_real.data(), x_pss.size());
            ImPlot::PlotLine("Imag", x_pss.data(), prepared.pss_imag.data(), x_pss.size());
        }
        ImPlot::EndPlot();
    }
    ImGui::Spacing();

    ImGui::Text("4. OFDM Symbol (No CP)");
    if (ImPlot::BeginPlot("##OFDM_Time", ImVec2(-1, plot_height))) {
        std::vector<double> x_ofdm(prepared.ofdm_time_real.size());
        std::iota(x_ofdm.begin(), x_ofdm.end(), 0.0);
        if (!x_ofdm.empty()) {
            ImPlot::PlotLine("Real", x_ofdm.data(), prepared.ofdm_time_real.data(), x_ofdm.size());
            ImPlot::PlotLine("Imag", x_ofdm.data(), prepared.ofdm_time_imag.data(), x_ofdm.size());
        }
        ImPlot::EndPlot();
    }
    ImGui::Spacing();

    ImGui::Text("5. OFDM with Cyclic Prefix");
    if (ImPlot::BeginPlot("##OFDM_CP", ImVec2(-1, plot_height))) {
        std::vector<double> x_cp(prepared.ofdm_cp_real.size());
        std::iota(x_cp.begin(), x_cp.end(), 0.0);
        if (!x_cp.empty()) {
            ImPlot::PlotLine("Real", x_cp.data(), prepared.ofdm_cp_real.data(), x_cp.size());
            const size_t cp_len = data.ofdm_with_cp.size() - data.ofdm_symbols.size();
            if (cp_len > 0 && cp_len < x_cp.size()) {
                double v_line_x = static_cast<double>(cp_len);
                ImPlot::PlotInfLines("CP End", &v_line_x, 1);
            }
        }
        ImPlot::EndPlot();
    }
    ImGui::Spacing();

    ImGui::Text("6. Full Tx Frame");
    if (ImPlot::BeginPlot("##Tx_Frame", ImVec2(-1, plot_height * 1.5f))) {
        std::vector<double> x_tx(data.tx_array.size());
        std::iota(x_tx.begin(), x_tx.end(), 0.0);
        if (!x_tx.empty()) {
            ImPlot::PlotLine("Real", x_tx.data(), prepared.tx_real.data(), x_tx.size());
        }
        ImPlot::EndPlot();
    }
    ImGui::Spacing();

    ImGui::Text("7. PSS Correlation Peak");
    if (ImPlot::BeginPlot("##Corr", ImVec2(-1, plot_height))) {
        if (!data.correlation_map.empty()) {
            std::vector<double> x_corr(data.correlation_map.size());
            std::iota(x_corr.begin(), x_corr.end(), 0.0);
            ImPlot::PlotLine("Corr", x_corr.data(), data.correlation_map.data(), x_corr.size());
            if (data.peak_position != SIZE_MAX) {
                const double peak_x = static_cast<double>(data.peak_position);
                ImPlot::PlotInfLines("Peak", &peak_x, 1);
            }
        }
        ImPlot::EndPlot();
    }
    ImGui::Spacing();

    ImGui::Text("8. Data Stream (After PSS)");
    if (!data.data_after_pss.empty()) {
        if (ImPlot::BeginPlot("##DataTime", ImVec2(-1, plot_height))) {
            std::vector<double> x_data(data.data_after_pss.size());
            std::vector<double> y_real(data.data_after_pss.size());
            for (size_t i = 0; i < data.data_after_pss.size(); ++i) {
                x_data[i] = static_cast<double>(i);
                y_real[i] = data.data_after_pss[i].real();
            }
            ImPlot::PlotLine("Real", x_data.data(), y_real.data(), x_data.size());
            ImPlot::EndPlot();
        }
        ImGui::Text(
            "Samples: %zu | Approx symbols: %zu",
            data.data_after_pss.size(),
            data.data_after_pss.size() / (LTE + CP_LENGTH)
        );
    } else {
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "No data extracted after PSS.");
    }
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "9. Received Constellation");
    if (!data.received_constellation.empty()) {
        ImGui::Text(
            "Blue: received points from current frame | Orange: full ideal %s grid",
            data.modulation_name.c_str()
        );
        if (ImPlot::BeginPlot("##RxConstellation", ImVec2(-1, 250))) {
            double max_abs = 1.5;
            for (const CF& symbol : data.ideal_constellation) {
                max_abs = std::max(
                    max_abs,
                    static_cast<double>(std::max(std::abs(symbol.real()), std::abs(symbol.imag()))) * 1.4
                );
            }
            ImPlot::SetupAxisLimits(ImAxis_X1, -max_abs, max_abs);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -max_abs, max_abs);

            std::vector<double> rx_i;
            std::vector<double> rx_q;
            rx_i.reserve(data.received_constellation.size());
            rx_q.reserve(data.received_constellation.size());
            for (const CF& symbol : data.received_constellation) {
                rx_i.push_back(symbol.real());
                rx_q.push_back(symbol.imag());
            }

            if (!prepared.ideal_i.empty()) {
                ImPlot::PlotScatter(
                    data.modulation_name.c_str(),
                    prepared.ideal_i.data(),
                    prepared.ideal_q.data(),
                    prepared.ideal_i.size()
                );
            }
            ImPlot::PlotScatter("Rx Points", rx_i.data(), rx_q.data(), rx_i.size());
            ImPlot::EndPlot();
        }
    } else {
        ImGui::Text("No constellation data available.");
    }

    ImGui::EndChild();
    ImGui::End();
}

}  // namespace

void run_gui(const GuiPlotData& data)
{
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    if (!init_gui(window, gl_context)) {
        return;
    }

    const PreparedGuiData prepared = prepare_gui_data(data);
    bool show_grid = true;
    int plot_height = 250;
    bool running = true;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        render_monitor(data, prepared, show_grid, plot_height, {});

        ImGui::Render();
        ImGuiIO& io = ImGui::GetIO();
        glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    shutdown_gui(window, gl_context);
}

int run_simulation_gui(
    const std::string& initial_text,
    double initial_snr,
    const std::string& initial_modulation_name)
{
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    if (!init_gui(window, gl_context)) {
        return 1;
    }

    const auto& specs = modulation_specs();
    int modulation_index = 0;
    for (size_t i = 0; i < specs.size(); ++i) {
        if (specs[i].name == initial_modulation_name) {
            modulation_index = static_cast<int>(i);
            break;
        }
    }

    std::array<char, 256> text_buffer{};
    std::snprintf(text_buffer.data(), text_buffer.size(), "%s", initial_text.c_str());
    float snr_value = static_cast<float>(initial_snr);

    GuiPlotData current = build_simulation_view(text_buffer.data(), snr_value, specs[modulation_index]);
    PreparedGuiData prepared = prepare_gui_data(current);

    bool show_grid = true;
    int plot_height = 250;
    bool running = true;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        bool rebuild_requested = false;
        render_monitor(
            current,
            prepared,
            show_grid,
            plot_height,
            [&]() {
                ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "SIMULATION CONTROL");

                ImGui::InputText("Text", text_buffer.data(), text_buffer.size());
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    rebuild_requested = true;
                }

                if (ImGui::BeginCombo("Modulation", specs[modulation_index].name.c_str())) {
                    for (size_t i = 0; i < specs.size(); ++i) {
                        const bool selected = (static_cast<int>(i) == modulation_index);
                        if (ImGui::Selectable(specs[i].name.c_str(), selected)) {
                            modulation_index = static_cast<int>(i);
                            rebuild_requested = true;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                if (ImGui::SliderFloat("SNR, dB", &snr_value, 0.0f, 40.0f, "%.1f")) {
                    rebuild_requested = true;
                }

                if (ImGui::Button("Apply")) {
                    rebuild_requested = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset")) {
                    std::snprintf(text_buffer.data(), text_buffer.size(), "%s", initial_text.c_str());
                    snr_value = static_cast<float>(initial_snr);
                    modulation_index = 0;
                    for (size_t i = 0; i < specs.size(); ++i) {
                        if (specs[i].name == initial_modulation_name) {
                            modulation_index = static_cast<int>(i);
                            break;
                        }
                    }
                    rebuild_requested = true;
                }
            });

        ImGui::Render();
        ImGuiIO& io = ImGui::GetIO();
        glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);

        if (rebuild_requested) {
            current = build_simulation_view(text_buffer.data(), snr_value, specs[modulation_index]);
            prepared = prepare_gui_data(current);
        }
    }

    shutdown_gui(window, gl_context);
    return 0;
}

namespace {

std::mutex sdr_init_mutex;
constexpr size_t kLiveRxWindowSamples = 8192;
constexpr long kRealtimeReadTimeoutUs = 20000;
constexpr auto kRealtimeDecodeInterval = std::chrono::milliseconds(100);

bool probe_tx_device(const char* device_uri, std::string& error_message)
{
    std::lock_guard<std::mutex> init_lock(sdr_init_mutex);
    SoapySDRDevice* sdr = open_pluto(device_uri, &error_message);
    if (sdr == nullptr) {
        if (error_message.empty()) {
            error_message = "failed to open TX device";
        }
        return false;
    }

    TxStreamContext tx_ctx;
    if (!open_tx_stream(sdr, tx_ctx, false)) {
        if (error_message.empty()) {
            error_message = SoapySDRDevice_lastError();
        }
        if (error_message.empty()) {
            error_message = "failed to open TX stream";
        }
        SoapySDRDevice_unmake(sdr);
        return false;
    }

    close_tx_stream(tx_ctx);
    SoapySDRDevice_unmake(sdr);
    return true;
}

bool probe_rx_device(const char* device_uri, std::string& error_message)
{
    std::lock_guard<std::mutex> init_lock(sdr_init_mutex);
    SoapySDRDevice* sdr = open_pluto(device_uri, &error_message);
    if (sdr == nullptr) {
        if (error_message.empty()) {
            error_message = "failed to open RX device";
        }
        return false;
    }

    RxStreamContext rx_ctx;
    if (!open_rx_stream(sdr, rx_ctx, false)) {
        if (error_message.empty()) {
            error_message = SoapySDRDevice_lastError();
        }
        if (error_message.empty()) {
            error_message = "failed to open RX stream";
        }
        SoapySDRDevice_unmake(sdr);
        return false;
    }

    close_rx_stream(rx_ctx);
    SoapySDRDevice_unmake(sdr);
    return true;
}

struct RealtimeSharedState {
    std::mutex mutex;
    bool quit = false;
    bool rx_enabled = true;
    bool tx_ready = false;
    bool rx_ready = false;
    std::string tx_status = "Opening TX device...";
    std::string rx_status = "Opening RX device...";
    std::string active_text;
    const ModulationSpec* active_modulation = nullptr;
    ChannelEstimatorMethod active_estimator = ChannelEstimatorMethod::DFTLS;
    TxFrameData tx_frame;
    std::vector<CF> ideal_constellation;
    std::vector<CF> rx_live_samples;
    uint64_t rx_total_samples = 0;
    uint64_t rx_live_generation = 0;
    LiveMonitorInfo live_monitor;
    std::shared_ptr<const TxViewData> tx_view;
    std::shared_ptr<const LiveViewData> live_view;
    CaptureInfo latest_capture;
    bool has_latest_capture = false;
    std::shared_ptr<const FrameViewData> latest_frame_view;
    CaptureInfo last_valid_capture;
    bool has_last_valid_capture = false;
    std::shared_ptr<const FrameViewData> last_valid_frame_view;
    size_t good_frames = 0;
};

struct RealtimeRenderSnapshot {
    std::string tx_status;
    std::string rx_status;
    std::string active_text;
    std::string modulation_name;
    std::string estimator_name;
    bool rx_enabled = true;
    bool tx_ready = false;
    bool rx_ready = false;
    std::shared_ptr<const TxViewData> tx_view;
    std::shared_ptr<const LiveViewData> live_view;
    bool has_latest_capture = false;
    bool latest_frame_found = false;
    bool latest_payload_ready = false;
    bool latest_crc_ok = false;
    size_t latest_pss_peak = SIZE_MAX;
    double latest_pss_corr = 0.0;
    double latest_cfo = 0.0;
    int latest_spectrum_mode = 0;
    int latest_subcarrier_shift = 0;
    int latest_data_order = 0;
    std::shared_ptr<const FrameViewData> latest_frame_view;
    bool has_last_valid_capture = false;
    std::shared_ptr<const FrameViewData> last_valid_frame_view;
    bool live_monitor_available = false;
    size_t live_symbol_start = SIZE_MAX;
    double live_cp_score = 0.0;
    int live_spectrum_mode = 0;
    int live_subcarrier_shift = 0;
    size_t good_frames = 0;
};

RealtimeRenderSnapshot make_realtime_snapshot(RealtimeSharedState& shared)
{
    std::lock_guard<std::mutex> lock(shared.mutex);

    RealtimeRenderSnapshot snapshot;
    snapshot.tx_status = shared.tx_status;
    snapshot.rx_status = shared.rx_status;
    snapshot.active_text = shared.active_text;
    snapshot.modulation_name = shared.active_modulation ? shared.active_modulation->name : "N/A";
    snapshot.estimator_name = channel_estimator_name(shared.active_estimator);
    snapshot.rx_enabled = shared.rx_enabled;
    snapshot.tx_ready = shared.tx_ready;
    snapshot.rx_ready = shared.rx_ready;
    snapshot.tx_view = shared.tx_view;
    snapshot.live_view = shared.live_view;
    snapshot.has_latest_capture = shared.has_latest_capture;
    snapshot.latest_frame_found = shared.latest_capture.frame_found;
    snapshot.latest_payload_ready = shared.latest_capture.payload_ready;
    snapshot.latest_crc_ok = shared.latest_capture.decoded.crc_ok;
    snapshot.latest_pss_peak = shared.latest_capture.sync.pss_peak;
    snapshot.latest_pss_corr = shared.latest_capture.sync.pss_corr;
    snapshot.latest_cfo = shared.latest_capture.cfo;
    snapshot.latest_spectrum_mode = shared.latest_capture.spectrum_mode;
    snapshot.latest_subcarrier_shift = shared.latest_capture.subcarrier_shift;
    snapshot.latest_data_order = shared.latest_capture.data_order;
    snapshot.latest_frame_view = shared.latest_frame_view;
    snapshot.has_last_valid_capture = shared.has_last_valid_capture;
    snapshot.last_valid_frame_view = shared.last_valid_frame_view;
    snapshot.live_monitor_available = shared.live_monitor.available;
    snapshot.live_symbol_start = shared.live_monitor.symbol_start;
    snapshot.live_cp_score = shared.live_monitor.cp_score;
    snapshot.live_spectrum_mode = shared.live_monitor.spectrum_mode;
    snapshot.live_subcarrier_shift = shared.live_monitor.subcarrier_shift;
    snapshot.good_frames = shared.good_frames;
    return snapshot;
}

void finalize_line_plot(LinePlotData& plot)
{
    if (plot.x.empty() || plot.y0.empty()) {
        return;
    }

    double min_y = std::numeric_limits<double>::infinity();
    double max_y = -std::numeric_limits<double>::infinity();
    for (double value : plot.y0) {
        min_y = std::min(min_y, value);
        max_y = std::max(max_y, value);
    }
    if (plot.show_secondary) {
        for (double value : plot.y1) {
            min_y = std::min(min_y, value);
            max_y = std::max(max_y, value);
        }
    }

    const double span = std::max(0.01, max_y - min_y);
    const double margin = span * 0.15;
    plot.x_min = plot.x.front();
    plot.x_max = plot.x.back();
    plot.y_min = min_y - margin;
    plot.y_max = max_y + margin;
    plot.available = true;
}

LinePlotData build_time_plot_data(
    const std::vector<CF>& samples,
    size_t max_points,
    bool show_imag,
    double x_offset = 0.0)
{
    LinePlotData plot;
    plot.show_secondary = show_imag;
    if (samples.empty()) {
        return plot;
    }

    const size_t point_count = (samples.size() <= max_points || max_points < 2)
        ? samples.size()
        : max_points;
    const double source_step =
        (point_count > 1 && samples.size() > point_count)
        ? static_cast<double>(samples.size() - 1) / static_cast<double>(point_count - 1)
        : 1.0;

    plot.x.reserve(point_count);
    plot.y0.reserve(point_count);
    if (show_imag) {
        plot.y1.reserve(point_count);
    }

    for (size_t i = 0; i < point_count; ++i) {
        const size_t idx = (samples.size() == point_count)
            ? i
            : std::min(
                static_cast<size_t>(std::llround(source_step * static_cast<double>(i))),
                samples.size() - 1
            );
        plot.x.push_back(x_offset + static_cast<double>(idx));
        plot.y0.push_back(samples[idx].real());
        if (show_imag) {
            plot.y1.push_back(samples[idx].imag());
        }
    }

    finalize_line_plot(plot);
    return plot;
}

LinePlotData build_live_time_plot_data(const std::vector<CF>& samples, uint64_t total_samples)
{
    const uint64_t first_sample =
        (total_samples >= samples.size()) ? (total_samples - samples.size()) : 0;
    return build_time_plot_data(samples, 2048, true, static_cast<double>(first_sample));
}

ScatterPlotData build_constellation_plot_data(
    const std::vector<CF>& points,
    const std::vector<CF>& ideal_points)
{
    ScatterPlotData plot;
    plot.x.reserve(points.size());
    plot.y.reserve(points.size());
    plot.ideal_x.reserve(ideal_points.size());
    plot.ideal_y.reserve(ideal_points.size());

    for (const CF& point : ideal_points) {
        plot.ideal_x.push_back(point.real());
        plot.ideal_y.push_back(point.imag());
        plot.axis_limit = std::max(
            plot.axis_limit,
            static_cast<double>(std::max(std::abs(point.real()), std::abs(point.imag()))) * 1.4
        );
    }

    for (const CF& point : points) {
        plot.x.push_back(point.real());
        plot.y.push_back(point.imag());
        plot.axis_limit = std::max(
            plot.axis_limit,
            static_cast<double>(std::max(std::abs(point.real()), std::abs(point.imag()))) * 1.4
        );
    }

    plot.available = !plot.x.empty() || !plot.ideal_x.empty();
    return plot;
}

ChannelPlotData build_channel_plot_data(const std::vector<CF>& channel)
{
    ChannelPlotData plot;
    plot.x.reserve(channel.size());
    plot.magnitude.reserve(channel.size());
    plot.phase.reserve(channel.size());

    for (size_t idx = 28; idx <= 100 && idx < channel.size(); ++idx) {
        if (idx == 64 || std::abs(channel[idx]) < 1e-6f) {
            continue;
        }

        plot.x.push_back(static_cast<double>(idx));
        plot.magnitude.push_back(std::abs(channel[idx]));
        plot.phase.push_back(std::arg(channel[idx]));
    }

    plot.available = !plot.x.empty();
    return plot;
}

std::vector<CF> extract_display_constellation(
    const std::vector<CF>& equalized_symbol,
    const std::vector<CF>& fallback_points)
{
    if (!equalized_symbol.empty()) {
        std::vector<CF> points;
        const std::vector<size_t>& data_indices = payload_data_indices();
        points.reserve(data_indices.size());
        for (size_t idx : data_indices) {
            if (idx < equalized_symbol.size()) {
                points.push_back(equalized_symbol[idx]);
            }
        }
        if (!points.empty()) {
            return points;
        }
    }

    return fallback_points;
}

FrameViewData build_frame_view_data(
    const CaptureInfo& capture,
    const std::vector<CF>& ideal_constellation)
{
    FrameViewData view;
    view.frame_time = build_time_plot_data(capture.data_after_pss, 2048, false);
    const std::vector<CF> display_points = extract_display_constellation(
        capture.decoded.equalized_symbol,
        capture.decoded.constellation_points
    );
    view.constellation = build_constellation_plot_data(
        display_points,
        ideal_constellation
    );
    view.channel = build_channel_plot_data(capture.decoded.channel_estimate);
    view.recovered_text = capture.decoded.recovered_text;
    view.crc_ok = capture.decoded.crc_ok;
    view.spectrum_mode = capture.spectrum_mode;
    view.subcarrier_shift = capture.subcarrier_shift;
    view.data_order = capture.data_order;
    view.cfo = capture.cfo;
    view.available = capture.frame_found &&
        capture.payload_ready &&
        !display_points.empty();
    return view;
}

LiveViewData build_live_view_data(
    const std::vector<CF>& rx_samples,
    uint64_t total_samples,
    const LiveMonitorInfo& live_monitor,
    const std::vector<CF>& ideal_constellation)
{
    LiveViewData view;
    view.rx_time = build_live_time_plot_data(rx_samples, total_samples);
    const std::vector<CF> display_points = extract_display_constellation(
        live_monitor.equalized_symbol,
        live_monitor.constellation_points
    );
    view.constellation = build_constellation_plot_data(
        display_points,
        ideal_constellation
    );
    view.channel = build_channel_plot_data(live_monitor.channel_estimate);
    view.symbol_start = live_monitor.symbol_start;
    view.cp_score = live_monitor.cp_score;
    view.spectrum_mode = live_monitor.spectrum_mode;
    view.subcarrier_shift = live_monitor.subcarrier_shift;
    view.available = live_monitor.available;
    return view;
}

TxViewData build_tx_view_data(
    const TxFrameData& frame,
    const std::vector<CF>& ideal_constellation)
{
    TxViewData view;
    view.constellation = build_constellation_plot_data(frame.modulated_symbols, ideal_constellation);
    view.frame_time = build_time_plot_data(frame.samples, 2048, false);
    view.training_time = build_time_plot_data(frame.training_ofdm, 2048, true);
    view.frame_samples = frame.samples.size();
    return view;
}

void append_recent_samples(
    std::vector<CF>& history,
    const std::vector<CF>& chunk,
    size_t max_samples)
{
    if (chunk.empty()) {
        return;
    }

    if (chunk.size() >= max_samples) {
        history.assign(
            chunk.end() - static_cast<ptrdiff_t>(max_samples),
            chunk.end()
        );
        return;
    }

    history.insert(history.end(), chunk.begin(), chunk.end());
    if (history.size() > max_samples) {
        history.erase(
            history.begin(),
            history.begin() + static_cast<ptrdiff_t>(history.size() - max_samples)
        );
    }
}

void draw_constellation_plot(
    const char* plot_id,
    const char* point_label,
    const ScatterPlotData& plot,
    float height)
{
    if (!plot.available || !ImPlot::BeginPlot(plot_id, ImVec2(-1, height))) {
        return;
    }

    ImPlot::SetupAxisLimits(ImAxis_X1, -plot.axis_limit, plot.axis_limit, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, -plot.axis_limit, plot.axis_limit, ImGuiCond_Always);

    if (!plot.ideal_x.empty()) {
        ImPlot::PlotScatter("Ideal", plot.ideal_x.data(), plot.ideal_y.data(), plot.ideal_x.size());
    }

    if (!plot.x.empty()) {
        ImPlot::PlotScatter(point_label, plot.x.data(), plot.y.data(), plot.x.size());
    }

    ImPlot::EndPlot();
}

void draw_time_plot(
    const char* plot_id,
    const LinePlotData& plot,
    float height)
{
    if (!plot.available || !ImPlot::BeginPlot(plot_id, ImVec2(-1, height))) {
        return;
    }

    ImPlot::SetupAxisLimits(ImAxis_X1, plot.x_min, plot.x_max, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, plot.y_min, plot.y_max, ImGuiCond_Always);
    ImPlot::PlotLine("Real", plot.x.data(), plot.y0.data(), plot.x.size());
    if (plot.show_secondary && !plot.y1.empty()) {
        ImPlot::PlotLine("Imag", plot.x.data(), plot.y1.data(), plot.x.size());
    }

    ImPlot::EndPlot();
}

void draw_channel_plot(
    const char* plot_id,
    const ChannelPlotData& plot,
    bool draw_phase,
    float height)
{
    if (!plot.available || !ImPlot::BeginPlot(plot_id, ImVec2(-1, height))) {
        return;
    }

    if (draw_phase) {
        ImPlot::PlotLine("Phase", plot.x.data(), plot.phase.data(), plot.x.size());
    } else {
        ImPlot::PlotLine("Magnitude", plot.x.data(), plot.magnitude.data(), plot.x.size());
    }
    ImPlot::EndPlot();
}

void tx_worker(const char* tx_device_uri, RealtimeSharedState& shared)
{
    std::string open_error;
    TxStreamContext tx_ctx;
    SoapySDRDevice* sdr = nullptr;
    {
        std::lock_guard<std::mutex> init_lock(sdr_init_mutex);
        sdr = open_pluto(tx_device_uri, &open_error);
        if (!sdr) {
            std::lock_guard<std::mutex> lock(shared.mutex);
            shared.tx_status = open_error.empty()
                ? "Failed to open TX device"
                : ("Failed to open TX device: " + open_error);
            shared.tx_ready = false;
            return;
        }

        if (!open_tx_stream(sdr, tx_ctx, true)) {
            SoapySDRDevice_unmake(sdr);
            std::lock_guard<std::mutex> lock(shared.mutex);
            shared.tx_status = "Failed to open TX stream";
            shared.tx_ready = false;
            return;
        }
    }

    {
        std::lock_guard<std::mutex> lock(shared.mutex);
        shared.tx_ready = true;
        shared.tx_status = "TX streaming";
    }

    while (true) {
        std::vector<CF> samples;
        {
            std::lock_guard<std::mutex> lock(shared.mutex);
            if (shared.quit) {
                break;
            }
            samples = shared.tx_frame.samples;
        }

        if (samples.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        if (!write_tx_frame(tx_ctx, samples, 1, false)) {
            std::lock_guard<std::mutex> lock(shared.mutex);
            shared.tx_status = "TX write error";
            shared.tx_ready = false;
            break;
        }
    }

    close_tx_stream(tx_ctx);
    SoapySDRDevice_unmake(sdr);
}

void rx_stream_worker(const char* rx_device_uri, RealtimeSharedState& shared)
{
    std::string open_error;
    RxStreamContext rx_ctx;
    SoapySDRDevice* sdr = nullptr;
    {
        std::lock_guard<std::mutex> init_lock(sdr_init_mutex);
        sdr = open_pluto(rx_device_uri, &open_error);
        if (!sdr) {
            std::lock_guard<std::mutex> lock(shared.mutex);
            shared.rx_status = open_error.empty()
                ? "Failed to open RX device"
                : ("Failed to open RX device: " + open_error);
            shared.rx_ready = false;
            return;
        }

        if (!open_rx_stream(sdr, rx_ctx, true)) {
            SoapySDRDevice_unmake(sdr);
            std::lock_guard<std::mutex> lock(shared.mutex);
            shared.rx_status = "Failed to open RX stream";
            shared.rx_ready = false;
            return;
        }
    }

    {
        std::lock_guard<std::mutex> lock(shared.mutex);
        shared.rx_ready = true;
        shared.rx_status = "RX listening";
    }

    bool warmup_required = true;
    std::vector<CF> rx_history;
    rx_history.reserve(kLiveRxWindowSamples);
    uint64_t total_samples = 0;
    while (true) {
        bool rx_enabled = true;
        const ModulationSpec* modulation = nullptr;
        ChannelEstimatorMethod estimator = ChannelEstimatorMethod::PilotLSLinear;
        std::vector<CF> ideal_constellation;
        {
            std::lock_guard<std::mutex> lock(shared.mutex);
            if (shared.quit) {
                break;
            }
            rx_enabled = shared.rx_enabled;
            modulation = shared.active_modulation;
            estimator = shared.active_estimator;
            ideal_constellation = shared.ideal_constellation;
        }

        if (!rx_enabled || modulation == nullptr) {
            warmup_required = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        const std::vector<CF> rx_samples =
            read_rx_samples(rx_ctx, 1, warmup_required ? 2 : 0, false, kRealtimeReadTimeoutUs);
        if (rx_samples.empty()) {
            std::lock_guard<std::mutex> lock(shared.mutex);
            shared.rx_status = "RX waiting for samples";
            warmup_required = true;
            continue;
        }
        warmup_required = false;
        append_recent_samples(rx_history, rx_samples, kLiveRxWindowSamples);
        total_samples += static_cast<uint64_t>(rx_samples.size());
        const LiveMonitorInfo live_monitor = analyze_live_monitor(rx_history, *modulation, estimator);
        const auto live_view = std::make_shared<LiveViewData>(
            build_live_view_data(rx_history, total_samples, live_monitor, ideal_constellation)
        );

        {
            std::lock_guard<std::mutex> lock(shared.mutex);
            shared.rx_live_samples = rx_history;
            shared.rx_total_samples = total_samples;
            ++shared.rx_live_generation;
            shared.live_monitor = live_monitor;
            shared.live_view = live_view;
            shared.rx_status = "RX listening (" + std::to_string(rx_history.size()) + " samples)";
        }
    }

    close_rx_stream(rx_ctx);
    SoapySDRDevice_unmake(sdr);
}

void rx_decode_worker(RealtimeSharedState& shared)
{
    uint64_t last_generation = 0;
    auto last_decode_time = std::chrono::steady_clock::now() - kRealtimeDecodeInterval;
    while (true) {
        bool rx_enabled = true;
        const ModulationSpec* modulation = nullptr;
        ChannelEstimatorMethod estimator = ChannelEstimatorMethod::DFTLS;
        std::vector<CF> rx_snapshot;
        std::vector<CF> ideal_constellation;
        LiveMonitorInfo live_monitor;
        uint64_t generation = 0;

        {
            std::lock_guard<std::mutex> lock(shared.mutex);
            if (shared.quit) {
                break;
            }
            rx_enabled = shared.rx_enabled;
            modulation = shared.active_modulation;
            estimator = shared.active_estimator;
            rx_snapshot = shared.rx_live_samples;
            ideal_constellation = shared.ideal_constellation;
            live_monitor = shared.live_monitor;
            generation = shared.rx_live_generation;
        }

        if (!rx_enabled || modulation == nullptr || rx_snapshot.size() < SYMBOL_LEN * 3) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (generation == 0 || generation == last_generation) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        last_generation = generation;

        const auto now = std::chrono::steady_clock::now();
        if (now - last_decode_time < kRealtimeDecodeInterval) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        last_decode_time = now;

        DecodeSearchHints search_hints;
        const DecodeSearchHints* search_hints_ptr = nullptr;
        if (live_monitor.available) {
            search_hints.use_spectrum_mode = true;
            search_hints.spectrum_mode = live_monitor.spectrum_mode;
            search_hints.use_subcarrier_shift = true;
            search_hints.subcarrier_shift = live_monitor.subcarrier_shift;
            search_hints_ptr = &search_hints;
        }

        CaptureInfo capture = decode_capture(rx_snapshot, *modulation, estimator, search_hints_ptr);
        const auto frame_view = std::make_shared<FrameViewData>(
            build_frame_view_data(capture, ideal_constellation)
        );
        {
            std::lock_guard<std::mutex> lock(shared.mutex);
            shared.latest_capture = capture;
            shared.has_latest_capture = true;
            shared.latest_frame_view = frame_view;
            if (capture.decoded.crc_ok) {
                shared.last_valid_capture = capture;
                shared.has_last_valid_capture = true;
                shared.last_valid_frame_view = frame_view;
                ++shared.good_frames;
            }
        }
    }
}

}  // namespace

int run_realtime_tx_gui(
    const char* device_uri,
    const std::string& initial_text,
    const std::string& initial_modulation_name)
{
    const ModulationSpec* initial_modulation = find_modulation_spec(initial_modulation_name);
    if (initial_modulation == nullptr) {
        std::cerr << "Error: Unknown modulation '" << initial_modulation_name << "'\n";
        return 1;
    }

    std::string error_message;
    if (!probe_tx_device(device_uri, error_message)) {
        std::cerr << "Error: TX device '" << device_uri << "' is unavailable: " << error_message << "\n";
        return 1;
    }

    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    if (!init_gui(window, gl_context)) {
        return 1;
    }

    SDL_SetWindowTitle(window, "OFDM TX Control");
    SDL_SetWindowSize(window, 720, 320);

    RealtimeSharedState shared;
    shared.active_text = initial_text.substr(0, MAX_TEXT_BYTES);
    shared.active_modulation = initial_modulation;
    shared.tx_frame = build_tx_frame(shared.active_text, *initial_modulation);
    shared.ideal_constellation = ideal_constellation_points(*initial_modulation);
    shared.tx_view = std::make_shared<TxViewData>(
        build_tx_view_data(shared.tx_frame, shared.ideal_constellation)
    );

    std::thread tx_thread(tx_worker, device_uri, std::ref(shared));

    const auto& specs = modulation_specs();
    int modulation_index = 0;
    for (size_t i = 0; i < specs.size(); ++i) {
        if (specs[i].name == initial_modulation_name) {
            modulation_index = static_cast<int>(i);
            break;
        }
    }

    std::array<char, 256> text_buffer{};
    std::snprintf(text_buffer.data(), text_buffer.size(), "%s", initial_text.c_str());

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        const RealtimeRenderSnapshot snapshot = make_realtime_snapshot(shared);

        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(680.0f, 260.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin(
            "OFDM TX",
            nullptr,
            ImGuiWindowFlags_NoCollapse
        );

        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "TX CONTROL");
        ImGui::Separator();
        ImGui::Text("Device: %s", device_uri);
        ImGui::SameLine();
        ImGui::Text("Status: %s", snapshot.tx_status.c_str());

        ImGui::Spacing();
        ImGui::Text("Message");
        ImGui::PushItemWidth(-1.0f);
        ImGui::InputText("##TxMessage", text_buffer.data(), text_buffer.size());
        ImGui::PopItemWidth();

        ImGui::Text("Modulation");
        ImGui::PushItemWidth(220.0f);
        if (ImGui::BeginCombo("##TxModulation", specs[modulation_index].name.c_str())) {
            for (size_t i = 0; i < specs.size(); ++i) {
                const bool selected = static_cast<int>(i) == modulation_index;
                if (ImGui::Selectable(specs[i].name.c_str(), selected)) {
                    modulation_index = static_cast<int>(i);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        if (ImGui::Button("Apply", ImVec2(160.0f, 0.0f))) {
            const std::string next_text = std::string(text_buffer.data()).substr(0, MAX_TEXT_BYTES);
            const ModulationSpec* next_modulation = &specs[modulation_index];
            const TxFrameData next_frame = build_tx_frame(next_text, *next_modulation);
            const std::vector<CF> next_ideal = ideal_constellation_points(*next_modulation);
            const auto next_tx_view = std::make_shared<TxViewData>(
                build_tx_view_data(next_frame, next_ideal)
            );

            std::lock_guard<std::mutex> lock(shared.mutex);
            shared.active_text = next_text;
            shared.active_modulation = next_modulation;
            shared.tx_frame = next_frame;
            shared.ideal_constellation = next_ideal;
            shared.tx_view = next_tx_view;
        }

        ImGui::Separator();
        ImGui::TextWrapped("Active text: %s", snapshot.active_text.c_str());
        ImGui::Text("Modulation: %s", snapshot.modulation_name.c_str());
        ImGui::Text(
            "Frame samples: %zu",
            snapshot.tx_view ? snapshot.tx_view->frame_samples : static_cast<size_t>(0)
        );

        ImGui::End();

        ImGui::Render();
        ImGuiIO& io = ImGui::GetIO();
        glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    {
        std::lock_guard<std::mutex> lock(shared.mutex);
        shared.quit = true;
    }

    tx_thread.join();
    shutdown_gui(window, gl_context);
    return 0;
}

int run_realtime_rx_gui(
    const char* device_uri,
    const std::string& initial_modulation_name)
{
    const ModulationSpec* initial_modulation = find_modulation_spec(initial_modulation_name);
    if (initial_modulation == nullptr) {
        std::cerr << "Error: Unknown modulation '" << initial_modulation_name << "'\n";
        return 1;
    }

    std::string error_message;
    if (!probe_rx_device(device_uri, error_message)) {
        std::cerr << "Error: RX device '" << device_uri << "' is unavailable: " << error_message << "\n";
        return 1;
    }

    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    if (!init_gui(window, gl_context)) {
        return 1;
    }

    SDL_SetWindowTitle(window, "OFDM RX Monitor");
    SDL_SetWindowSize(window, 1680, 980);

    RealtimeSharedState shared;
    shared.active_modulation = initial_modulation;
    shared.active_estimator = ChannelEstimatorMethod::DFTLS;
    shared.ideal_constellation = ideal_constellation_points(*initial_modulation);

    std::thread rx_thread(rx_stream_worker, device_uri, std::ref(shared));
    std::thread rx_decode_thread(rx_decode_worker, std::ref(shared));

    const auto& specs = modulation_specs();
    const auto& estimators = channel_estimator_methods();

    int modulation_index = 0;
    for (size_t i = 0; i < specs.size(); ++i) {
        if (specs[i].name == initial_modulation_name) {
            modulation_index = static_cast<int>(i);
            break;
        }
    }

    int estimator_index = 0;
    for (size_t i = 0; i < estimators.size(); ++i) {
        if (estimators[i] == shared.active_estimator) {
            estimator_index = static_cast<int>(i);
            break;
        }
    }

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        const RealtimeRenderSnapshot snapshot = make_realtime_snapshot(shared);

        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(1560.0f, 920.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin(
            "OFDM RX",
            nullptr,
            ImGuiWindowFlags_NoCollapse
        );

        ImGui::TextColored(ImVec4(0.2f, 0.9f, 1.0f, 1.0f), "RX MONITOR");
        ImGui::Separator();
        ImGui::Text("Device: %s", device_uri);
        ImGui::SameLine();
        ImGui::Text("Status: %s", snapshot.rx_status.c_str());
        ImGui::SameLine();
        ImGui::Text("Good frames: %zu", snapshot.good_frames);

        ImGui::BeginGroup();
        ImGui::Text("Modulation");
        ImGui::PushItemWidth(220.0f);
        if (ImGui::BeginCombo("##RxModulation", specs[modulation_index].name.c_str())) {
            for (size_t i = 0; i < specs.size(); ++i) {
                const bool selected = static_cast<int>(i) == modulation_index;
                if (ImGui::Selectable(specs[i].name.c_str(), selected)) {
                    modulation_index = static_cast<int>(i);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::Text("Channel estimation");
        ImGui::PushItemWidth(320.0f);
        if (ImGui::BeginCombo("##RxEstimator", channel_estimator_name(estimators[estimator_index]))) {
            for (size_t i = 0; i < estimators.size(); ++i) {
                const bool selected = static_cast<int>(i) == estimator_index;
                if (ImGui::Selectable(channel_estimator_name(estimators[i]), selected)) {
                    estimator_index = static_cast<int>(i);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::EndGroup();

        ImGui::SameLine();
        if (ImGui::Button("Apply", ImVec2(160.0f, 0.0f))) {
            const ModulationSpec* next_modulation = &specs[modulation_index];
            const ChannelEstimatorMethod next_estimator = estimators[estimator_index];
            const std::vector<CF> next_ideal = ideal_constellation_points(*next_modulation);

            std::lock_guard<std::mutex> lock(shared.mutex);
            shared.active_modulation = next_modulation;
            shared.active_estimator = next_estimator;
            shared.ideal_constellation = next_ideal;
            shared.has_latest_capture = false;
            shared.has_last_valid_capture = false;
            shared.live_monitor = {};
            shared.live_view.reset();
            shared.latest_frame_view.reset();
            shared.last_valid_frame_view.reset();
        }

        ImGui::SameLine();
        if (ImGui::Button(snapshot.rx_enabled ? "Stop RX" : "Start RX", ImVec2(160.0f, 0.0f))) {
            std::lock_guard<std::mutex> lock(shared.mutex);
            shared.rx_enabled = !shared.rx_enabled;
            shared.rx_status = shared.rx_enabled ? "RX listening" : "RX stopped";
            if (!shared.rx_enabled) {
                shared.live_monitor = {};
                shared.live_view.reset();
                shared.has_latest_capture = false;
                shared.latest_frame_view.reset();
            }
        }

        ImGui::Separator();
        ImGui::Text("Continuous RX time-domain signal");
        if (snapshot.live_view) {
            draw_time_plot("##RxGuiRawRx", snapshot.live_view->rx_time, 260.0f);
        }

        if (snapshot.has_latest_capture && snapshot.latest_frame_found) {
            ImGui::Text(
                "Current sync: peak=%zu corr=%.3f cfo=%.6f",
                snapshot.latest_pss_peak,
                snapshot.latest_pss_corr,
                snapshot.latest_cfo
            );
            ImGui::SameLine();
            ImGui::Text(
                "CRC: %s | Mode=%d Shift=%d Order=%d",
                snapshot.latest_crc_ok ? "OK" : "FAIL",
                snapshot.latest_spectrum_mode,
                snapshot.latest_subcarrier_shift,
                snapshot.latest_data_order
            );
        } else if (snapshot.live_monitor_available) {
            ImGui::Text(
                "Live OFDM slice: start=%zu cp=%.3f mode=%d shift=%d",
                snapshot.live_symbol_start,
                snapshot.live_cp_score,
                snapshot.live_spectrum_mode,
                snapshot.live_subcarrier_shift
            );
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No frame detected in current live stream.");
        }

        ImGui::Spacing();
        ImGui::Text("Last valid received frame");
        const FrameViewData* display_frame = nullptr;
        if (snapshot.has_latest_capture &&
            snapshot.latest_frame_view &&
            snapshot.latest_frame_view->available) {
            display_frame = snapshot.latest_frame_view.get();
        } else if (snapshot.has_last_valid_capture && snapshot.last_valid_frame_view) {
            display_frame = snapshot.last_valid_frame_view.get();
        }

        if (display_frame != nullptr && display_frame->available) {
            draw_time_plot("##RxGuiLastFrame", display_frame->frame_time, 220.0f);
            ImGui::TextWrapped("Decoded text: %s", display_frame->recovered_text.c_str());
            ImGui::Text("CRC: %s", display_frame->crc_ok ? "OK" : "FAIL");
            ImGui::Text(
                "Mode=%d Shift=%d Order=%d CFO=%.6f",
                display_frame->spectrum_mode,
                display_frame->subcarrier_shift,
                display_frame->data_order,
                display_frame->cfo
            );

            ImGui::Spacing();
            ImGui::Text("RX constellation before demodulation");
            draw_constellation_plot(
                "##RxGuiConstellation",
                "RX Symbols",
                display_frame->constellation,
                220.0f
            );

            ImGui::Spacing();
            ImGui::Text("Channel estimate magnitude");
            draw_channel_plot(
                "##RxGuiChannelMag",
                display_frame->channel,
                false,
                180.0f
            );

            ImGui::Text("Channel estimate phase");
            draw_channel_plot(
                "##RxGuiChannelPhase",
                display_frame->channel,
                true,
                180.0f
            );
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Last valid frame is not available yet.");
        }

        ImGui::End();

        ImGui::Render();
        ImGuiIO& io = ImGui::GetIO();
        glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    {
        std::lock_guard<std::mutex> lock(shared.mutex);
        shared.quit = true;
    }

    rx_thread.join();
    rx_decode_thread.join();
    shutdown_gui(window, gl_context);
    return 0;
}
