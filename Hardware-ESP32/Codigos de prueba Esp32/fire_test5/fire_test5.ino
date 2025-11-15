#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <DHT.h>
#include <Adafruit_Sensor.h>

// ======= CONFIG WiFi =======
#define WIFI_SSID "IBARRA 25"
#define WIFI_PASSWORD "COSMO3625"

// ======= CONFIG Firebase =======
#define API_KEY "AIzaSyAe_GED12ttFaO68TkFBAq45RQMC9bB96Q"
#define DATABASE_URL "https://parking-test-14cef-default-rtdb.firebaseio.com/"

// ======= DHT11 =======
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ======= HC-SR04 =======
#define TRIG_PIN 12
#define ECHO_PIN 14

// ======= KY-037 (Ruido) =======
#define SOUND_PIN 34  // entrada analógica

// ======= LEDs y BUZZER =======
#define LED_VERDE 26
#define LED_ROJO 27
#define BUZZER 25

// ======= Firebase =======
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  dht.begin();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n✅ Conectado!");

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  Firebase.signUp(&config, &auth, "", "");
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("Inicio de sesión anónimo exitoso!");
}

void loop() {
  // ===== Lectura DHT11 =====
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  // ===== Lectura HC-SR04 =====
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  float distancia = duration * 0.034 / 2;

  // ===== Lectura KY-037 =====
  int ruido = analogRead(SOUND_PIN);

  // ===== Lógica de ocupación =====
  String estado = (distancia < 10) ? "Ocupado" : "Libre";

  Serial.print("📏 Distancia filtrada: "); Serial.println(distancia);
  Serial.print("🌡️ "); Serial.print(temp); Serial.print(" °C | 💧 ");
  Serial.print(hum); Serial.print(" % | 🎤 Ruido: "); Serial.print(ruido);
  Serial.print(" | Estado: "); Serial.println(estado);

  // ===== Lógica de LEDs y buzzer =====
  if (estado == "Ocupado") {
    digitalWrite(LED_ROJO, HIGH);
    digitalWrite(LED_VERDE, LOW);
    tone(BUZZER, 2000, 300);
  } else {
    digitalWrite(LED_ROJO, LOW);
    digitalWrite(LED_VERDE, HIGH);
    noTone(BUZZER);
  }

  // ===== Envío a Firebase =====
  Serial.println("📡 Enviando datos a Firebase...");

  bool ok = true;
  if (!isnan(temp) && !isnan(hum)) {
    ok &= Firebase.RTDB.setFloat(&fbdo, "parking/temp", temp);
    ok &= Firebase.RTDB.setFloat(&fbdo, "parking/hum", hum);
  } else {
    Serial.println("⚠️ DHT11 devolvió NaN, se omite envío de temp/hum.");
  }

  ok &= Firebase.RTDB.setFloat(&fbdo, "parking/distancia", distancia);
  ok &= Firebase.RTDB.setInt(&fbdo, "parking/ruido", ruido);
  ok &= Firebase.RTDB.setString(&fbdo, "parking/estado", estado);

  if (ok)
    Serial.println("✅ Datos enviados correctamente.");
  else
    Serial.println("❌ Error enviando datos: " + fbdo.errorReason());

  delay(2000);
}
