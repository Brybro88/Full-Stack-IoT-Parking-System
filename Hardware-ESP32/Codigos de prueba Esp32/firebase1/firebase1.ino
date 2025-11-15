#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// 🔹 Config WiFi
#define WIFI_SSID "IBARRA 25"
#define WIFI_PASSWORD "COSMO3625"

// 🔹 Config Firebase
#define API_KEY "AIzaSyAe_GED12ttFaO68TkFBAq45RQMC9bB96Q"
#define DATABASE_URL "https://parking-test-14cef-default-rtdb.firebaseio.com/"

// Objetos Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(" Conectado!");

  // 🔹 Config Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Inicializar Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Iniciar sesión anónimo (compatible con versiones antiguas)
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Inicio de sesión anónimo exitoso!");
  } else {
    Serial.printf("Error al iniciar sesión anónimo: %s\n", config.signer.signupError.message.c_str());
  }

  delay(2000);

  if (Firebase.ready() && Firebase.RTDB.setString(&fbdo, "parking_status/estado", "Libre")) {
    Serial.println("Dato enviado a Firebase!");
  } else {
    Serial.println("Error: " + fbdo.errorReason());
  }
}

void loop() {
}
