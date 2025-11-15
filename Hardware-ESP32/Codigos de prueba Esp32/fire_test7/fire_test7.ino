/*
  Versión mejorada ESP32:
  - Reconexión WiFi + Firebase (backoff)
  - Lecturas DHT robustas con reintentos
  - Evita enviar NaN; incluye valid flags
  - Envío eficiente de JSON sólo cuando toca
*/

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <DHT.h>
#include <time.h>

// ========== CONFIG (ajusta según tu HW) ==========
#define WIFI_SSID "INFINITUM1F9C"
#define WIFI_PASSWORD "YH4yp4haj"

#define API_KEY "AIzaSyAe_GED12ttFaO68TkFBAq45RQMC9bB96Q"
#define DATABASE_URL "https://parking-test-14cef-default-rtdb.firebaseio.com/"

// Pines
#define TRIG_PIN 5
#define ECHO_PIN 18
#define LED_VERDE 22
#define LED_ROJO 23
#define BUZZER 21
#define DHTPIN 4
#define DHTTYPE DHT22
#define KY037_PIN 34   // analog

// ========== OBJETOS ==========
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
DHT dht(DHTPIN, DHTTYPE);

// ========== TIMERS Y CONSTANTES ==========
const unsigned long intervaloLoop = 100;       // ciclo principal (ms)
const unsigned long intervaloDHT = 2500;       // tiempo entre lecturas DHT (ms)
const unsigned long intervaloEnvio = 5000;     // tiempo mínimo entre envíos al RTDB (ms)
const unsigned long minEnvioEntreCambios = 2000; // debounce de cambios de estado (ms)

// WiFi reconnection/backoff
const unsigned long wifiBaseBackoff = 2000;   // 2s
const unsigned long wifiMaxBackoff  = 30000;  // 30s
int wifiFailCount = 0;
unsigned long nextWifiAttempt = 0;

// Firebase reconnection/backoff
const unsigned long fbBaseBackoff = 2000;
const unsigned long fbMaxBackoff  = 30000;
int fbFailCount = 0;
unsigned long nextFbAttempt = 0;

// Variables sensores / estado
int distancia = 999;
int distanciaAnterior = 999;
float temp = NAN;
float hum = NAN;
int sonido = 0;
String ultimoEstado = "";
unsigned long ultimoEnvioFirebase = 0;
unsigned long ultimoDHT = 0;
unsigned long tiempoAnterior = 0;

// Buzzer/led non-blocking (simple)
bool buzzerEnabled = false;
bool buzzerOn = false;
unsigned long buzzerUntil = 0;
unsigned long buzzerOffUntil = 0;
unsigned long buzzerOnDuration = 0;
unsigned long buzzerOffDuration = 0;
int buzzerFreq = 1000;

// ======== FUNCIONES DE UTILIDAD ========
void tokenStatusCallback(FirebaseConfig *config) {
  Serial.println("🔑 Token status callback");
}

// Reconectar WiFi con backoff exponencial (no bloqueante)
void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiFailCount = 0;
    return;
  }
  unsigned long now = millis();
  if (now < nextWifiAttempt) return;
  Serial.println("🔁 Intentando conectar WiFi...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  // Intento corto, no bloquear mucho: esperamos hasta 3 segundos por intento
  while (millis() - start < 3000) {
    if (WiFi.status() == WL_CONNECTED) break;
    delay(100);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("✅ WiFi conectado: ");
    Serial.println(WiFi.localIP());
    wifiFailCount = 0;
  } else {
    wifiFailCount++;
    unsigned long backoff = wifiBaseBackoff * (1UL << min(wifiFailCount, 6)); // hasta ~128x
    if (backoff > wifiMaxBackoff) backoff = wifiMaxBackoff;
    nextWifiAttempt = now + backoff;
    Serial.printf("⚠️ WiFi no conectado. Reintentará en %lu ms\n", backoff);
  }
}

