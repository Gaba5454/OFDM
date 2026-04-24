#include "../include/gui.h"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <imgui.h>
#include <implot.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <iostream>
#include <numeric> // Для std::iota

// Вспомогательная функция для преобразования complex vector в два вектора double (Real/Imag)
void complex_to_vectors(const std::vector<CD>& in, std::vector<double>& out_real, std::vector<double>& out_imag) {
    out_real.resize(in.size());
    out_imag.resize(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        out_real[i] = in[i].real();
        out_imag[i] = in[i].imag();
    }
}

void run_gui(
    const std::string& original_text,
    const std::vector<int8_t>& raw_bits,
    const std::vector<CD>& qpsk_symbols,
    const std::vector<CD>& pss_signal,
    const std::vector<CD>& ofdm_symbols,
    const std::vector<CD>& ofdm_with_cp) 
{
    // 1. Инициализация SDL и OpenGL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cout << "Error: SDL_Init failed\n";
        return;
    }
    
    SDL_Window* window = SDL_CreateWindow("OFDM Visualization", 
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                          1600, 900, 
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cout << "Error: GLEW init failed\n";
        return;
    }

    // 2. Инициализация ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Подготовка данных для графиков
    std::vector<double> bits_x, bits_y;
    for(size_t i=0; i<raw_bits.size(); ++i) {
        bits_x.push_back(i); bits_x.push_back(i+1);
        bits_y.push_back(raw_bits[i]); bits_y.push_back(raw_bits[i]);
    }

    std::vector<double> qpsk_i, qpsk_q;
    for(const auto& s : qpsk_symbols) {
        qpsk_i.push_back(s.real());
        qpsk_q.push_back(s.imag());
    }

    std::vector<double> ofdm_time_real, ofdm_time_imag;
    complex_to_vectors(ofdm_symbols, ofdm_time_real, ofdm_time_imag);
    
    std::vector<double> ofdm_cp_real;
    for(const auto& s : ofdm_with_cp) {
        ofdm_cp_real.push_back(s.real());
    }
    
    std::vector<double> pss_real, pss_imag;
    complex_to_vectors(pss_signal, pss_real, pss_imag);

    // Главный цикл
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        
        // Создаем DockSpace
        ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
        ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, nullptr, dockspace_flags);

        // --- Окно 1: Инфо ---
        ImGui::Begin("System Info");
        ImGui::Text("Input Text: \"%s\"", original_text.c_str());
        ImGui::Separator();
        ImGui::Text("Raw Bits: %zu", raw_bits.size());
        ImGui::Text("QPSK Symbols: %zu", qpsk_symbols.size());
        ImGui::Text("PSS Samples: %zu", pss_signal.size());
        ImGui::Text("OFDM Samples (N=%zu): %zu", LTE, ofdm_symbols.size());
        ImGui::Text("With CP Samples: %zu", ofdm_with_cp.size());
        ImGui::End();

        // --- Окно 2: Биты ---
        ImGui::Begin("1. Bit Stream");
        if (ImPlot::BeginPlot("Binary Data")) {
            ImPlot::SetupAxisLimits(ImAxis_Y1, -0.2, 1.2, ImGuiCond_Always);
            ImPlot::PlotStairs("Bits", bits_x.data(), bits_y.data(), bits_x.size());
            ImPlot::EndPlot();
        }
        ImGui::End();

        // --- Окно 3: Созвездие QPSK ---
        ImGui::Begin("2. QPSK Constellation");
        if (ImPlot::BeginPlot("Constellation")) {
            ImPlot::SetupAxisLimits(ImAxis_X1, -1.5, 1.5);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -1.5, 1.5);
            ImPlot::SetupAxes("I (Real)", "Q (Imag)");
            ImPlot::PlotScatter("Symbols", qpsk_i.data(), qpsk_q.data(), qpsk_i.size());
            // Рисуем границы решений (серые пунктирные линии)
            ImPlot::PlotLine("Boundary X", std::vector<double>{0, 0}.data(), std::vector<double>{-1.5, 1.5}.data(), 2);
            ImPlot::PlotLine("Boundary Y", std::vector<double>{-1.5, 1.5}.data(), std::vector<double>{0, 0}.data(), 2);
            ImPlot::EndPlot(); 
        }
        ImGui::End();

        // --- Окно 4: PSS Signal ---
        ImGui::Begin("3. PSS Signal (Time Domain)");
        if (ImPlot::BeginPlot("PSS Waveform")) {
            std::vector<double> x_pss(pss_real.size());
            std::iota(x_pss.begin(), x_pss.end(), 0);
            ImPlot::PlotLine("Real", x_pss.data(), pss_real.data(), x_pss.size());
            ImPlot::PlotLine("Imag", x_pss.data(), pss_imag.data(), x_pss.size());
            ImPlot::EndPlot();
        }
        ImGui::End();

        // --- Окно 5: OFDM Symbol ---
        ImGui::Begin("4. OFDM Symbol (Before CP)");
        if (ImPlot::BeginPlot("OFDM Time Domain")) {
            std::vector<double> x_ofdm(ofdm_time_real.size());
            std::iota(x_ofdm.begin(), x_ofdm.end(), 0);
            ImPlot::PlotLine("Real Part", x_ofdm.data(), ofdm_time_real.data(), x_ofdm.size());
            ImPlot::PlotLine("Imag Part", x_ofdm.data(), ofdm_time_imag.data(), x_ofdm.size());
            ImPlot::EndPlot();
        }
        ImGui::End();

        // --- Окно 6: With CP ---
        ImGui::Begin("5. OFDM with Cyclic Prefix");
        if (ImPlot::BeginPlot("Final Tx Signal")) {
            std::vector<double> x_cp(ofdm_cp_real.size());
            std::iota(x_cp.begin(), x_cp.end(), 0);
            ImPlot::PlotLine("Real Part with CP", x_cp.data(), ofdm_cp_real.data(), x_cp.size());
            
            // Визуальная метка конца CP
            size_t cp_len = ofdm_with_cp.size() - ofdm_symbols.size();
            if(cp_len > 0 && cp_len < x_cp.size()) {
                 double v_line_x = static_cast<double>(cp_len);
                ImPlot::PlotInfLines("CP End", &v_line_x, 1); // 
            }
            ImPlot::EndPlot();
        }
        ImGui::End();

        // --- Окно 7: TX array ---
        ImGui::Begin("TX array");
        if (ImPlot::BeginPlot("Final Tx Signal")) {
            std::vector<double> x_cp(ofdm_cp_real.size());
            std::iota(x_cp.begin(), x_cp.end(), 0);
            ImPlot::PlotLine("Real Part with CP", x_cp.data(), ofdm_cp_real.data(), x_cp.size());
            
            // Визуальная метка конца CP
            size_t cp_len = ofdm_with_cp.size() - ofdm_symbols.size();
            if(cp_len > 0 && cp_len < x_cp.size()) {
                 double v_line_x = static_cast<double>(cp_len);
                ImPlot::PlotInfLines("CP End", &v_line_x, 1); // 
            }
            ImPlot::EndPlot();
        }
        ImGui::End();

        // Рендер
        ImGui::Render();
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Очистка
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}