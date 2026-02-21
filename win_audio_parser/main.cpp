#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <iostream>
#include <vector>
#include <windows.h>

HANDLE hSerial;

bool initSerial(const char* portName) {
    hSerial = CreateFileA(portName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) return false;

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    GetCommState(hSerial, &dcbSerialParams);
    dcbSerialParams.BaudRate = CBR_115200; // Должно совпадать с Arduino
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    return SetCommState(hSerial, &dcbSerialParams);
}

void sendByte(unsigned char b) {
    DWORD written;
    WriteFile(hSerial, &b, 1, &written, NULL);
}

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    // В режиме loopback pInput содержит данные из системы
    float* pSamples = (float*)pInput;
    if (pSamples == NULL) return;

    float maxAmp = 0;
    for (ma_uint32 i = 0; i < frameCount; i++) {
        if (fabsf(pSamples[i]) > maxAmp) maxAmp = fabsf(pSamples[i]);
    }

    // Если амплитуда выше порога, выводим "биение" в консоль
    if (maxAmp > 0.1f) {
        std::cout << "[HELLO WORLD] Sound detected! Level: " << maxAmp << std::endl;
    }
}
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    float* pSamples = (float*)pInput;
    float maxAmp = 0;
    for (ma_uint32 i = 0; i < frameCount; i++) {
        if (fabsf(pSamples[i]) > maxAmp) maxAmp = fabsf(pSamples[i]);
    }

    if (maxAmp > 0.5f) { // Ваш порог
        sendByte(0xFF); // Шлем "вспышку"
    } else {
        sendByte(0x00); // Гасим
    }
}

int main() {
    std::cout << "--- Arduino Music Sync: PC Side Hello World ---" << std::endl;

    ma_device_config config = ma_device_config_init(ma_device_type_loopback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 1;
    config.sampleRate        = 44100;
    config.dataCallback      = data_callback;

    ma_device device;
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
        std::cerr << "Error: Could not initialize audio device." << std::endl;
        return -1;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        std::cerr << "Error: Could not start device." << std::endl;
        ma_device_uninit(&device);
        return -1;
    }

    std::cout << "Listening to your system audio... Play something in foobar2000!" << std::endl;
    std::cout << "Press Enter to stop." << std::endl;

    std::cin.get();

    ma_device_uninit(&device);
    return 0;
}