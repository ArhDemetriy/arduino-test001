#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <iostream>
#include <vector>
#include <windows.h>

HANDLE hSerial;

bool initSerial(const char *portName)
{
    hSerial = CreateFileA(portName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSerial == INVALID_HANDLE_VALUE)
        return false;

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    GetCommState(hSerial, &dcbSerialParams);
    dcbSerialParams.BaudRate = CBR_115200; // Должно совпадать с Arduino
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    return SetCommState(hSerial, &dcbSerialParams);
}

void sendByte(unsigned char b)
{
    DWORD written;
    WriteFile(hSerial, &b, 1, &written, NULL);
}

bool isTriggered = false;
void data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    float *pSamples = (float *)pInput;
    float maxAmp = 0;
    for (ma_uint32 i = 0; i < frameCount; i++)
    {
        if (fabsf(pSamples[i]) > maxAmp)
            maxAmp = fabsf(pSamples[i]);
    }

    if (maxAmp > 0.5f && !isTriggered)
    {
        // Условие: громко И триггер еще не взведен
        sendByte(0xFF);
        isTriggered = true;
        std::cout << "[*] TRIGGER! " << maxAmp << std::endl;
    }
    else if (maxAmp <= 0.5f && isTriggered)
    {
        // Условие: тихо И триггер был взведен
        sendByte(0x00);
        isTriggered = false;

        // Вывод шкалы
        int barWidth = (int)(maxAmp * 50);
        std::cout << "[ ";
        for (int i = 0; i < 50; ++i)
        {
            std::cout << (i < barWidth ? "=" : " ");
        }
        std::cout << " ] " << maxAmp << "\r" << std::flush;
    }
}

int main()
{
    std::cout << "--- Arduino Music Sync: PC Side Hello World ---" << std::endl;

    // 1. Сначала инициализируем порт.
    // Если Arduino на COM3, указываем путь для Windows API:
    if (!initSerial("\\\\.\\COM3"))
    {
        std::cerr << "Error: Could not open Arduino port. Check COM number!" << std::endl;
        return -1;
    }

    // Даем Arduino 2 секунды на "проснуться" после авто-сброса при открытии порта
    Sleep(2000);

    ma_device_config config = ma_device_config_init(ma_device_type_loopback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 1;
    config.sampleRate = 44100;
    config.dataCallback = data_callback;

    ma_device device;
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS)
    {
        CloseHandle(hSerial);
        std::cerr << "Error: Could not initialize audio device." << std::endl;
        return -1;
    }

    if (ma_device_start(&device) != MA_SUCCESS)
    {
        std::cerr << "Error: Could not start device." << std::endl;
        ma_device_uninit(&device);
        CloseHandle(hSerial);
        return -1;
    }

    std::cout << "Listening to your system audio... Play something in foobar2000!" << std::endl;
    std::cout << "Press Enter to stop." << std::endl;

    std::cin.get();

    ma_device_uninit(&device);
    CloseHandle(hSerial);
    return 0;
}