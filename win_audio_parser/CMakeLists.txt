cmake_minimum_required(VERSION 3.10)
project(WinAudioParser LANGUAGES C CXX)

# 1. Настройки стандарта
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. Пути к инклюдам (заменяем -I из tasks.json)
include_directories(
    "src"
    "../common"             # Общая папка с протоколом
    "lib/miniaudio"         # Путь к miniaudio
    "lib/kissfft"           # Путь к kissfft
)

# 3. Определение исходных файлов
set(SOURCES
    "src/main.cpp"
    "lib/kissfft/kiss_fft.c"
    "lib/kissfft/kiss_fftr.c"
)

# 4. Создание исполняемого файла
add_executable(${PROJECT_NAME} ${SOURCES})

# 5. Линковка системных библиотек Windows (заменяем -lole32 -lwinmm)
target_link_libraries(${PROJECT_NAME} PRIVATE ole32 winmm)

# 6. Специфичные настройки для MinGW/w64devkit
if(MINGW)
    target_compile_options(${PROJECT_NAME} PRIVATE -fdiagnostics-color=always -g)
endif()