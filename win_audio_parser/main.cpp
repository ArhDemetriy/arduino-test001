#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <iostream>
#include <vector>
#include <windows.h>
#include <cmath>
#include <cstdint>
#include <iomanip>
extern "C"
{
#include "kiss_fftr.h"
}

HANDLE hSerial;

bool initSerial(const char *portName)
{
    hSerial = CreateFileA(portName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSerial == INVALID_HANDLE_VALUE)
        return false;

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    GetCommState(hSerial, &dcbSerialParams);
    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    return SetCommState(hSerial, &dcbSerialParams);
}

// 1. Создаем перечисление для удобного выбора типа фильтра
enum FilterType
{
    FILTER_LPF, // Пропускает только низкие (Бас)
    FILTER_BPF, // Пропускает средние (Голос)
    FILTER_HPF  // Пропускает высокие (Тарелочки)
};

// 2. Описываем структуру одного канала (аналог твоего PinData)
struct BandData
{
    FilterType type;  // Тип фильтра
    double freq;      // Частота среза/центра
    float multiplier; // Усилитель громкости (чтобы подтянуть тихие частоты)

    // Храним все виды фильтров miniaudio (использоваться будет только нужный)
    ma_lpf lpf;
    ma_bpf bpf;
    ma_hpf hpf;

    std::vector<float> buffer; // Персональный буфер для этого канала
    uint8_t currentVal = 0;    // Текущая громкость (0-255)
};

// 3. Динамическая отправка пакета (собирает пакет под любое количество каналов)
void sendPacket(const std::vector<BandData> &bands)
{
    std::vector<uint8_t> packet;
    packet.push_back(0xFE); // Стартовый маркер

    for (const auto &band : bands)
    {
        packet.push_back(band.currentVal);
    }

    DWORD written;
    WriteFile(hSerial, packet.data(), packet.size(), &written, NULL);
}

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    // Получаем ссылку на наш массив диапазонов
    auto *bands = (std::vector<BandData> *)pDevice->pUserData;

    // 4. Перебираем каждый диапазон и обрабатываем его (как в твоем loop())
    for (auto &band : *bands)
    {
        if (band.buffer.size() < frameCount)
        {
            band.buffer.resize(frameCount);
        }

        // Пропускаем звук через нужный фильтр
        if (band.type == FILTER_LPF)
        {
            ma_lpf_process_pcm_frames(&band.lpf, band.buffer.data(), pInput, frameCount);
        }
        else if (band.type == FILTER_BPF)
        {
            ma_bpf_process_pcm_frames(&band.bpf, band.buffer.data(), pInput, frameCount);
        }
        else if (band.type == FILTER_HPF)
        {
            ma_hpf_process_pcm_frames(&band.hpf, band.buffer.data(), pInput, frameCount);
        }

        // Ищем пиковую громкость
        float maxAmp = 0;
        for (ma_uint32 i = 0; i < frameCount; i++)
        {
            if (fabsf(band.buffer[i]) > maxAmp)
                maxAmp = fabsf(band.buffer[i]);
        }

        // Вычисляем значение и применяем множитель
        band.currentVal = (uint8_t)(fminf(maxAmp * band.multiplier, 1.0f) * 255.0f);
    }

    // Отправляем все значения в порт
    sendPacket(*bands);

    // Динамический вывод в консоль
    std::cout << "\r";
    for (size_t i = 0; i < bands->size(); ++i)
    {
        std::cout << " | CH" << (i + 1) << ": " << std::setw(3) << (int)(*bands)[i].currentVal;
    }
    std::cout << "    " << std::flush;
}

int main()
{
    std::cout << "--- Multi-Band Audio Visualizer ---" << std::endl;

    if (!initSerial("\\\\.\\COM3"))
    {
        std::cerr << "Error: Could not open Arduino port." << std::endl;
        return -1;
    }

    Sleep(2000);

    ma_device_config config = ma_device_config_init(ma_device_type_loopback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 1;
    config.sampleRate = 44100;
    config.dataCallback = data_callback;

    // 5. НАСТРОЙКА КАНАЛОВ (Здесь ты можешь добавлять сколько угодно диапазонов!)
    std::vector<BandData> bands = {
        // 1. Глубокий бас (Sub-bass). Удары бочки и низкий гул.
        {FILTER_LPF, 100.0, 1.0f},

        // 2. Мид-бас (Mid-bass). Бас-гитара и "тело" ударных.
        {FILTER_BPF, 300.0, 1.0f},

        // 3. Нижняя середина (Low-mids). Основной тон мужского вокала.
        {FILTER_BPF, 1000.0, 1.0f},

        // 4. Верхняя середина (High-mids). Женский вокал, гитарное соло.
        {FILTER_BPF, 2500.0, 1.0f},

        // 5. Презенс (Presence). Четкость инструментов, перкуссия.
        {FILTER_BPF, 5000.0, 1.0f},

        // 6. Высокие частоты (Brilliance/Highs). Тарелки, "воздух", звон.
        {FILTER_HPF, 10000.0, 1.0f}};

    // Автоматическая инициализация всех фильтров в массиве
    for (auto &band : bands)
    {
        if (band.type == FILTER_LPF)
        {
            ma_lpf_config cfg = ma_lpf_config_init(config.playback.format, config.playback.channels, config.sampleRate, band.freq, 2);
            ma_lpf_init(&cfg, NULL, &band.lpf);
        }
        else if (band.type == FILTER_BPF)
        {
            ma_bpf_config cfg = ma_bpf_config_init(config.playback.format, config.playback.channels, config.sampleRate, band.freq, 2);
            ma_bpf_init(&cfg, NULL, &band.bpf);
        }
        else if (band.type == FILTER_HPF)
        {
            ma_hpf_config cfg = ma_hpf_config_init(config.playback.format, config.playback.channels, config.sampleRate, band.freq, 2);
            ma_hpf_init(&cfg, NULL, &band.hpf);
        }
    }

    // Передаем ссылку на массив в callback-функцию
    config.pUserData = &bands;

    ma_device device;
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS)
    {
        CloseHandle(hSerial);
        return -1;
    }

    ma_device_start(&device);
    std::cout << "Streaming bands to Arduino... Press Enter to stop." << std::endl;
    std::cin.get();

    ma_device_uninit(&device);
    CloseHandle(hSerial);
    return 0;
}