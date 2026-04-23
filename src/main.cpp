#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.println("xh-s3e-ai boot");
    Serial.printf("PSRAM size: %u bytes\n", (unsigned) ESP.getPsramSize());
    Serial.printf("Free heap:  %u bytes\n", (unsigned) ESP.getFreeHeap());
}

void loop() {
    delay(1000);
}
