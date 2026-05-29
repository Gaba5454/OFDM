cmake_minimum_required(VERSION 3.16)
project(OFDM LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_GUI "Build with GUI support" ON)
option(ENABLE_WARNINGS "Enable compiler warnings" ON)


# === Зависимости ===
find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)
find_package(PkgConfig REQUIRED)

if(WIN32)
    find_path(FFTW3_INCLUDE_DIR NAMES fftw3.h PATHS "C:/fftw/include" "$ENV{FFTW_DIR}/include")
    find_library(FFTW3_LIBRARY NAMES libfftw3-3 fftw3 PATHS "C:/fftw/lib" "$ENV{FFTW_DIR}/lib")
    if(NOT FFTW3_INCLUDE_DIR OR NOT FFTW3_LIBRARY)
        message(FATAL_ERROR "FFTW3 not found. Set FFTW3_INCLUDE_DIR and FFTW3_LIBRARY.")
    endif()
else()
    pkg_check_modules(FFTW3 REQUIRED IMPORTED_TARGET fftw3)
    add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/SoapySDR)
endif()

# === Исходники ===
set(PROJECT_SOURCES
    src/main.cpp 
    src/gui.cpp
    include/modulations.cpp 
    include/demodulations.cpp 
    include/ofdm_symbol.cpp 
    include/cycle_prefix.cpp
    include/channel_simulate.cpp 
    include/corellations.cpp 
    include/cfo_functions.cpp
    include/pss_generator.cpp 
    include/translate.cpp 
    include/receive.cpp
    include/simulation.cpp 
    include/another_functions.cpp
)

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui")
set(IMPLOT_DIR "${CMAKE_SOURCE_DIR}/third_party/implot")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_demo.cpp ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
set(IMPLOT_SOURCES ${IMPLOT_DIR}/implot.cpp ${IMPLOT_DIR}/implot_items.cpp)

# === Цель сборки ===
add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCES}
    $<$<BOOL:${BUILD_GUI}>:${IMGUI_SOURCES} ${IMPLOT_SOURCES}>
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include ${IMGUI_DIR} ${IMGUI_DIR}/backends ${IMPLOT_DIR}
    ${SDL2_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS}
    $<$<BOOL:WIN32>:${FFTW3_INCLUDE_DIR}>
    $<$<NOT:$<BOOL:WIN32>>:${FFTW3_INCLUDE_DIRS}>
)

set(BASE_LIBS ${OPENGL_LIBRARIES} ${SDL2_LIBRARIES} ${GLEW_LIBRARIES})
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${FFTW3_LIBRARY} ${BASE_LIBS})
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::FFTW3 SoapySDR ${BASE_LIBS} pthread m)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    IMGUI_IMPL_OPENGL_LOADER_GLEW
    $<$<NOT:$<BOOL:${BUILD_GUI}>>:NO_GUI>
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)