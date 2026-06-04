#include "../include/gui.h"

// Функция Хэннинга (окно для сглаживания)
static inline double hann_window(int n, int N) {
    return 0.5 * (1.0 - std::cos(2.0 * M_PI * n / (N - 1)));
}

// Расчёт спектрограммы через FFTW3
std::vector<float> compute_spectrogram_fftw(
    const std::vector<CF>& signal,
    int fft_size,
    int hop_size,
    int& out_rows,   // частотные бины (на выходе)
    int& out_cols    // временные окна (на выходе)
) {
    if (signal.empty() || fft_size < 64) {
        out_rows = out_cols = 0;
        return {};
    }

    out_rows = fft_size / 2 + 1;  // только положительные частоты
    out_cols = 0;
    std::vector<float> magnitude_data;

    // Буферы для FFTW: комплексные числа в формате double[2]
    fftw_complex* in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * fft_size);
    fftw_complex* out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * fft_size);
    
    // План БПФ (FFTW_ESTIMATE — быстрый, FFTW_MEASURE — точнее, но медленнее при инициализации)
    fftw_plan plan = fftw_plan_dft_1d(fft_size, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    // Скользящее окно по сигналу
    for (size_t start = 0; start + fft_size <= signal.size(); start += hop_size) {
        // Заполняем входной буфер с окном Хэннинга
        for (int i = 0; i < fft_size; ++i) {
            double w = hann_window(i, fft_size);
            in[i][0] = signal[start + i].real() * w;  // real part
            in[i][1] = signal[start + i].imag() * w;  // imag part
        }
        
        // Выполняем БПФ
        fftw_execute(plan);
        
        // Извлекаем magnitude только для положительных частот (0...N/2)
        for (int i = 0; i < out_rows; ++i) {
            double real = out[i][0];
            double imag = out[i][1];
            double mag = std::sqrt(real*real + imag*imag);
            // Конвертируем в dB с защитой от log(0)
            float db = 20.0f * std::log10(mag + 1e-10);
            magnitude_data.push_back(db);
        }
        ++out_cols;
        
        // Ограничение для производительности (опционально)
        if (out_cols >= 256) break;
    }

    // Очистка ресурсов FFTW
    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);
    // fftw_cleanup();  // можно вызвать в конце программы, но не обязательно

    return magnitude_data;
}

// Вспомогательная функция для разделения комплексных чисел
void complex_to_vectors(const std::vector<CF>& in, std::vector<double>& out_real, std::vector<double>& out_imag) {
    out_real.resize(in.size());
    out_imag.resize(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        out_real[i] = in[i].real();
        out_imag[i] = in[i].imag();
    }
}

