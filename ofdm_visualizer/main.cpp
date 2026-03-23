#include <iostream>
#include <fftw3.h>
#include <cmath>
#include <complex>
#include <vector>
#include <iomanip>

// ImGui + ImPlot
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

// GLFW
#include <GLFW/glfw3.h>

#define N_SUBCARRIERS 64
#define N_USED         52
#define CP_LENGTH      16

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Глобальные данные для отрисовки (float для совместимости с ImPlot)
std::vector<float> plot_time_real, plot_time_imag, plot_time_amp;
std::vector<float> plot_freq_amp;
std::vector<float> plot_const_real, plot_const_imag;

// ✅ Исправленная генерация QPSK символов (4 точки)
std::complex<double> generate_qpsk_symbol(int seed) {
    // QPSK: 4 точки в комплексной плоскости
    switch (seed % 4) {
        case 0: return std::complex<double>(1.0, 1.0);
        case 1: return std::complex<double>(-1.0, 1.0);
        case 2: return std::complex<double>(-1.0, -1.0);
        case 3: return std::complex<double>(1.0, -1.0);
        default: return std::complex<double>(1.0, 1.0);
    }
}

// Основная функция OFDM-передатчика
void run_ofdm_transmitter() {
    // 1. Формируем частотные символы
    std::vector<std::complex<double>> freq_symbols(N_SUBCARRIERS, {0, 0});
    
    int data_idx = 0;
    for (int k = -N_USED/2; k <= N_USED/2; k++) {
        if (k == 0) continue;  // DC null
        int idx = (k < 0) ? k + N_SUBCARRIERS : k;
        freq_symbols[idx] = generate_qpsk_symbol(data_idx++);
    }
    
    // 2. Выделяем память FFTW
    fftw_complex *in  = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N_SUBCARRIERS);
    fftw_complex *out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N_SUBCARRIERS);
    
    for (int i = 0; i < N_SUBCARRIERS; i++) {
        in[i][0] = freq_symbols[i].real();
        in[i][1] = freq_symbols[i].imag();
    }
    
    // 3. IFFT
    fftw_plan ifft_plan = fftw_plan_dft_1d(N_SUBCARRIERS, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_execute(ifft_plan);
    
    // 4. Нормировка и подготовка данных для графиков
    plot_time_real.clear(); plot_time_imag.clear(); plot_time_amp.clear();
    plot_freq_amp.clear();
    plot_const_real.clear(); plot_const_imag.clear();
    
    for (int i = 0; i < N_SUBCARRIERS; i++) {
        double real = out[i][0] / N_SUBCARRIERS;
        double imag = out[i][1] / N_SUBCARRIERS;
        double amp = sqrt(real*real + imag*imag);
        
        // Конвертируем в float для ImPlot
        plot_time_real.push_back(static_cast<float>(real));
        plot_time_imag.push_back(static_cast<float>(imag));
        plot_time_amp.push_back(static_cast<float>(amp));
    }
    
    // Спектр амплитуд по поднесущим
    for (int i = 0; i < N_SUBCARRIERS; i++) {
        double amp = std::abs(freq_symbols[i]);
        plot_freq_amp.push_back(static_cast<float>(amp));
    }
    
    // Созвездие (только активные поднесущие)
    for (int k = -N_USED/2; k <= N_USED/2; k++) {
        if (k == 0) continue;
        int idx = (k < 0) ? k + N_SUBCARRIERS : k;
        plot_const_real.push_back(static_cast<float>(freq_symbols[idx].real()));
        plot_const_imag.push_back(static_cast<float>(freq_symbols[idx].imag()));
    }
    
    // Очистка
    fftw_destroy_plan(ifft_plan);
    fftw_free(in);
    fftw_free(out);
}

// Обработчик закрытия окна
void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << "\n";
}

