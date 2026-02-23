cmake_minimum_required(VERSION 3.14) # 3.14+ лучше работает с FetchContent
project(WinAudioParser LANGUAGES C CXX)

# Глобальные настройки
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === 1. УДАЛЕННЫЕ ЗАВИСИМОСТИ ===
include(FetchContent)
FetchContent_Declare(
    kissfft
    GIT_REPOSITORY https://github.com/mborgerding/kissfft.git
    GIT_TAG        v131
)
FetchContent_MakeAvailable(kissfft)

# print path for debugging
message(STATUS "kissfft_SOURCE_DIR='${kissfft_SOURCE_DIR}'")

# === 2. ВАШЕ ПРИЛОЖЕНИЕ (ЦЕЛЬ) ===
add_executable(${PROJECT_NAME}
    "src/main.cpp"
    "${kissfft_SOURCE_DIR}/kiss_fft.c"
    "${kissfft_SOURCE_DIR}/tools/kiss_fftr.c"
)

# ensure C sources are treated as C (not C++)
set_source_files_properties(
    "${kissfft_SOURCE_DIR}/kiss_fft.c"
    "${kissfft_SOURCE_DIR}/tools/kiss_fftr.c"
    PROPERTIES LANGUAGE C
)

# === 3. ЛОКАЛЬНЫЕ ПУТИ ===
# Вычисляем красивый абсолютный путь к общей папке
file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../common" COMMON_DIR)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${COMMON_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/miniaudio
    ${kissfft_SOURCE_DIR}
    ${kissfft_SOURCE_DIR}/tools
)

# === 4. ЛИНКОВКА ===
target_link_libraries(${PROJECT_NAME} PRIVATE
    ole32
    winmm
)