#include <Arduino.h>
#include "DHT.h"

// HC-SR04
#define TRIG_PIN 5
#define ECHO_PIN 18

// LEDs y buzzer
#define LED_ROJO 23
#define LED_VERDE 22
#define BUZZER 21

// DHT11 (cambiar a DHT22 cuando lo tenga)
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Variables de distancia y movimiento
long duracion;
int distancia;
int distanciaAnterior = 0;
int contadorQuieto = 0;

// Parámetros de lógica
const int UMBRAL_MOV = 4;       // variación mínima en cm para detectar movimiento
const int CICLOS_QUIETO = 5;    // lecturas consecutivas quieto = estacionado
bool alertaMostrada = false;    // para no repetir la alerta de ocupado

// Variables del DHT
float temp = NAN;
float hum = NAN;
unsigned long ultimoDHT = 0;
const unsigned long intervaloDHT = 2000; // leer cada 2 segundos

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  dht.begin();
}

void loop() {
  // --- Lectura del ultrasonico ---
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duracion = pulseIn(ECHO_PIN, HIGH, 30000);
  distancia = duracion * 0.034 / 2;

  // --- Detección de movimiento ---
  if (abs(distancia - distanciaAnterior) <= UMBRAL_MOV) {
    contadorQuieto++;
  } else {
    contadorQuieto = 0;
    alertaMostrada = false; // resetear si el coche vuelve a moverse
  }
  distanciaAnterior = distancia;

  // --- Lectura del DHT cada 2s ---
  if (millis() - ultimoDHT > intervaloDHT) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      temp = t;
      hum = h;
    }
    ultimoDHT = millis();
  }

  // --- Determinar estado ---
  String estado = "";

  if (distancia > 100 || distancia <= 0) {
    // Estado: Libre (incluye fuera de rango >170 cm)
    estado = "Libre";
    digitalWrite(LED_ROJO, LOW);
    digitalWrite(LED_VERDE, HIGH);
    noTone(BUZZER);
    delay(200);
    digitalWrite(LED_VERDE, LOW);

  } else if (distancia > 30 && distancia <= 100) {
    // Estado: Aproximación
    estado = "Aproximación";
    digitalWrite(LED_ROJO, HIGH);
    digitalWrite(LED_VERDE, LOW);

    int frecuencia = map(distancia, 100, 30, 800, 1200);
    int duracionBeep = map(distancia, 100, 30, 400, 150);

    tone(BUZZER, frecuencia);
    delay(duracionBeep);
    noTone(BUZZER);
    delay(duracionBeep);

  } else if (distancia <= 30) {
    // Estado dentro de rango de estacionado
    if (contadorQuieto >= CICLOS_QUIETO) {
      // Ocupado (carro ya detenido)
      estado = "Ocupado";
      noTone(BUZZER);
      digitalWrite(LED_VERDE, LOW);

      if (!alertaMostrada) {
        // Alerta con parpadeo rojo solo 1 vez
        for (int i = 0; i < 5; i++) {
          digitalWrite(LED_ROJO, HIGH);
          delay(300);
          digitalWrite(LED_ROJO, LOW);
          delay(300);
        }
        alertaMostrada = true;
      }

      // Todo apagado después de la alerta
      digitalWrite(LED_ROJO, LOW);
      digitalWrite(LED_VERDE, LOW);

    } else {
      // Maniobra (aún se mueve dentro de ≤30 cm)
      estado = "Maniobra";
      digitalWrite(LED_ROJO, HIGH);
      tone(BUZZER, 1600);
      delay(150);
      noTone(BUZZER);
      delay(150);
    }
  }

  // --- Mostrar datos en Serial ---
  Serial.print("Distancia: ");
  Serial.print(distancia);
  Serial.print(" cm | Estado: ");
  Serial.print(estado);

  if (!isnan(temp) && !isnan(hum)) {
    Serial.print(" | Temp: ");
    Serial.print(temp);
    Serial.print(" °C | Hum: ");
    Serial.print(hum);
    Serial.print(" %");
  }
  Serial.println();

  delay(200);
}
