#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <iostream>
#include <vector>
#include <windows.h>
#include <cmath>
#include <cstdint>
#include <iomanip>

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

void sendPacket(uint8_t bass, uint8_t mid, uint8_t high)
{
    uint8_t packet[] = {0xFE, bass, mid, high};
    DWORD written;
    WriteFile(hSerial, packet, sizeof(packet), &written, NULL);
}

// Структура для хранения фильтров и буферов
struct AudioDSP
{
    ma_lpf lpf; // Фильтр низких (Бас)
    ma_bpf bpf; // Фильтр средних (СЧ)
    ma_hpf hpf; // Фильтр высоких (ВЧ)

    // Временные массивы для отфильтрованного звука
    std::vector<float> bufL;
    std::vector<float> bufM;
    std::vector<float> bufH;
};

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    // Получаем наш DSP объект
    AudioDSP *dsp = (AudioDSP *)pDevice->pUserData;

    // Увеличиваем размер буферов, если miniaudio прислал больше кадров, чем обычно
    if (dsp->bufL.size() < frameCount)
    {
        dsp->bufL.resize(frameCount);
        dsp->bufM.resize(frameCount);
        dsp->bufH.resize(frameCount);
    }

    // Пропускаем оригинальный звук через 3 разных фильтра
    ma_lpf_process_pcm_frames(&dsp->lpf, dsp->bufL.data(), pInput, frameCount);
    ma_bpf_process_pcm_frames(&dsp->bpf, dsp->bufM.data(), pInput, frameCount);
    ma_hpf_process_pcm_frames(&dsp->hpf, dsp->bufH.data(), pInput, frameCount);

    float maxL = 0, maxM = 0, maxH = 0;

    // Ищем максимальную амплитуду в каждой частотной полосе
    for (ma_uint32 i = 0; i < frameCount; i++)
    {
        if (fabsf(dsp->bufL[i]) > maxL)
            maxL = fabsf(dsp->bufL[i]);
        if (fabsf(dsp->bufM[i]) > maxM)
            maxM = fabsf(dsp->bufM[i]);
        if (fabsf(dsp->bufH[i]) > maxH)
            maxH = fabsf(dsp->bufH[i]);
    }

    // Переводим в байты (0-255).
    // Mids и Highs мы умножаем (x1.5 и x2.0), так как в музыке басы физически громче,
    // и без усиления средние и высокие диоды будут светить тускло.
    uint8_t valBass = (uint8_t)(fminf(maxL * 1.0f, 1.0f) * 255.0f);
    uint8_t valMid = (uint8_t)(fminf(maxM * 1.5f, 1.0f) * 255.0f);
    uint8_t valHigh = (uint8_t)(fminf(maxH * 2.0f, 1.0f) * 255.0f);

    sendPacket(valBass, valMid, valHigh);

    // Вывод в консоль для дебага
    std::cout
        << " | High: " << std::setw(3) << (int)valHigh
        << " | Mid: " << std::setw(3) << (int)valMid
        << " | Bass: " << std::setw(3) << (int)valBass
        << "    \r" << std::flush;

    // std::cout
    //     << "\x1b[3A" // Поднимаемся на 3 строки вверх
    //     << "High: " << std::setw(3) << (int)valHigh << "    \x1b[K"
    //     << "Mid:  " << std::setw(3) << (int)valMid << "    \x1b[K\n"
    //     << "Bass: " << std::setw(3) << (int)valBass << "    \x1b[K\n"
    //     << std::flush;
}

int main()
{
    std::cout << "--- 3-Band Audio Visualizer ---" << std::endl;

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

    // Инициализация фильтров
    AudioDSP dsp;

    // 1. Конфиг для НЧ (Бас) - порядок 2
    ma_lpf_config lpfCfg = ma_lpf_config_init(config.playback.format, config.playback.channels, config.sampleRate, 200.0, 2);
    if (ma_lpf_init(&lpfCfg, NULL, &dsp.lpf) != MA_SUCCESS)
    {
        std::cerr << "Failed to init LPF" << std::endl;
        return -1;
    }

    // 2. Конфиг для СЧ - порядок 2 (обязательно четный для BPF)
    ma_bpf_config bpfCfg = ma_bpf_config_init(config.playback.format, config.playback.channels, config.sampleRate, 1500.0, 2);
    if (ma_bpf_init(&bpfCfg, NULL, &dsp.bpf) != MA_SUCCESS)
    {
        std::cerr << "Failed to init BPF" << std::endl;
        return -1;
    }

    // 3. Конфиг для ВЧ - порядок 2
    ma_hpf_config hpfCfg = ma_hpf_config_init(config.playback.format, config.playback.channels, config.sampleRate, 5000.0, 2);
    if (ma_hpf_init(&hpfCfg, NULL, &dsp.hpf) != MA_SUCCESS)
    {
        std::cerr << "Failed to init HPF" << std::endl;
        return -1;
    }

    // Передаем нашу структуру с фильтрами в коллбэк
    config.pUserData = &dsp;

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