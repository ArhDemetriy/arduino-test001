cmake_minimum_required(VERSION 3.14) # 3.14+ лучше работает с FetchContent
project(WinAudioParser LANGUAGES C CXX)

# Глобальные настройки
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Заставляем CMake писать полные команды в compile_commands.json (для IDE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES OFF)

# === 1. УДАЛЕННЫЕ ЗАВИСИМОСТИ ===
include(FetchContent)
FetchContent_Declare(
    kissfft
    GIT_REPOSITORY https://github.com/mborgerding/kissfft.git
    GIT_TAG        v131
)
FetchContent_MakeAvailable(kissfft)

# === 2. ВАШЕ ПРИЛОЖЕНИЕ (ЦЕЛЬ) ===
add_executable(${PROJECT_NAME} "src/main.cpp")

# === 3. ЛОКАЛЬНЫЕ ПУТИ ===
# Вычисляем красивый абсолютный путь к общей папке
file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../common" COMMON_DIR)

# Привязываем пути ТОЛЬКО к нашей цели (PRIVATE означает "только для сборки этой цели")
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/miniaudio"
    "${COMMON_DIR}"
)

# === 4. ЛИНКОВКА ===
# Объединяем наше приложение, системные либы Windows и удаленный kissfft
target_link_libraries(${PROJECT_NAME} PRIVATE
    ole32
    winmm
    kissfft-float
)