cmake_minimum_required(VERSION 3.16)
project(OFDM CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === Зависимости ===
find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)

if(WIN32)
    message(WARNING "FFTW3 not auto-detected on Windows. Please set FFTW3_INCLUDE_DIR and FFTW3_LIBRARY manually.")
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFTW3 REQUIRED fftw3)
endif()

# === Пути к третьим сторонам ===
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui")
set(IMPLOT_DIR "${CMAKE_SOURCE_DIR}/third_party/implot")

# === Исходники ImGui ===
file(GLOB IMGUI_SOURCES
    ${IMGUI_DIR}/*.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# === Исходники ImPlot ===
file(GLOB IMPLOT_SOURCES
    ${IMPLOT_DIR}/*.cpp
)

# === Исходники проекта ===
set(PROJECT_SOURCES
    src/main.cpp
    src/gui.cpp
    include/functions.cpp
    include/modulations.cpp
    include/ofdm.cpp
)

# === Создаём исполняемый файл ===
add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCES}
    ${IMGUI_SOURCES}
    ${IMPLOT_SOURCES}
)

# === Include directories ===
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
)

# Добавляем FFTW3 includes
if(WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE ${FFTW3_INCLUDE_DIR})
else()
    target_include_directories(${PROJECT_NAME} PRIVATE ${FFTW3_INCLUDE_DIRS})
endif()

# === Линковка ===
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${OPENGL_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${GLEW_LIBRARIES}
)

# Линкуем FFTW3
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${FFTW3_LIBRARY})
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE ${FFTW3_LIBRARIES})
endif()

# Дополнительные библиотеки для Linux
if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE pthread m)
endif()

# === Дефайны для ImGui ===
target_compile_definitions(${PROJECT_NAME} PRIVATE
    IMGUI_IMPL_OPENGL_LOADER_GLEW
)

sudo apt update
sudo apt install libsdl2-dev libglew-dev libgl1-mesa-dev libfftw3-dev