void run_gui(
    const std::string& original_text,
    const std::vector<uint8_t>& raw_bits,
    const std::vector<CF>& qpsk_symbols,
    const std::vector<CF>& pss_signal,
    const std::vector<CF>& ofdm_symbols,
    const std::vector<CF>& ofdm_with_cp,
    const std::vector<CF>& tx_array,
    double SNR,
    const std::vector<double>& correlation_map,
    size_t peak_position,
    const std::vector<CF>& data_after_pss,
    const std::string& recovered_text,           
    const std::vector<CF>& received_constellation  
) {
    (void)SNR;

    // 1. Инициализация SDL и OpenGL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cout << "Error: SDL_Init failed\n";
        return;
    }
    
    SDL_Window* window = SDL_CreateWindow("OFDM Visualization", 
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                          1920, 1080, 
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cout << "Error: GLEW init failed\n";
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    // 2. Инициализация ImGui и ImPlot
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImPlot::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Подготовка данных (выполняется один раз)
    std::vector<double> bits_x, bits_y;
    for(size_t i=0; i<raw_bits.size(); ++i) {
        bits_x.push_back((double)i); bits_x.push_back((double)i+1);
        bits_y.push_back((double)raw_bits[i]); bits_y.push_back((double)raw_bits[i]);
    }

    std::vector<double> qpsk_i, qpsk_q;
    for(const auto& s : qpsk_symbols) {
        qpsk_i.push_back(s.real());
        qpsk_q.push_back(s.imag());
    }

    std::vector<double> ofdm_time_real, ofdm_time_imag;
    complex_to_vectors(ofdm_symbols, ofdm_time_real, ofdm_time_imag);
    
    std::vector<double> ofdm_cp_real;
    for(const auto& s : ofdm_with_cp) ofdm_cp_real.push_back(s.real());
    
    std::vector<double> pss_real, pss_imag;
    complex_to_vectors(pss_signal, pss_real, pss_imag);

    std::vector<double> tx_real, tx_imag;
    complex_to_vectors(tx_array, tx_real, tx_imag);

    // Переменные интерфейса
    float noise_level = 0.1f;
    bool show_grid = true;
    int plot_height = 250;

    // === Подготовка спектрограммы через FFTW3 ===
    std::vector<float> spectrogram_values;
    int spec_rows = 0, spec_cols = 0;
    {
        int fft_size = 512;           
        int hop_size = fft_size / 4;  
        
        spectrogram_values = compute_spectrogram_fftw(tx_array, fft_size, hop_size, spec_rows, spec_cols);
    }

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
        
        // Основное окно
        ImGui::Begin("OFDM System Monitor");
        
        // Рассчитываем доступную ширину для разделения на 25% / 75%
        float avail_width = ImGui::GetContentRegionAvail().x;
        float left_w = avail_width * 0.25f;
        float right_w = avail_width * 0.75f;

        // === ЛЕВАЯ ПАНЕЛЬ (Инфо + Настройки) ===
        ImGui::BeginChild("LeftPanel", ImVec2(left_w, 0), true);
        
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "INFORMATION");
        ImGui::Separator();
        ImGui::Text("Input text: %s", original_text.c_str());
        ImGui::Text("Raw bits: %zu", raw_bits.size());
        ImGui::Text("QPSK symbols: %zu", qpsk_symbols.size());
        ImGui::Text("PSS ength: %zu", pss_signal.size());
        ImGui::Text("OFDM-symbol length: %zu", ofdm_symbols.size());
        ImGui::Text("CP length: %zu", ofdm_with_cp.size() - ofdm_symbols.size());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "SYNCHRONIZATION");
        ImGui::Text("PSS Peak Pos: %zu", peak_position);
        if (!correlation_map.empty() && peak_position < correlation_map.size()) {
            ImGui::Text("Peak Value: %.3f", correlation_map[peak_position]);
        }
        ImGui::Text("Decoded Text: %s", recovered_text.c_str());
        ImGui::Text("Match: %s", (recovered_text == original_text ? "YES" : "NO"));
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "PARAMETERS");
        ImGui::SliderFloat("Noise Level", &noise_level, 0.0f, 1.0f, "%.2f");
        
        ImGui::Checkbox("Show Grid", &show_grid);
        ImGui::SliderInt("Plot Height", &plot_height, 100, 500);
        
        ImGui::EndChild();

        // === ПРАВАЯ ПАНЕЛЬ (Графики, скролл) ===
        ImGui::SameLine();
        ImGui::BeginChild("RightPanel", ImVec2(right_w, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
        
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "SIGNAL ANALYSIS");
        ImGui::Separator();

        // 1. Биты
        ImGui::Text("1. Bit Stream");
        if (ImPlot::BeginPlot("##Bits", ImVec2(-1, plot_height))) {
            if(show_grid) ImPlot::SetupAxesLimits(ImAxis_Y1, -0.2, 1.2, ImGuiCond_Always);
            ImPlot::PlotStairs("Bits", bits_x.data(), bits_y.data(), bits_x.size());
            ImPlot::EndPlot();
        }
        ImGui::Spacing();

        // 2. Созвездие QPSK
        ImGui::Text("2. QPSK Constellation");
        if (ImPlot::BeginPlot("##Constellation", ImVec2(-1, plot_height))) {
            ImPlot::SetupAxisLimits(ImAxis_X1, -1.5, 1.5);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -1.5, 1.5);
            ImPlot::PlotScatter("Symbols", qpsk_i.data(), qpsk_q.data(), qpsk_i.size());
            ImPlot::EndPlot();
        }
        ImGui::Spacing();

        // 3. PSS
        ImGui::Text("3. PSS Signal");
        if (ImPlot::BeginPlot("##PSS", ImVec2(-1, plot_height))) {
            std::vector<double> x_pss(pss_real.size());
            std::iota(x_pss.begin(), x_pss.end(), 0.0);
            ImPlot::PlotLine("Real", x_pss.data(), pss_real.data(), x_pss.size());
            ImPlot::PlotLine("Imag", x_pss.data(), pss_imag.data(), x_pss.size());
            ImPlot::EndPlot();
        }
        ImGui::Spacing();

        // 4. OFDM без CP
        ImGui::Text("4. OFDM Symbol (No CP)");
        if (ImPlot::BeginPlot("##OFDM_Time", ImVec2(-1, plot_height))) {
            std::vector<double> x_ofdm(ofdm_time_real.size());
            std::iota(x_ofdm.begin(), x_ofdm.end(), 0.0);
            ImPlot::PlotLine("Real", x_ofdm.data(), ofdm_time_real.data(), x_ofdm.size());
            ImPlot::PlotLine("Imag", x_ofdm.data(), ofdm_time_imag.data(), x_ofdm.size());
            ImPlot::EndPlot();
        }
        ImGui::Spacing();

        // 5. OFDM с CP
        ImGui::Text("5. OFDM with Cyclic Prefix");
        if (ImPlot::BeginPlot("##OFDM_CP", ImVec2(-1, plot_height))) {
            std::vector<double> x_cp(ofdm_cp_real.size());
            std::iota(x_cp.begin(), x_cp.end(), 0.0);
            ImPlot::PlotLine("Real", x_cp.data(), ofdm_cp_real.data(), x_cp.size());
            size_t cp_len = ofdm_with_cp.size() - ofdm_symbols.size();
            if(cp_len > 0 && cp_len < x_cp.size()) {
                double v_line_x = static_cast<double>(cp_len);
                ImPlot::PlotInfLines("CP End", &v_line_x, 1);
            }
            ImPlot::EndPlot();
        }
        ImGui::Spacing();

        // 6. Полный кадр
        ImGui::Text("6. Full Tx Frame");
        if (ImPlot::BeginPlot("##Tx_Frame", ImVec2(-1, plot_height * 1.5f))) {
            std::vector<double> x_tx(tx_array.size());
            std::iota(x_tx.begin(), x_tx.end(), 0.0);
            ImPlot::PlotLine("Real", x_tx.data(), tx_real.data(), x_tx.size());
            ImPlot::EndPlot();
        }
        ImGui::Spacing();

        // 7. Корреляция
        ImGui::Text("7. PSS Correlation Peak");
        if (ImPlot::BeginPlot("##Corr", ImVec2(-1, plot_height))) {
            if(!correlation_map.empty()) {
                std::vector<double> x_corr(correlation_map.size());
                std::iota(x_corr.begin(), x_corr.end(), 0.0);
                ImPlot::PlotLine("Corr", x_corr.data(), correlation_map.data(), x_corr.size());
                double peak_x = static_cast<double>(peak_position);
                ImPlot::PlotInfLines("Peak", &peak_x, 1);
            }
            ImPlot::EndPlot();
        }
        ImGui::Spacing();

        // 8. Данные после PSS
        ImGui::Text("8. Data Stream (After PSS)");
        if (!data_after_pss.empty()) {
            if (ImPlot::BeginPlot("Data Time", ImVec2(-1, plot_height))) {
                std::vector<double> x_data(data_after_pss.size());
                std::vector<double> y_real(data_after_pss.size());
                for(size_t i=0; i<data_after_pss.size(); ++i) {
                    x_data[i] = static_cast<double>(i);
                    y_real[i] = data_after_pss[i].real();
                }
                ImPlot::PlotLine("Real", x_data.data(), y_real.data(), x_data.size());
                ImPlot::EndPlot();
            }
            ImGui::Text("Samples: %zu | Approx symbols: %zu", 
                        data_after_pss.size(), 
                        data_after_pss.size() / (LTE + CP_LENGTH));
        } else {
            ImGui::TextColored(ImVec4(1,0.3,0.3,1), "No data extracted after PSS.");
        }
        ImGui::Spacing();

        // 9. Созвездие принятых символов
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "9. Received Constellation");
        if (!received_constellation.empty()) {
            if (ImPlot::BeginPlot("##RxConstellation", ImVec2(-1, 250))) {
                ImPlot::SetupAxisLimits(ImAxis_X1, -1.5, 1.5);
                ImPlot::SetupAxisLimits(ImAxis_Y1, -1.5, 1.5);
                
                std::vector<double> rx_i, rx_q;
                size_t show_n = std::min(received_constellation.size(), size_t(50));
                for(size_t k=0; k<show_n; ++k) {
                    rx_i.push_back(received_constellation[k].real());
                    rx_q.push_back(received_constellation[k].imag());
                }
                ImPlot::PlotScatter("Rx Points", rx_i.data(), rx_q.data(), rx_i.size());
                
                std::vector<double> id_i = {0.7, -0.7, -0.7, 0.7};
                std::vector<double> id_q = {0.7, 0.7, -0.7, -0.7};
                ImPlot::PlotScatter("Ideal QPSK", id_i.data(), id_q.data(), 4);
                ImPlot::EndPlot();
            }
        } else {
            ImGui::Text("No constellation data available.");
        }

        ImGui::EndChild(); // Конец правой панели
        ImGui::End();      // Конец главного окна

        // Рендеринг кадра
        ImGui::Render();
        glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
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
