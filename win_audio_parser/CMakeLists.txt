cmake_minimum_required(VERSION 3.14)
project(WinAudioParser LANGUAGES C CXX)

# Глобальные настройки
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === 1. УДАЛЕННЫЕ ЗАВИСИМОСТИ ===
include(FetchContent)

set(KISSFFT_TOOLS OFF CACHE BOOL "" FORCE)
set(KISSFFT_TEST OFF  CACHE BOOL "" FORCE)

FetchContent_Declare(
    kissfft
    GIT_REPOSITORY https://github.com/mborgerding/kissfft.git
    GIT_TAG        131.2.0
)
FetchContent_MakeAvailable(kissfft)

# === 2. ВАШЕ ПРИЛОЖЕНИЕ (ЦЕЛЬ) ===
add_executable(${PROJECT_NAME}
    "src/main.cpp"
)

# подсоединяем библиотеку из fetchcontent
# (kissfft::kissfft создаётся в субпроекте)
target_link_libraries(${PROJECT_NAME} PRIVATE kissfft::kissfft)

# === 3. ЛОКАЛЬНЫЕ ПУТИ ===
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