#include <usb_rename.h>
#include <MIDIUSB.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

USBRename usbRename = USBRename("X.U.L", "NeoArchCatDev", "0001");

#define NUM_FADERS 3
#define RAW_THRESHOLD 4      // Threshold for raw value change
#define SMOOTHING_FACTOR 0.2 // Smoothing factor for raw values

const uint8_t faderPins[NUM_FADERS] = {A10, A9, A8};
uint8_t ccNumbers[NUM_FADERS] = {1, 2, 3};
uint8_t lastValues[NUM_FADERS] = {0};
float smoothedRawValues[NUM_FADERS] = {0}; // Smoothed raw values

void sendMIDI(uint8_t channel, uint8_t control, uint8_t value);
void receiveCCValuesFromWebsite();
void saveCCValuesToEEPROM();
void loadCCValuesFromEEPROM();

void setup()
{
    loadCCValuesFromEEPROM();

    for (int i = 0; i < NUM_FADERS; i++)
    {
        pinMode(faderPins[i], INPUT);
        smoothedRawValues[i] = analogRead(faderPins[i]);
    }
}

void loop()
{
    receiveCCValuesFromWebsite();

    for (int i = 0; i < NUM_FADERS; i++)
    {
        // Read and smooth the raw analog value
        uint16_t rawValue = analogRead(faderPins[i]);
        smoothedRawValues[i] = (SMOOTHING_FACTOR * rawValue) + ((1 - SMOOTHING_FACTOR) * smoothedRawValues[i]);

        // Map smoothed raw values to MIDI values
        uint8_t midiValue = (uint16_t)smoothedRawValues[i] / 8;

        // Send MIDI only if the MIDI value has changed
        if (midiValue != lastValues[i])
        {
            sendMIDI(0, ccNumbers[i], midiValue);
            lastValues[i] = midiValue;
        }
    }
}

void sendMIDI(uint8_t channel, uint8_t control, uint8_t value)
{
    midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
    MidiUSB.sendMIDI(event);
    MidiUSB.flush();
}

void receiveCCValuesFromWebsite()
{
    if (Serial.available() > 0)
    {
        StaticJsonDocument<200> doc;
        String jsonString = Serial.readStringUntil('\n');
        DeserializationError error = deserializeJson(doc, jsonString);

        if (error)
        {
            return;
        }

        for (int i = 0; i < NUM_FADERS; i++)
        {
            if (doc["cc"][i].is<int>())
            {
                uint8_t newCC = doc["cc"][i];
                if (ccNumbers[i] != newCC) // Only update if the value has changed
                {
                    ccNumbers[i] = newCC;
                    saveCCValuesToEEPROM(); // Save updated CC values to EEPROM
                }
            }
        }
    }
}

void saveCCValuesToEEPROM()
{
    for (int i = 0; i < NUM_FADERS; i++)
    {
        EEPROM.update(i, ccNumbers[i]);
    }
}

void loadCCValuesFromEEPROM()
{
    for (int i = 0; i < NUM_FADERS; i++)
    {
        uint8_t value = EEPROM.read(i);

        if (value >= 1 && value <= 127)
        {
            ccNumbers[i] = value;
        }
        else
        {
            ccNumbers[i] = i + 1; // Default to {1, 2, 3} if invalid
        }
    }
}
