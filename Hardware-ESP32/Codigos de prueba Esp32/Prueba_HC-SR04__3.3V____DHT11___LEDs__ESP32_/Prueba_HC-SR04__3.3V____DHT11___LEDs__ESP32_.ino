// Prueba: HC-SR04 (3.3V) + DHT11 + LEDs (ESP32)
// Pines según tu layout:
// TRIG -> GPIO 5
// ECHO -> GPIO 18
// DHT11 DATA -> GPIO 4
// LED ROJO -> GPIO 21
// LED VERDE -> GPIO 22

#include <NewPing.h>
#include <DHT.h>

// ====== Pines ======
#define TRIG_PIN 5
#define ECHO_PIN 18
#define MAX_DISTANCE 200  // cm

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define LED_ROJO 21
#define LED_VERDE 22

NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(LED_ROJO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);

  digitalWrite(LED_ROJO, LOW);
  digitalWrite(LED_VERDE, LOW);

  Serial.println("=== Prueba HC-SR04 (3.3V) + DHT11 + LEDs ===");
}

void loop() {
  // Leer distancia (NewPing devuelve 0 si no detecta)
  unsigned int dist = sonar.ping_cm();
  if (dist == 0) dist = 999; // indicador de fuera de rango

  // Leer DHT11 (puede tardar ~1s entre lecturas correctas)
  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();

  // Lógica de LEDs:
  // Ocupado < 30 cm -> LED rojo ON
  // Libre >= 100 cm -> LED verde ON
  // Entre 30 y 100 -> ambos OFF
  if (dist < 30) {
    digitalWrite(LED_ROJO, HIGH);
    digitalWrite(LED_VERDE, LOW);
  } else if (dist >= 100 && dist < 999) {
    digitalWrite(LED_ROJO, LOW);
    digitalWrite(LED_VERDE, HIGH);
  } else {
    digitalWrite(LED_ROJO, LOW);
    digitalWrite(LED_VERDE, LOW);
  }

  // Imprimir por Serial
  Serial.print("Distancia: ");
  if (dist == 999) Serial.print("FueraRango");
  else Serial.print(dist);
  Serial.print(" cm | Temp: ");
  if (isnan(temp)) Serial.print("ERR");
  else Serial.print(temp);
  Serial.print(" C | Hum: ");
  if (isnan(hum)) Serial.print("ERR");
  else Serial.print(hum);
  Serial.println(" %");

  delay(2000); // espera entre lecturas
}
