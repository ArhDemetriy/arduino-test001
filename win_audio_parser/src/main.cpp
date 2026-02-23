#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <iostream>
#include <vector>
#include <windows.h>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <algorithm>
#include "shared_protocol.h"
extern "C"
{
#include "kiss_fftr.h"
}

#define FFT_SIZE 4096 // Размер окна (степень двойки)
// #define FFT_SIZE 2048 // Размер окна (степень двойки)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

struct BandData
{
    float freqMin;
    float freqMax;
    float multiplier; // Теперь работает как коэффициент чувствительности в дБ
    uint8_t currentVal = 0;
};

struct AudioDSP
{
    std::vector<BandData> bands;
    kiss_fftr_cfg fftConfig;
    float fftInput[FFT_SIZE];
    kiss_fft_cpx fftOutput[FFT_SIZE / 2 + 1];
    int sampleCounter = 0;
    float sampleRate = 44100.0f;

    AudioDSP()
    {
        fftConfig = kiss_fftr_alloc(FFT_SIZE, 0, NULL, NULL);
        memset(fftInput, 0, sizeof(fftInput));
    }

    ~AudioDSP()
    {
        kiss_fftr_free(fftConfig);
    }
};

void sendPacket(const std::vector<BandData> &bands)
{
    std::vector<uint8_t> packet;
    packet.push_back(0xFE);
    for (const auto &band : bands)
    {
        packet.push_back(band.currentVal);
    }
    DWORD written;
    WriteFile(hSerial, packet.data(), (DWORD)packet.size(), &written, NULL);
}

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    AudioDSP *dsp = (AudioDSP *)pDevice->pUserData;
    float *pIn = (float *)pInput;

    if (pIn == NULL)
        return;

    for (ma_uint32 i = 0; i < frameCount; i++)
    {
        dsp->fftInput[dsp->sampleCounter] = pIn[i * pDevice->capture.channels];
        dsp->sampleCounter++;

        if (dsp->sampleCounter >= FFT_SIZE)
        {
            // 1. Окно Ханна (убирает шумы на соседних каналах)
            for (int j = 0; j < FFT_SIZE; j++)
            {
                float window = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * j / (FFT_SIZE - 1)));
                dsp->fftInput[j] *= window;
            }

            kiss_fftr(dsp->fftConfig, dsp->fftInput, dsp->fftOutput);

            std::vector<float> bandMax(dsp->bands.size(), 0.0f);

            for (int bin = 0; bin <= FFT_SIZE / 2; bin++)
            {
                float freq = bin * (dsp->sampleRate / (float)FFT_SIZE);
                float r = dsp->fftOutput[bin].r;
                float im = dsp->fftOutput[bin].i;

                // Амплитуда (нормализованная)
                float magnitude = sqrtf(r * r + im * im) / (FFT_SIZE / 2.0f);

                for (size_t b = 0; b < dsp->bands.size(); b++)
                {
                    if (freq >= dsp->bands[b].freqMin && freq < dsp->bands[b].freqMax)
                    {
                        if (magnitude > bandMax[b])
                            bandMax[b] = magnitude;
                    }
                }
            }

            // 2. Логарифмическая обработка (дБ)
            for (size_t b = 0; b < dsp->bands.size(); b++)
            {
                // Перевод в децибелы (magnitude 1.0 = 0dB, 0.01 = -40dB)
                float db = 20.0f * log10f(bandMax[b] + 1e-6f);

                // Настройки диапазона:
                // minDb - уровень полной темноты (шум покоя)
                // maxDb - уровень максимальной яркости (пик)
                float minDb = -50.0f;
                float maxDb = 0.0f;
                // float maxDb = -15.0f / dsp->bands[b].multiplier;

                // Линейная интерполяция дБ в диапазон 0..1
                float normalized = (db - minDb) / (maxDb - minDb);

                if (normalized < 0.0f)
                    normalized = 0.0f;
                if (normalized > 1.0f)
                    normalized = 1.0f;

                dsp->bands[b].currentVal = (uint8_t)(normalized * 255.0f);
            }

            const bool hasSignal = std::any_of(dsp->bands.begin(), dsp->bands.end(), [](const auto &item)
                                               { return item.currentVal > 0; });

            if (hasSignal)
                sendPacket(dsp->bands);

            std::cout << "\r";
            for (size_t b = 0; b < dsp->bands.size(); ++b)
            {
                std::cout << " | CH" << (b + 1) << ": " << std::setw(3) << (int)dsp->bands[b].currentVal;
            }
            std::cout << "    " << std::flush;

            dsp->sampleCounter = 0;
        }
    }
}

int main()
{
    std::cout << "--- Multi-Band FFT Visualizer (Logarithmic) ---" << std::endl;

    if (!initSerial("\\\\.\\COM3"))
    {
        std::cerr << "Error: Could not open Arduino port." << std::endl;
    }

    Sleep(2000);
    AudioDSP dsp;

    dsp.bands = {
        {0.0f, 150.0f, 1.0f},
        {150.0f, 400.0f, 1.0f},
        {400.0f, 1500.0f, 1.0f},
        {1500.0f, 4000.0f, 1.0f},
        {4000.0f, 8000.0f, 1.0f},
        {8000.0f, 22000.0f, 1.0f}};

    ma_device_config config = ma_device_config_init(ma_device_type_loopback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 1;
    config.sampleRate = (ma_uint32)dsp.sampleRate;
    config.dataCallback = data_callback;
    config.pUserData = &dsp;

    ma_device device;
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS)
    {
        CloseHandle(hSerial);
        return -1;
    }

    ma_device_start(&device);
    std::cout << "\nStreaming FFT bands to Arduino... Press Enter to stop." << std::endl;
    std::cin.get();

    ma_device_uninit(&device);
    CloseHandle(hSerial);
    return 0;
}