// Reconectar Firebase con backoff (no bloqueante)
void ensureFirebase() {
  if (!Firebase.ready()) {
    unsigned long now = millis();
    if (now < nextFbAttempt) return;
    Serial.println("🔁 Intentando inicializar Firebase...");
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    config.token_status_callback = tokenStatusCallback;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    // Intenta signup anónimo
    if (Firebase.signUp(&config, &auth, "", "")) {
      Serial.println("🔐 Firebase: login anónimo OK");
      fbFailCount = 0;
    } else {
      fbFailCount++;
      unsigned long backoff = fbBaseBackoff * (1UL << min(fbFailCount, 6));
      if (backoff > fbMaxBackoff) backoff = fbMaxBackoff;
      nextFbAttempt = now + backoff;
      Serial.printf("❌ Firebase login falló. Reintentará en %lu ms\n", backoff);
    }
  } else {
    fbFailCount = 0;
  }
}

// Medir distancia con HC-SR04 (una lectura)
int medirDistanciaOnce() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur = pulseIn(ECHO_PIN, HIGH, 25000); // timeout 25ms ~ 4.25m
  if (dur == 0) return -1;
  int d = (int)(dur * 0.034 / 2.0);
  return d;
}

// Lectura filtrada (promedio de N validas)
int medirDistanciaFiltrada() {
  const int N = 4;
  const int MIN_VALID = 2;
  const int MAX_VALID = 400;
  long sum = 0;
  int valid = 0;
  for (int i = 0; i < N; ++i) {
    int d = medirDistanciaOnce();
    if (d >= MIN_VALID && d <= MAX_VALID) {
      sum += d;
      valid++;
    }
    delay(6);
  }
  if (valid == 0) {
    // si no hay lecturas válidas, devolvemos la anterior
    return distanciaAnterior;
  }
  int avg = sum / valid;
  distanciaAnterior = avg;
  return avg;
}

// Lectura DHT con reintentos cortos (no bloquante en el loop general)
void leerDHTconReintento() {
  unsigned long now = millis();
  if (now - ultimoDHT < intervaloDHT) return;
  ultimoDHT = now;
  // Intento 1
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    // reintento corto
    delay(100);
    t = dht.readTemperature();
    h = dht.readHumidity();
    if (isnan(t) || isnan(h)) {
      Serial.println("⚠️ DHT: lectura fallida tras reintento");
      // no actualizar temp/hum (mantiene último válido)
      return;
    }
  }
  temp = t;
  hum = h;
  Serial.printf("🌡 DHT ok: %.1f°C  💧 %.1f%%\n", temp, hum);
}

// manejo buzzer simple (pattern no-bloqueante)
void setBuzzerPattern(int freq, unsigned long onMs, unsigned long offMs) {
  buzzerFreq = freq;
  buzzerOnDuration = onMs;
  buzzerOffDuration = offMs;
  buzzerEnabled = true;
  buzzerOn = false;
  buzzerOffUntil = millis();
}

void stopBuzzer() {
  buzzerEnabled = false;
  noTone(BUZZER);
}

void handleBuzzer() {
  if (!buzzerEnabled) return;
  unsigned long now = millis();
  if (buzzerOn) {
    if (now >= buzzerUntil) {
      noTone(BUZZER);
      buzzerOn = false;
      buzzerOffUntil = now + buzzerOffDuration;
    }
  } else {
    if (now >= buzzerOffUntil) {
      tone(BUZZER, buzzerFreq);
      buzzerOn = true;
      buzzerUntil = now + buzzerOnDuration;
    }
  }
}

