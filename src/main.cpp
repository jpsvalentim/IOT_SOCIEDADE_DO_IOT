#include <Arduino.h>

#define LED_PIN 2     // LED onboard no ESP32 geralmente é o GPIO 2
#define BUTTON_PIN 14 // Botão (ligado entre GPIO 14 e GND)
#define ADC_PIN 34    // Entrada analógica (GPIO 34)

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  analogReadResolution(12); // resolução de 12 bits: 0 a 4095
}

void loop() {
  // Pisca LED
  Serial.println("Pisca LED...");
  digitalWrite(LED_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
  delay(500);

  // Lê botão
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("Botão pressionado!");
    delay(500);
  }

  // Lê ADC
  int adcVal = analogRead(ADC_PIN);
  Serial.print("Valor ADC (GPIO34): ");
  Serial.println(adcVal);

  delay(1000);
}
