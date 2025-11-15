#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <DHT.h>
#include <time.h>

// ===================== CONFIG =====================
#define WIFI_SSID "Bryan's Galaxy Z Flip3 5G"
#define WIFI_PASSWORD "begiakhi88"

#define API_KEY "AIzaSyAe_GED12ttFaO68TkFBAq45RQMC9bB96Q"
#define DATABASE_URL "https://parking-test-14cef-default-rtdb.firebaseio.com/"

// ===== PINES =====
#define TRIG_PIN 5
#define ECHO_PIN 18
#define LED_VERDE 22
#define LED_ROJO 23
#define BUZZER 21
#define DHTPIN 4
#define DHTTYPE DHT22
#define KY037_PIN 34  // Micrófono analógico KY-037

// ===================== OBJETOS =====================
FirebaseData fbdo;
FirebaseData fbdo_verifier; // ===== AÑADIDO PARA PRUEBAS ===== Objeto extra para no interferir con el principal
FirebaseAuth auth;
FirebaseConfig config;
DHT dht(DHTPIN, DHTTYPE);

// ===================== VARIABLES =====================
int distancia = 999;
int distanciaAnterior = 999;
float temp = NAN, hum = NAN;
int sonido = 0;
String ultimoEstado = "";
unsigned long ultimoEnvioFirebase = 0;
unsigned long ultimoDHT = 0;
unsigned long tiempoAnterior = 0;

const unsigned long intervaloLoop = 100;
const unsigned long intervaloDHT = 2500;
const unsigned long intervaloEnvio = 5000;
const unsigned long minEnvioEntreCambios = 2000;

// ===================== FUNCIONES =====================
void tokenStatusCallback(FirebaseConfig *config) {}

void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("🔁 Conectando WiFi...");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 4000) delay(100);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ Conectado a WiFi");
  } else {
    Serial.println("⚠️ Falló WiFi");
  }
}

void ensureFirebase() {
  if (!Firebase.ready()) {
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    config.token_status_callback = tokenStatusCallback;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
  }
}

// --- Medición HC-SR04 con filtro ---
int medirDistanciaFiltrada() {
  const int N = 5;
  const int MIN_VALID = 5;
  const int MAX_VALID = 300;
  int valid = 0;
  long suma = 0;

  for (int i = 0; i < N; i++) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duracion = pulseIn(ECHO_PIN, HIGH, 25000);
    int d = duracion * 0.034 / 2;
    if (d >= MIN_VALID && d <= MAX_VALID) {
      suma += d;
      valid++;
    }
    delay(10);
  }

  if (valid == 0) return distanciaAnterior;
  distanciaAnterior = suma / valid;
  return distanciaAnterior;
}

// --- Lectura DHT con validación ---
void leerDHT() {
  unsigned long now = millis();
  if (now - ultimoDHT < intervaloDHT) return;
  ultimoDHT = now;
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temp = t;
  if (!isnan(h)) hum = h;
}

// --- Detección de ruido fuerte ---
bool ruidoExcesivo(int umbral = 2700) {
  sonido = analogRead(KY037_PIN);
  return (sonido > umbral);
}

// --- Buzzer control ---
void buzzerBeep(int freq, int durMs) {
  tone(BUZZER, freq);
  delay(durMs);
  noTone(BUZZER);
}

// --- Lógica de estado mejorada ---
String decidirEstado(int dist) {
  static unsigned long ultimoCambio = 0;
  static String estadoPrevio = "Libre";

  // Evitar falsos “999”
  if (dist >= 400 || dist <= 0) dist = distanciaAnterior;

  String estado;
  if (dist > 120) {
    estado = "Libre";
  } else if (dist > 35 && dist <= 120) {
    estado = "Aproximacion";
  } else {
    // Verificar si está estable (vehículo detenido)
    static int contadorEstable = 0;
    if (abs(dist - distanciaAnterior) <= 3) contadorEstable++;
    else contadorEstable = 0;

    if (contadorEstable >= 3) estado = "Ocupado";
    else estado = "Maniobra";
  }

  if (estado != estadoPrevio) ultimoCambio = millis();
  estadoPrevio = estado;
  return estado;
}

// ===== AÑADIDO PARA PRUEBAS =====
// --- Función para verificar la escritura leyendo el dato de vuelta ---
void verificarDatoEnFirebase(String path, String key, String expectedValue) {
  Serial.print("🔎 Verificando dato en Firebase... ");
  // Intentamos leer el campo 'estado' del path que acabamos de actualizar
  if (Firebase.RTDB.getString(&fbdo_verifier, path + "/" + key)) {
    if (fbdo_verifier.stringData() == expectedValue) {
      Serial.println("✅ Verificado correctamente!");
    } else {
      Serial.println("⚠️ Fallo de verificación: El dato leído no coincide.");
      Serial.printf("   - Se esperaba: %s\n", expectedValue.c_str());
      Serial.printf("   - Se leyó: %s\n", fbdo_verifier.stringData().c_str());
    }
  } else {
    Serial.println("⚠️ Fallo al leer el dato de vuelta.");
    Serial.printf("   - Razón: %s\n", fbdo_verifier.errorReason().c_str());
  }
}


