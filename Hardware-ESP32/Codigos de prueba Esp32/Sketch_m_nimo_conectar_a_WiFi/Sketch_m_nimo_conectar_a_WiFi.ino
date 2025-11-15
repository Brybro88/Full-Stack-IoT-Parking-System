// Sketch mínimo: conectar a WiFi y enviar mensajes al Serial
#include <WiFi.h>

const char* ssid = "IBARRA 25";
const char* password = "COSMO3625";

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("=== Prueba WiFi ESP32 ===");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Conectando a WiFi");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) { // ~20 segundos max
    delay(500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("✅ Conectado a WiFi");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    Serial.print("RSSI (dBm): "); Serial.println(WiFi.RSSI());
  } else {
    Serial.println();
    Serial.println("❌ No se pudo conectar. Revisa credenciales o señal.");
  }
}

void loop() {
  // Enviar un "paquete" sencillo cada 5 segundos al Serial (como si fuera payload)
  Serial.print("{\"ts\":");
  Serial.print(millis());
  Serial.print(",\"ip\":\""); Serial.print(WiFi.localIP());
  Serial.print("\",\"rssi\":"); Serial.print(WiFi.RSSI());
  Serial.println("}");
  delay(5000);
}
