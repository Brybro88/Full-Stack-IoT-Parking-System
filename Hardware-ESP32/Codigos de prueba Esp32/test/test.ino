#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// --- PEGA TUS CREDENCIALES AQUÍ ---
const char* WIFI_SSID = "INFINITUM2CB5";
const char* WIFI_PASSWORD = "X7e4upMA4Y";
const char* API_KEY = "AIzaSyA7kOahrIAr_XImPq05EhbJ7CZSPATIZUs";
const char* DATABASE_URL = "https://parking-iot-a0b8e-default-rtdb.firebaseio.com/";
// ------------------------------------

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- PRUEBA DE CONEXIÓN MÍNIMA ---");

  // 1. Conectar a WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Conectado.");

  // 2. Configurar Firebase
  Serial.println("Configurando Firebase...");
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // 3. Intentar Iniciar Firebase
  Serial.println("Intentando Firebase.begin()...");
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // 4. Verificar si la conexión fue exitosa
  if (Firebase.ready()) {
    Serial.println("✅✅✅ ¡ÉXITO! Firebase.ready() es VERDADERO.");
  } else {
    Serial.println("❌❌❌ FALLO: Firebase.ready() es FALSO.");
    Serial.println("    Razón del fallo: " + fbdo.errorReason());
    Serial.println("    Si dice 'Permission denied', son TUS REGLAS.");
    Serial.println("    Si dice otra cosa, es la API_KEY o la URL.");
  }

  Serial.println("--- Prueba finalizada ---");
}

void loop() {
  // No hacer nada
  delay(2000);
}
