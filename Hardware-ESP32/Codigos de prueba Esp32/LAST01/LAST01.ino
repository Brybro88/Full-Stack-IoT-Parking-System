#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "DHT.h"
#include <time.h>

// ==================================================
// 1. CONFIGURACIÓN DE RED Y FIREBASE
// ==================================================
const char* WIFI_SSID = "INFINITUM2CB5";
const char* WIFI_PASSWORD = "X7e4upMA4Y";
const char* API_KEY = "AIzaSyA7kOahrIAr_XImPq05EhbJ7CZSPATIZUs";
const char* DATABASE_URL = "https://parking-iot-a0b8e-default-rtdb.firebaseio.com/";

const char* FB_PATH_STATUS = "/Estacionamiento/status";
const char* FB_PATH_LOGS = "/Estacionamiento/logs";

// ==================================================
// 2. CONFIGURACIÓN DE SENSORES Y PINES
// ==================================================
#define DHTPIN 4
#define DHTTYPE DHT22
#define TRIG_PIN 5
#define ECHO_PIN 18
#define BUZZER_PIN 19
#define KY037_PIN 34

DHT dht(DHTPIN, DHTTYPE);

// ==================================================
// 3. OBJETOS FIREBASE Y VARIABLES GLOBALES
// ==================================================
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastSendTime = 0;
const int sendInterval = 10000; // cada 10 segundos
const float DISTANCIA_ALERTA = 20.0; // cm
const float DISTANCIA_OCUPADO = 30.0; // cm

// ==================================================
// 4. CONEXIÓN WIFI Y FIREBASE
// ==================================================
void conectarWiFi() {
  Serial.print("🔌 Conectando a WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (++intentos > 40) {
      Serial.println("\n⚠️ No se pudo conectar a WiFi. Reiniciando...");
      ESP.restart();
    }
  }
  Serial.printf("\n✅ WiFi Conectado: %s\n", WiFi.localIP().toString().c_str());
}

void conectarFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("🔥 Firebase conectado correctamente.");
}

// ==================================================
// 5. FUNCIONES DE LECTURA DE SENSORES
// ==================================================
float medirDistancia() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duracion = pulseIn(ECHO_PIN, HIGH, 30000);
  float distancia = duracion * 0.034 / 2;
  if (distancia <= 0 || distancia > 400) return NAN;
  return distancia;
}

void controlBuzzer(float distancia) {
  if (!isnan(distancia)) {
    if (distancia <= DISTANCIA_ALERTA) {
      digitalWrite(BUZZER_PIN, HIGH);
      Serial.println("🚨 ¡PELIGRO! Vehículo muy cerca — Buzzer ACTIVADO");
    } else {
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("🟢 Distancia segura — Buzzer APAGADO");
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// ==================================================
// 6. SETUP
// ==================================================
void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(KY037_PIN, INPUT);

  dht.begin();
  conectarWiFi();
  conectarFirebase();

  Serial.println("🚀 Sistema de Estacionamiento Iniciado y listo.");
}

// ==================================================
// 7. LOOP PRINCIPAL
// ==================================================
void loop() {
  unsigned long ahora = millis();

  // Reintentar WiFi si se pierde conexión
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi desconectado, intentando reconectar...");
    conectarWiFi();
  }

  // Lecturas de sensores
  float temperatura = dht.readTemperature();
  float humedad = dht.readHumidity();
  float distancia = medirDistancia();
  int sonido = analogRead(KY037_PIN);

  controlBuzzer(distancia);

  // Estado general del estacionamiento
  String estado = "Libre";
  if (!isnan(distancia) && distancia < DISTANCIA_OCUPADO) {
    estado = "Ocupado";
  }

  // Mostrar datos en monitor serial
  Serial.println("----------------------------------------------------");
  Serial.printf("🌡 Temperatura: %.2f°C | 💧 Humedad: %.2f%% | 📏 Distancia: %.2fcm | 🎤 Sonido: %d | 🅿️ Estado: %s\n",
                temperatura, humedad, distancia, sonido, estado.c_str());

  // Envío periódico a Firebase
  if (ahora - lastSendTime > sendInterval) {
    lastSendTime = ahora;

    FirebaseJson json;
    json.set("temperatura_c", String(temperatura));
    json.set("humedad_pct", String(humedad));
    json.set("distancia_cm", String(distancia));
    json.set("nivel_sonido", sonido);
    json.set("estado", estado);
    json.set("timestamp", String(esp_timer_get_time() / 1000000)); // segundos desde inicio

    Serial.println("📤 Enviando datos a Firebase...");

    if (Firebase.RTDB.setJSON(&fbdo, FB_PATH_STATUS, &json)) {
      Serial.println("✅ Datos enviados correctamente a /status");
    } else {
      Serial.print("❌ Error al enviar: ");
      Serial.println(fbdo.errorReason());
    }

    // Registrar log (historial)
    if (Firebase.RTDB.pushJSON(&fbdo, FB_PATH_LOGS, &json)) {
      Serial.println("🗃️ Registro histórico guardado en /logs");
    } else {
      Serial.print("⚠️ Error al guardar log: ");
      Serial.println(fbdo.errorReason());
    }

    // Comprobar lectura
    if (Firebase.RTDB.getJSON(&fbdo, FB_PATH_STATUS)) {
      Serial.println("📥 Lectura Firebase OK:");
      Serial.println(fbdo.payload());
    }
  }

  delay(500);
}