// Construye y envía JSON de forma segura (evita NaN)
void enviarFirebaseSiCorresponde(const String &estado, bool forceSend = false) {
  unsigned long now = millis();
  if (!forceSend && (now - ultimoEnvioFirebase < intervaloEnvio)) return;
  if (WiFi.status() != WL_CONNECTED || !Firebase.ready()) {
    Serial.println("⚠️ No se enviará: WiFi/Firebase no listos");
    return;
  }

  // Preparar JSON
  FirebaseJson json;
  json.set("estado", estado);
  json.set("distancia", distancia);

  // Enviar valid flags en vez de NaN:
  if (!isnan(temp)) {
    json.set("temperatura", temp);
    json.set("temperatura_valid", true);
  } else {
    json.set("temperatura_valid", false);
  }
  if (!isnan(hum)) {
    json.set("humedad", hum);
    json.set("humedad_valid", true);
  } else {
    json.set("humedad_valid", false);
  }
  json.set("nivel_sonido", sonido);

  // timestamp ISO simple
  time_t nowt = time(nullptr);
  char ts[32];
  struct tm *tm_info = localtime(&nowt);
  strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", tm_info);
  json.set("timestamp", ts);

  // Rutas
  String basePath = "parking_status";
  String logPath  = "parking_logs/";
  logPath += String(ts);

  // Enviar en dos operaciones:
  bool ok1 = Firebase.RTDB.setJSON(&fbdo, basePath.c_str(), &json);
  bool ok2 = Firebase.RTDB.setJSON(&fbdo, logPath.c_str(), &json);

  if (ok1 && ok2) {
    Serial.println("✅ Envío Firebase OK");
    ultimoEnvioFirebase = now;
  } else {
    Serial.printf("❌ Error Firebase: %s\n", fbdo.errorReason().c_str());
  }
}

// Decide estado por distancia y maneja actuadores (LED/buzzer)
String decidirEstadoYActuar(int dist) {
  String estado = "Desconocido";
  if (dist <= 0 || dist > 100) {
    estado = "Libre";
    digitalWrite(LED_VERDE, HIGH);
    digitalWrite(LED_ROJO, LOW);
    stopBuzzer();
  } else if (dist > 30 && dist <= 100) {
    estado = "Aproximacion";
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_ROJO, HIGH);
    int freq = map(dist, 100, 30, 800, 1200);
    int durOn = map(dist, 100, 30, 120, 60);
    setBuzzerPattern(freq, durOn, durOn);
  } else { // dist <= 30
    // estaríamos en maniobra/ocupado; simplificamos: si estable -> ocupado
    static int counterQuiet = 0;
    if (abs(dist - distanciaAnterior) <= 3) counterQuiet++; else counterQuiet = 0;
    if (counterQuiet >= 4) {
      estado = "Ocupado";
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_ROJO, HIGH);
      stopBuzzer();
    } else {
      estado = "Maniobra";
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_ROJO, HIGH);
      setBuzzerPattern(1600, 150, 150);
    }
  }
  return estado;
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(50);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(KY037_PIN, INPUT);

  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_ROJO, LOW);
  noTone(BUZZER);

  dht.begin();

  // Inicia WiFi (no espera indefinidamente)
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  nextWifiAttempt = millis(); // intenta inmediatamente en loop
  nextFbAttempt = millis();

  // NTP time
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  Serial.println("⏳ Sincronizando hora...");

  // CONFIG FIREBASE básico (se inicializa en ensureFirebase)
  config.token_status_callback = tokenStatusCallback;
}

// ===== LOOP =====
void loop() {
  unsigned long now = millis();

  // Lógica de reconexión (WiFi y Firebase)
  ensureWiFi();
  ensureFirebase();

  // Control non-blocking: si no ha pasado ciclo, ejecutar handlers y salir
  if (now - tiempoAnterior < intervaloLoop) {
    handleBuzzer();
    return;
  }
  tiempoAnterior = now;

  // Lecturas sensores
  distancia = medirDistanciaFiltrada();
  leerDHTconReintento();
  sonido = analogRead(KY037_PIN);

  // Decidir estado y actuar
  String estado = decidirEstadoYActuar(distancia);

  // Mostrar por Serial (útil para debug)
  Serial.printf("📏 Dist: %d cm | 🌡 Temp: %s | 💧 Hum: %s | 🔊 %d | Estado: %s\n",
                distancia,
                isnan(temp) ? "NaN" : String(temp,1).c_str(),
                isnan(hum) ? "NaN" : String(hum,1).c_str(),
                sonido,
                estado.c_str());

  // Envío eficiente: si cambia estado o paso tiempo
  bool stateChanged = (estado != ultimoEstado);
  bool forceSend = false;
  if (stateChanged && (now - ultimoEnvioFirebase >= minEnvioEntreCambios)) forceSend = true;

  enviarFirebaseSiCorresponde(estado, forceSend);

  if (stateChanged) ultimoEstado = estado;

  // Actual handlers
  handleBuzzer();
}
