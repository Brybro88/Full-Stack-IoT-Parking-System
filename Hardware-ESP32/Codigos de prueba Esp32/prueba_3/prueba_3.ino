#include <Arduino.h>
#include "DHT.h"

// Pines HC-SR04
#define TRIG_PIN 5
#define ECHO_PIN 18

// LEDs y buzzer
#define LED_ROJO 23
#define LED_VERDE 22
#define BUZZER 21

// DHT22
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// KY-037 (pendiente)
// #define KY037 34

long duracion;
int distancia;

void setup() {
  Serial.begin(115200);

  // Pines
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  // DHT
  dht.begin();
}

void loop() {
  // --- Distancia ---
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duracion = pulseIn(ECHO_PIN, HIGH);
  distancia = duracion * 0.034 / 2;

  // --- Lecturas ambientales ---
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  // float ruido = analogRead(KY037); // (pendiente cuando llegue el sensor)

  // --- Serial Monitor ---
  Serial.print("Distancia: ");
  Serial.print(distancia);
  Serial.println(" cm");

  if (!isnan(temp) && !isnan(hum)) {
    Serial.print("Temperatura: ");
    Serial.print(temp);
    Serial.print(" °C | Humedad: ");
    Serial.print(hum);
    Serial.println(" %");
  } else {
    Serial.println("Error en lectura del DHT22");
  }

  // Serial.print("Ruido: ");
  // Serial.println(ruido);

  // --- Lógica de estacionamiento ---
  if (distancia < 30) {
    Serial.println("Estado: Ocupado");
    digitalWrite(LED_ROJO, HIGH);
    digitalWrite(LED_VERDE, LOW);
    tone(BUZZER, 1000, 100);
    delay(100);

  } else if (distancia > 100) {
    Serial.println("Estado: Libre");
    digitalWrite(LED_ROJO, LOW);
    digitalWrite(LED_VERDE, HIGH);
    noTone(BUZZER);
    delay(3000);
    digitalWrite(LED_VERDE, LOW);

  } else {
    Serial.println("Estado: En proceso");
    digitalWrite(LED_ROJO, HIGH);
    digitalWrite(LED_VERDE, LOW);
    tone(BUZZER, 1000, 200);
    delay(400);
  }

  delay(1000); // Intervalo de actualización
}