// --- Envío Firebase (Mejorado con manejo de errores) ---
void enviarFirebase(String estado) {
  if (WiFi.status() != WL_CONNECTED || !Firebase.ready()) return;

  Serial.print("📡 Enviando a Firebase... ");

  FirebaseJson json;
  json.set("estado", estado);
  json.set("distancia_cm", distancia);
  json.set("temperatura_c", String(temp, 1)); // Enviar como String con 1 decimal
  json.set("humedad_pct", String(hum, 1));   // Enviar como String con 1 decimal
  json.set("nivel_sonido", sonido);

  time_t now = time(nullptr);
  char ts[30];
  strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
  json.set("timestamp", ts);

  // Guarda estado actual
  String pathStatus = "/parking_status";
  if (Firebase.RTDB.setJSON(&fbdo, pathStatus.c_str(), &json)) {
    Serial.print("Status OK. ");
    // ===== AÑADIDO PARA PRUEBAS =====
    verificarDatoEnFirebase(pathStatus, "estado", estado); // Verificamos que se escribió bien
  } else {
    Serial.print("Status FALLÓ. ");
    // ===== AÑADIDO PARA PRUEBAS =====
    Serial.printf("Razón: %s\n", fbdo.errorReason().c_str());
  }

  // Guarda histórico
  String pathHist = "/parking_logs/" + String(ts);
  if (Firebase.RTDB.setJSON(&fbdo, pathHist.c_str(), &json)) {
    Serial.println("Historial OK.");
  } else {
    Serial.print("Historial FALLÓ. ");
    // ===== AÑADIDO PARA PRUEBAS =====
    Serial.printf("Razón: %s\n", fbdo.errorReason().c_str());
  }
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(KY037_PIN, INPUT);

  dht.begin();
  ensureWiFi();
  configTime(0, 0, "pool.ntp.org");
  ensureFirebase();

  // ===== AÑADIDO PARA PRUEBAS =====
  // --- Prueba de conexión inicial al arrancar ---
  if (Firebase.ready()) {
    Serial.println("✅ Firebase listo. Realizando prueba de escritura inicial...");
    FirebaseJson deviceStatus;
    deviceStatus.set("status", "online");
    deviceStatus.set("last_boot", time(nullptr));
    if (Firebase.RTDB.setJSON(&fbdo, "/device_status", &deviceStatus)) {
      Serial.println("👍 Prueba inicial exitosa. El dispositivo está online en Firebase.");
    } else {
      Serial.println("👎 Falló la prueba de escritura inicial.");
      Serial.printf("   - Razón: %s\n", fbdo.errorReason().c_str());
    }
  } else {
    Serial.println("⚠️ No se pudo inicializar Firebase.");
  }
  
  Serial.println("🚀 Sistema listo");
}

// ===================== LOOP =====================
void loop() {
  ensureWiFi(); // Revisa la conexión en cada ciclo
  ensureFirebase(); // Revisa si Firebase sigue listo

  distancia = medirDistanciaFiltrada();
  leerDHT();
  bool alarma = ruidoExcesivo();

  String estado = decidirEstado(distancia);

  // LEDs según estado
  if (estado == "Libre") {
    digitalWrite(LED_VERDE, HIGH);
    digitalWrite(LED_ROJO, LOW);
  } else {
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_ROJO, HIGH);
  }

  // Buzzer: beep solo en aproximación o ruido alto
  if (estado == "Aproximacion" || alarma) buzzerBeep(1000, 80);

  // Mostrar en Serial
  Serial.printf("📏 %d cm | 🌡 %.1f°C | 💧 %.1f%% | 🔊 %d | Estado: %s\n",
                distancia, temp, hum, sonido, estado.c_str());

  // Envío cada 5 seg o si hay cambio
  unsigned long now = millis();
  if (now - ultimoEnvioFirebase > intervaloEnvio || estado != ultimoEstado) {
    if (estado != ultimoEstado && (now - ultimoEnvioFirebase < minEnvioEntreCambios)) {
      // Si el estado cambió muy rápido, esperamos un poco para no saturar
      delay(minEnvioEntreCambios);
    }
    enviarFirebase(estado);
    ultimoEnvioFirebase = now;
    ultimoEstado = estado;
  }

  delay(intervaloLoop);
}