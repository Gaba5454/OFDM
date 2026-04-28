#include "../include/gui.h"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <imgui.h>
#include <implot.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <iostream>
#include <numeric> // Для std::iota
#include <algorithm> // Для std::max_element

// Вспомогательная функция
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
    const std::vector<CD>& ofdm_with_cp,
    const std::vector<CD>& tx_array,
    const std::vector<double>& correlation_map,
    size_t peak_position) 
{
    // 1. Инициализация SDL и OpenGL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cout << "Error: SDL_Init failed\n";
        return;
    }
    
    // Создаем окно 1920x1080
    SDL_Window* window = SDL_CreateWindow("OFDM Visualization Pro", 
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                          1920, 1080, 
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE); // RESIZABLE позволяет менять размер вручную
    
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
    // Включаем Docking (стыковку окон)
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Опционально: темная тема сразу
    ImGui::StyleColorsDark();
    ImPlot::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Подготовка данных (один раз при старте)
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

    std::vector<double> tx_real, tx_imag;
    complex_to_vectors(tx_array, tx_real, tx_imag);

    // Переменные для параметров (можно менять в GUI)
    static float noise_level = 0.1f;
    static bool show_grid = true;
    static int plot_height = 300; // Высота каждого графика

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
        
        // === НАСТРОЙКА DOCKSPACE ===
        // Создаем полноэкранный DockSpace
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("DockSpace Demo", nullptr, window_flags);
        ImGui::PopStyleVar(2);
        
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        ImGui::End();

        // === ЛЕВАЯ ПАНЕЛЬ: ПАРАМЕТРЫ И ИНФО ===
        // Мы "прикрепляем" это окно к левой части экрана
        ImGui::Begin("Parameters & Info");
        
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "System Status: RUNNING");
        ImGui::Separator();
        
        // Информация о сигнале
        ImGui::Text("Input Text: \"%s\"", original_text.c_str());
        ImGui::Text("Raw Bits: %zu", raw_bits.size());
        ImGui::Text("QPSK Symbols: %zu", qpsk_symbols.size());
        ImGui::Text("PSS Samples: %zu", pss_signal.size());
        ImGui::Text("OFDM Size: %zu (FFT=%zu)", ofdm_with_cp.size(), LTE);
        
        ImGui::Separator();
        ImGui::Text("Sync Result:");
        ImGui::Text("PSS Peak Pos: %zu", peak_position);
        if (!correlation_map.empty()) {
             double peak_val = correlation_map[peak_position];
             ImGui::Text("Peak Value: %.2f", peak_val);
        }

        ImGui::Separator();
        ImGui::Text("Simulation Params:");
        ImGui::SliderFloat("Noise Level", &noise_level, 0.0f, 1.0f);
        ImGui::Checkbox("Show Grid", &show_grid);
        ImGui::SliderInt("Plot Height", &plot_height, 100, 600);

        ImGui::End();

        // === ПРАВАЯ ЧАСТЬ: ГРАФИКИ ===
        // Создаем контейнер для графиков, который можно скроллить
        ImGui::Begin("Signal Analysis");
        
        // Если нужно, чтобы графики шли друг за другом вертикально и скроллились:
        ImGui::BeginChild("GraphsScrollArea", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
        
        // --- График 1: Биты ---
        ImGui::Text("1. Bit Stream");
        if (ImPlot::BeginPlot("##Bits", ImVec2(-1, plot_height))) {
            if(show_grid) ImPlot::SetupAxesLimits(ImAxis_Y1, -0.2, 1.2, ImGuiCond_Always);
            ImPlot::PlotStairs("Bits", bits_x.data(), bits_y.data(), bits_x.size());
            ImPlot::EndPlot();
        }

        // --- График 2: Созвездие ---
        ImGui::Text("2. QPSK Constellation");
        if (ImPlot::BeginPlot("##Constellation", ImVec2(-1, plot_height))) {
            ImPlot::SetupAxisLimits(ImAxis_X1, -1.5, 1.5);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -1.5, 1.5);
            ImPlot::PlotScatter("Symbols", qpsk_i.data(), qpsk_q.data(), qpsk_i.size());
            // Границы
            ImPlot::PlotLine("##BoundX", std::vector<double>{0,0}.data(), std::vector<double>{-1.5,1.5}.data(), 2);
            ImPlot::PlotLine("##BoundY", std::vector<double>{-1.5,1.5}.data(), std::vector<double>{0,0}.data(), 2);
            ImPlot::EndPlot();
        }

        // --- График 3: PSS ---
        ImGui::Text("3. PSS Signal");
        if (ImPlot::BeginPlot("##PSS", ImVec2(-1, plot_height))) {
            std::vector<double> x_pss(pss_real.size());
            std::iota(x_pss.begin(), x_pss.end(), 0);
            ImPlot::PlotLine("Real", x_pss.data(), pss_real.data(), x_pss.size());
            ImPlot::PlotLine("Imag", x_pss.data(), pss_imag.data(), x_pss.size());
            ImPlot::EndPlot();
        }

        // --- График 4: OFDM Time ---
        ImGui::Text("4. OFDM Symbol (No CP)");
        if (ImPlot::BeginPlot("##OFDM_Time", ImVec2(-1, plot_height))) {
            std::vector<double> x_ofdm(ofdm_time_real.size());
            std::iota(x_ofdm.begin(), x_ofdm.end(), 0);
            ImPlot::PlotLine("Real", x_ofdm.data(), ofdm_time_real.data(), x_ofdm.size());
            ImPlot::PlotLine("Imag", x_ofdm.data(), ofdm_time_imag.data(), x_ofdm.size());
            ImPlot::EndPlot();
        }

        // --- График 5: With CP ---
        ImGui::Text("5. OFDM with Cyclic Prefix");
        if (ImPlot::BeginPlot("##OFDM_CP", ImVec2(-1, plot_height))) {
            std::vector<double> x_cp(ofdm_cp_real.size());
            std::iota(x_cp.begin(), x_cp.end(), 0);
            ImPlot::PlotLine("Real", x_cp.data(), ofdm_cp_real.data(), x_cp.size());
            
            size_t cp_len = ofdm_with_cp.size() - ofdm_symbols.size();
            if(cp_len > 0 && cp_len < x_cp.size()) {
                 double v_line_x = static_cast<double>(cp_len);
                 ImPlot::PlotInfLines("CP End", &v_line_x, 1);
            }
            ImPlot::EndPlot();
        }

        // --- График 6: Full Tx Frame ---
        ImGui::Text("6. Full Tx Frame");
        if (ImPlot::BeginPlot("##Tx_Frame", ImVec2(-1, plot_height * 1.5))) { // Этот график повыше
            std::vector<double> x_tx(tx_array.size());
            std::iota(x_tx.begin(), x_tx.end(), 0);
            ImPlot::PlotLine("Real", x_tx.data(), tx_real.data(), x_tx.size());
            ImPlot::EndPlot();
        }

        // --- График 7: Correlation ---
        ImGui::Text("7. PSS Correlation Peak");
        if (ImPlot::BeginPlot("##Corr", ImVec2(-1, plot_height))) {
            if(!correlation_map.empty()) {
                std::vector<double> x_corr(correlation_map.size());
                std::iota(x_corr.begin(), x_corr.end(), 0);
                ImPlot::PlotLine("Corr", x_corr.data(), correlation_map.data(), x_corr.size());
                
                double peak_x = static_cast<double>(peak_position);
                ImPlot::PlotInfLines("Peak", &peak_x, 1);
            }
            ImPlot::EndPlot();
        }

        ImGui::EndChild(); // Конец области скролла
        ImGui::End();      // Конец окна Signal Analysis

        // Рендеринг
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