// Proyecto: Sistema de Monitoreo de Estacionamiento
// ESP32+HC-SR04+DHT11+LEDs+Buzzer+(KY-037 comentado)

#include <NewPing.h>
#include <DHT.h>

// ==== Pines ====
// Ultrasonido HC-SR04
#define TRIG_PIN 5
#define ECHO_PIN 18
#define MAX_DISTANCE 200 // cm
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);

// DHT11
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// LEDs
#define LED_ROJO 23
#define LED_VERDE 22

// Buzzer activo
#define BUZZER_PIN 21

// KY-037 (comentado hasta que llegue)
//#define KY037_PIN 34   // Entrada analógica
//int micValue = 0;

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(LED_ROJO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_ROJO, LOW);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("=== Prueba con buzzer + código KY-037 preparado ===");
}

void loop() {
  // ==== Lectura ultrasonido ====
  unsigned int dist = sonar.ping_cm();
  if (dist == 0) dist = 999; // fuera de rango

  // ==== Lectura DHT11 ====
  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();

  // ==== LEDs ====
  if (dist <= 30) {                 // Ocupado
    digitalWrite(LED_ROJO, HIGH);
    digitalWrite(LED_VERDE, LOW);
  } else if (dist >= 100 && dist < 999) { // Libre
    digitalWrite(LED_ROJO, LOW);
    digitalWrite(LED_VERDE, HIGH);
  } else {                         // Intermedio
    digitalWrite(LED_ROJO, LOW);
    digitalWrite(LED_VERDE, LOW);
  }

  // ==== Buzzer activo (alerta sonora) ====
  if (dist < 100 && dist > 0) {
    // A menor distancia, mayor frecuencia de pitidos
    int delayMs = map(dist, 1, 100, 5, 50); // de 50 ms (muy cerca) a 500 ms (100 cm)
    digitalWrite(BUZZER_PIN, HIGH);
    delay(50); // pitido corto fijo
    digitalWrite(BUZZER_PIN, LOW);
    delay(delayMs);
  } else {
    digitalWrite(BUZZER_PIN, LOW); // no sonar si libre
    delay(500);
  }

  // ==== Sensor de ruido KY-037 (comentado) ====
  /*
  micValue = analogRead(KY037_PIN);
  Serial.print("Mic: ");
  Serial.println(micValue);
  */

  // ==== Mostrar en Serial ====
  Serial.print("Distancia: ");
  Serial.print(dist);
  Serial.print(" cm | Temp: ");
  Serial.print(isnan(temp) ? -999 : temp);
  Serial.print(" C | Hum: ");
  Serial.print(isnan(hum) ? -999 : hum);
  Serial.println(" %");
}
