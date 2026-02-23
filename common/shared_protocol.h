// common/shared_protocol.h
#pragma once
#include <stdint.h>

// Общие константы
const uint16_t SYNC_WORD = 0xA55A;
const int BAUD_RATE = 115200;

// Отключаем выравнивание памяти для совместимости ПК и Arduino
#pragma pack(push, 1)

struct AudioDataPacket {
    uint16_t sync;       // 2 байта (для проверки начала пакета)
    uint8_t command;     // 1 байт
    float frequency;     // 4 байта
    int16_t amplitude;   // 2 байта
};

#pragma pack(pop) // Возвращаем стандартное выравнивание