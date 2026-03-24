cmake_minimum_required(VERSION 3.10)
project(ofdm_visualizer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Поиск зависимостей
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

# 2. Поиск FFTW через pkg-config (работает на Ubuntu!)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFTW3 REQUIRED fftw3)

# 3. Исходники ImGui
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/imgui)
set(IMPLOT_DIR ${CMAKE_SOURCE_DIR}/implot)

file(GLOB IMGUI_SOURCES 
    ${IMGUI_DIR}/*.cpp 
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp 
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)

file(GLOB IMPLOT_SOURCES ${IMPLOT_DIR}/*.cpp)

# 4. Сборка исполняемого файла
add_executable(ofdm_visualizer 
    main.cpp 
    ${IMGUI_SOURCES} 
    ${IMPLOT_SOURCES})

# 5. Настройка заголовков
target_include_directories(ofdm_visualizer PRIVATE 
    ${IMGUI_DIR} 
    ${IMGUI_DIR}/backends 
    ${IMPLOT_DIR}
    ${FFTW3_INCLUDE_DIRS})  # Пути от pkg-config

# 6. Линковка библиотек
target_link_libraries(ofdm_visualizer PRIVATE 
    glfw 
    ${OPENGL_LIBRARIES} 
    ${FFTW3_LIBRARIES}    # Библиотеки от pkg-config
    m 
    dl)

# 7. Определение версии OpenGL для ImGui
target_compile_definitions(ofdm_visualizer PRIVATE 
    IMGUI_IMPL_OPENGL_LOADER_GLAD=1)