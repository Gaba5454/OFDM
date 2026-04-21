cmake_minimum_required(VERSION 3.14)
project(backend_notes)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/third_party)
set(SRC_DIR ${PROJECT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

# === Зависимости ===
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFTW3 REQUIRED fftw3)

# === ImGui (с бэкендами) ===
add_library(imgui STATIC
    ${THIRD_PARTY_DIR}/imgui/imgui.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_draw.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_demo.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_tables.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_widgets.cpp
    ${THIRD_PARTY_DIR}/imgui/backends/imgui_impl_sdl2.cpp
    ${THIRD_PARTY_DIR}/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC 
    ${THIRD_PARTY_DIR}/imgui
    ${THIRD_PARTY_DIR}/imgui/backends
    ${SDL2_INCLUDE_DIRS}        # ← ДОБАВЛЕНО: пути к заголовкам SDL2
    ${OPENGL_INCLUDE_DIR}       # ← ДОБАВЛЕНО: для OpenGL-бэкенда
    ${GLEW_INCLUDE_DIRS}        # ← ДОБАВЛЕНО: если используется GLEW
)

target_compile_options(imgui PRIVATE -fPIC)

# === Линковка зависимостей для imgui ===
target_link_libraries(imgui PUBLIC 
    ${SDL2_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${GLEW_LIBRARIES}
)

# === ImPlot ===
add_library(implot STATIC
    ${THIRD_PARTY_DIR}/implot/implot.cpp
    ${THIRD_PARTY_DIR}/implot/implot_demo.cpp
    ${THIRD_PARTY_DIR}/implot/implot_items.cpp
)
target_include_directories(implot PUBLIsC ${THIRD_PARTY_DIR}/implot)
target_link_libraries(implot PUBLIC imgui)  # ← PUBLIC для передачи зависимостей
target_compile_options(implot PRIVATE -fPIC)

# === Основной исполняемый файл ===
add_executable(main ${SRC_DIR}/main.cpp)

target_include_directories(main PRIVATE 
    ${INCLUDE_DIR}
    ${FFTW3_INCLUDE_DIRS}
)

target_link_libraries(main PRIVATE 
    imgui 
    implot 
    ${SDL2_LIBRARIES} 
    ${OPENGL_LIBRARIES} 
    ${GLEW_LIBRARIES} 
    ${FFTW3_LIBRARIES}
    pthread m  # ← pthread и math для Linux
)

target_link_directories(main PRIVATE ${FFTW3_LIBRARY_DIRS})