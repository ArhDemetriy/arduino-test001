#include <Arduino.h>

const int fadeInterval = 10;
decltype(millis()) nextFadeTime = 0;

void setup()
{
    Serial.begin(115200);
    nextFadeTime = millis() + fadeInterval;
}

// Описываем тип данных (как интерфейс в TS)
struct PinData
{
    int key;
    uint8_t val;
    decltype(millis()) nextFadeTime;
};
PinData pinMap[] = {
    {3, 0, nextFadeTime},
    {5, 0, nextFadeTime},
    {6, 0, nextFadeTime}};

void loop()
{
    // Timer
    const auto currentTime = millis();
    if (currentTime >= nextFadeTime)
    {
        const auto requiredNextFadeTime = currentTime + fadeInterval;
        auto tempNextFadeTime = pinMap[0].nextFadeTime;

        for (auto &item : pinMap)
        {
            if (item.nextFadeTime > currentTime)
            {
                tempNextFadeTime = min(tempNextFadeTime, item.nextFadeTime);
                continue;
            }

            item.val >>= 1;
            item.nextFadeTime = requiredNextFadeTime;
            analogWrite(item.key, item.val);
        }

        nextFadeTime = min(requiredNextFadeTime, tempNextFadeTime);
    }

    // Input

    if (Serial.available() < 4)
        return;
    if (Serial.read() != 0xFE)
        return;

    for (auto &item : pinMap)
    {
        const uint8_t temp_val = Serial.read();
        if (item.val >= temp_val)
            continue;

        item.val = temp_val;
        item.nextFadeTime = currentTime + fadeInterval;
        analogWrite(item.key, item.val);
    }
}