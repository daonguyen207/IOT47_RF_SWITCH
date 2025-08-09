#include "IOT47_RF_decoder.h"

#define RF_PIN 10

void setup()
{
	Serial.begin(115200);
	rf_gpio_init(RF_PIN);
}
void loop()
{
	if (rf_available())
    {
        uint32_t v = rf_read();
        Serial.printf("Frame 24-bit: 0x%06X\n", v & 0xFFFFFF);
        rf_print_bits24(v);
    }
    delay(1);
}