int main() {
    // Инициализация GLFW
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;
    
    // Настройка OpenGL
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(1280, 900, "OFDM Visualizer", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // VSync
    
    // Инициализация ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // ✅ Загрузка шрифта с поддержкой кириллицы
    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;
    
    // Список возможных путей к шрифтам с кириллицей
    const char* font_paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
    };
    
    bool font_loaded = false;
    for (const char* path : font_paths) {
        if (FILE* f = fopen(path, "rb")) {
            fclose(f);
            // Загружаем шрифт с поддержкой кириллицы
            static const ImWchar glyph_ranges[] = {
                0x0020, 0x00FF, // Basic Latin + Latin Supplement
                0x0400, 0x044F, // Cyrillic
                0x0450, 0x0459, // Cyrillic Supplement
                0x2100, 0x2144, // Letter-like symbols
                0,
            };
            io.Fonts->AddFontFromFileTTF(path, 16.0f, &font_cfg, glyph_ranges);
            font_loaded = true;
            std::cout << "✅ Font loaded: " << path << std::endl;
            break;
        }
    }
    
    if (!font_loaded) {
        std::cout << "⚠️  No Cyrillic font found, using default" << std::endl;
        io.Fonts->AddFontDefault();
    }
    
    // Стиль тёмной темы
    ImGui::StyleColorsDark();
    ImPlot::StyleColorsDark();
    
    // Инициализация бэкендов
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    
    // Параметры отрисовки
    bool show_time_domain = true;
    bool show_freq_domain = true;
    bool show_constellation = true;
    bool auto_update = true;
    int subcarriers = N_SUBCARRIERS;
    
    // Первый запуск
    run_ofdm_transmitter();
    
    // Главный цикл
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Новый кадр
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // === Панель управления ===
        ImGui::Begin("🎛️ Управление");
        
        ImGui::Checkbox("📈 Временная область", &show_time_domain);
        ImGui::Checkbox("📊 Частотная область", &show_freq_domain);
        ImGui::Checkbox("🔵 Созвездие", &show_constellation);
        ImGui::Checkbox("🔄 Автообновление", &auto_update);
        
        ImGui::Separator();
        
        if (ImGui::Button("🔄 Обновить")) {
            run_ofdm_transmitter();
        }
        
        ImGui::SliderInt("Поднесущие", &subcarriers, 16, 256);
        
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::End();
        
        // === График: Временной сигнал ===
        if (show_time_domain) {
            ImGui::Begin("⏱️ Временной сигнал (после IFFT)");
            if (ImPlot::BeginPlot("OFDM Symbol", ImVec2(-1, 300))) {
                ImPlot::SetupAxis(ImAxis_X1, "Отсчёт");
                ImPlot::SetupAxis(ImAxis_Y1, "Амплитуда");
                ImPlot::SetupLegend(ImPlotLocation_NorthEast);
                
                // Используем float для X, чтобы совпадало с Y
                std::vector<float> x(N_SUBCARRIERS);
                for (int i = 0; i < N_SUBCARRIERS; i++) x[i] = static_cast<float>(i);
                
                ImPlot::PlotLine("Real", x.data(), plot_time_real.data(), N_SUBCARRIERS);
                ImPlot::PlotLine("Imag", x.data(), plot_time_imag.data(), N_SUBCARRIERS);
                ImPlot::PlotLine("Amplitude", x.data(), plot_time_amp.data(), N_SUBCARRIERS);
                
                ImPlot::EndPlot();
            }
            ImGui::End();
        }
        
        // === График: Частотный спектр ===
        if (show_freq_domain) {
            ImGui::Begin("📡 Спектр поднесущих");
            if (ImPlot::BeginPlot("Frequency Domain", ImVec2(-1, 300))) {
                ImPlot::SetupAxis(ImAxis_X1, "Индекс поднесущей");
                ImPlot::SetupAxis(ImAxis_Y1, "Амплитуда");
                ImPlot::SetupAxesLimits(-N_SUBCARRIERS/2, N_SUBCARRIERS/2, 0, 2);
                
                // Центрируем спектр для наглядности
                std::vector<float> x_freq(N_SUBCARRIERS);
                std::vector<float> y_freq_shifted(N_SUBCARRIERS);
                for (int i = 0; i < N_SUBCARRIERS; i++) {
                    x_freq[i] = (i < N_SUBCARRIERS/2) ? static_cast<float>(i) : static_cast<float>(i - N_SUBCARRIERS);
                    y_freq_shifted[i] = plot_freq_amp[i];
                }
                
                ImPlot::PlotStems("Amplitude", x_freq.data(), y_freq_shifted.data(), N_SUBCARRIERS);
                
                ImPlot::EndPlot();
            }
            ImGui::End();
        }
        
        // === График: Созвездие ===
        if (show_constellation) {
            ImGui::Begin("🔵 Созвездие модуляции");
            if (ImPlot::BeginPlot("Constellation", ImVec2(-1, -1), ImPlotFlags_Equal)) {
                ImPlot::SetupAxis(ImAxis_X1, "I (Real)");
                ImPlot::SetupAxis(ImAxis_Y1, "Q (Imag)");
                ImPlot::SetupAxesLimits(-2, 2, -2, 2);
                ImPlot::PlotScatter("Symbols", plot_const_real.data(), plot_const_imag.data(), plot_const_real.size());
                
                // Опорные точки для QPSK
                if (plot_const_real.size() > 0) {
                    double ref_x[] = {1.0, -1.0, 1.0, -1.0};
                    double ref_y[] = {1.0, 1.0, -1.0, -1.0};
                    ImPlot::PlotScatter("QPSK Ref", ref_x, ref_y, 4);
                }
                
                ImPlot::EndPlot();
            }
            ImGui::End();
        }
        
        // Отрисовка
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    
    // Очистка
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}