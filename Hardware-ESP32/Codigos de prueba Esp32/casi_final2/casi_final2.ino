// ==========================
// Final ESP32 - Parking IoT
// DHT22, HC-SR04, KY-037, buzzer pasivo
// Muestra en Serial cada procedimiento y confirma Firebase
// ==========================

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <DHT.h>
#include <time.h>
#include <algorithm>

// ------------------ CREDENCIALES (las tuyas) ------------------
#define WIFI_SSID "INFINITUM2CB5"
#define WIFI_PASSWORD "X7e4upMA4Y"

#define API_KEY "AIzaSyA7kOahrIAr_XImPq05EhbJ7CZSPATIZUs"
#define DATABASE_URL "https://parking-iot-a0b8e-default-rtdb.firebaseio.com/"

// ------------------ PINES (los tuyos) ------------------
#define TRIG_PIN 5
#define ECHO_PIN 18
#define LED_VERDE 22
#define LED_ROJO 23
#define BUZZER_PIN 21
#define DHTPIN 4
#define DHTTYPE DHT22
#define KY037_PIN 34

// ------------------ CONSTANTES y UMBRALES ------------------
const unsigned long SENSOR_READ_INTERVAL = 500;     // ms
const unsigned long FIREBASE_SEND_INTERVAL = 5000;  // ms mínimo entre envíos
const unsigned long WIFI_RECONNECT_INTERVAL = 20000;
const float DIST_LIBRE = 120.0;
const float DIST_OCUPADO = 35.0;
const int RUIDO_UMBRAL = 2700;
const int STABILIZATION_COUNT = 4;    // lecturas para confirmar "Ocupado"

// ------------------ Firebase objects ------------------
FirebaseData fbdo;       // para status
FirebaseData fbdo_log;   // para logs
FirebaseAuth auth;
FirebaseConfig config;

// ------------------ Sensor objects & estado ------------------
DHT dht(DHTPIN, DHTTYPE);

unsigned long lastSensorMillis = 0;
unsigned long lastFirebaseMillis = 0;
unsigned long lastWifiReconnect = 0;

float distancia_cm = 999.0;
float temperatura_c = NAN;
float humedad_pct = NAN;
bool temperatura_valid = false;
bool humedad_valid = false;
int nivel_sonido = 0;

String estadoActual = "Inicializando";
String estadoPrevioParaDebounce = "Inicializando";
int debounceCounter = 0;
String ultimoEstadoEnviado = "";

// ------------------ Buzzer (no bloqueante simple) ------------------
bool buzzerEnabled = false;
bool buzzerOn = false;
unsigned long buzzerUntil = 0;
unsigned long buzzerOffUntil = 0;
unsigned long buzzerOnDuration = 0;
unsigned long buzzerOffDuration = 0;
int buzzerFreq = 2000; // Hz

// ------------------ PROTOTIPOS ------------------
void iniciarWiFi();
void ensureWiFi(unsigned long now);
void ensureFirebase(unsigned long now);
float medirDistanciaFiltrada();
void leerDHTconReintento();
void leerKY037();
String decidirEstado(float dist);
void manejarActuadores(String estado, bool cambio);
void setBuzzerPattern(int freq, unsigned long onMs, unsigned long offMs);
void handleBuzzer();
String getTimestampISO();
void enviarAFirebase(bool enviarLog);
void verificarEnFirebase(String path, String key, String expected);

// ------------------ SETUP ------------------
void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println("=== Inicio Sistema Parking IoT (final) ===");

  // pines
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(KY037_PIN, INPUT);

  digitalWrite(LED_ROJO, HIGH); // indicamos inicio
  dht.begin();

  iniciarWiFi();

  // NTP
  Serial.println("⏱ Intentando sincronizar hora (NTP)...");
  configTime(-6 * 3600, 3600, "pool.ntp.org", "time.google.com"); // GMT-6, ajuste si necesitas
  time_t now = time(nullptr);
  int tries = 0;
  while (now < 1600000000 && tries < 20) {
    delay(200);
    Serial.print(".");
    now = time(nullptr);
    tries++;
  }
  Serial.println();
  if (now >= 1600000000) Serial.println("✅ Hora NTP OK");
  else Serial.println("⚠️ NTP no sincronizada (continuamos de todas formas)");

  // Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = nullptr;
  Firebase.reconnectWiFi(true);

  // Intentar signup anonimoy begin() — mostramos resultado
  Serial.print("🔐 Autenticando en Firebase (anónimo)...");
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println(" ✅ Signup OK");
  } else {
    Serial.printf(" ⚠️ Signup error: %s\n", config.signer.signupError.message.c_str());
    // Aunque falle signup, llamamos a begin para inicializar internals
  }
  Firebase.begin(&config, &auth);
  if (Firebase.ready()) Serial.println("✅ Firebase inicializado y listo");
  else Serial.println("⚠️ Firebase NO listo (reintentará en loop)");

  digitalWrite(LED_ROJO, LOW);
  Serial.println("🚀 Setup completo.");
}

// ------------------ LOOP ------------------
void loop() {
  unsigned long now = millis();

  // 1) mantener WiFi / Firebase
  ensureWiFi(now);
  ensureFirebase(now);

  // 2) leer sensores periódicamente
  if (now - lastSensorMillis >= SENSOR_READ_INTERVAL) {
    lastSensorMillis = now;

    // guardar previo
    String prevEstado = estadoActual;

    // lecturas
    float newDist = medirDistanciaFiltrada();
    distancia_cm = (isnan(newDist) ? distancia_cm : newDist); // si newDist NaN mantener previo

    leerDHTconReintento(); // actualiza temperatura_c, humedad_pct y flags
    leerKY037();           // actualiza nivel_sonido

    // decidir estado con debounce
    estadoActual = decidirEstado(distancia_cm);

    bool cambio = (estadoActual != prevEstado);

    // mostrar cálculos por serial
    Serial.println("----------------------------------------------------");
    Serial.printf("Lecturas -> Distancia: %.2f cm | Temp: %s | Hum: %s | Sonido: %d\n",
                  distancia_cm,
                  temperatura_valid ? String(temperatura_c,1).c_str() : "NaN",
                  humedad_valid ? String(humedad_pct,1).c_str() : "NaN",
                  nivel_sonido);
    Serial.printf("Determinación: estadoActual = %s (prev: %s) -> cambio? %s\n",
                  estadoActual.c_str(), prevEstado.c_str(), cambio? "SI":"NO");

    // actuadores / buzzer
    manejarActuadores(estadoActual, cambio);

    // 3) Envío a Firebase si cambio o intervalo
    bool tiempoEnvio = (now - lastFirebaseMillis >= FIREBASE_SEND_INTERVAL);
    if ((cambio || tiempoEnvio) && Firebase.ready() && WiFi.status() == WL_CONNECTED) {
      Serial.printf("📡 %s — preparando JSON y enviando a Firebase...\n", cambio ? "Cambio de estado" : "Envio periódico");
      enviarAFirebase(cambio);
      lastFirebaseMillis = now;
    } else if (!Firebase.ready()) {
      Serial.println("⚠️ Firebase no listo: se omitirá envío esta pasada.");
    } else if (WiFi.status() != WL_CONNECTED) {
      Serial.println("⚠️ WiFi no conectado: se omitirá envío esta pasada.");
    }
  }

  // 4) manejar buzzer no bloqueante
  handleBuzzer();

  // no bloquear loop
  delay(10);
}

// ------------------ FUNCIONES ------------------

void iniciarWiFi() {
  Serial.print("🔌 Conectando a WiFi ");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("✅ WiFi conectado: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("❌ No se pudo conectar a WiFi en el tiempo esperado");
  }
}

void ensureWiFi(unsigned long now) {
  if (WiFi.status() != WL_CONNECTED && (now - lastWifiReconnect >= WIFI_RECONNECT_INTERVAL)) {
    Serial.println("🔁 WiFi desconectado, intentando reconectar...");
    WiFi.reconnect();
    lastWifiReconnect = now;
  }
}

void ensureFirebase(unsigned long now) {
  if (!Firebase.ready() && (now - lastWifiReconnect >= 5000)) {
    Serial.println("🔁 Intentando inicializar Firebase...");
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    if (Firebase.ready()) {
      Serial.println("✅ Firebase listo ahora");
    } else {
      Serial.printf("❌ Firebase sigue sin estar listo: %s\n", fbdo.errorReason().c_str());
    }
  }
}

// Filtrado de HC-SR04: mediana de 3 lecturas + valida
float medirDistanciaFiltrada() {
  float readings[3];
  for (int i=0;i<3;i++){
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long dur = pulseIn(ECHO_PIN, HIGH, 30000);
    if (dur == 0) readings[i] = NAN;
    else readings[i] = dur * 0.0343f / 2.0f;
    delay(10);
  }
  // eliminar NaN antes de ordenar: si 2 o más NaN devolvemos NaN
  int validCount = 0;
  for (int i=0;i<3;i++) if (!isnan(readings[i])) validCount++;
  if (validCount == 0) return NAN;
  if (validCount == 1) {
    for (int i=0;i<3;i++) if (!isnan(readings[i])) return readings[i];
  }
  // si al menos 2 válidas, ordenamos asignando gran valor a NaN
  float tmp[3];
  for (int i=0;i<3;i++) tmp[i] = isnan(readings[i]) ? 10000.0f : readings[i];
  std::sort(tmp, tmp+3);
  float med = tmp[1];
  if (med > 1000.0f) return NAN;
  return med;
}

void leerDHTconReintento() {
  unsigned long t = millis();
  // intento 1
  float t1 = dht.readTemperature();
  float h1 = dht.readHumidity();
  if (!isnan(t1) && !isnan(h1)) {
    temperatura_c = t1; humedad_pct = h1;
    temperatura_valid = true; humedad_valid = true;
    Serial.printf("✔️ DHT22 OK: T=%.1f°C H=%.1f%%\n", temperatura_c, humedad_pct);
    return;
  }
  // reintento corto
  delay(100);
  float t2 = dht.readTemperature();
  float h2 = dht.readHumidity();
  if (!isnan(t2)) { temperatura_c = t2; temperatura_valid = true; }
  if (!isnan(h2)) { humedad_pct = h2; humedad_valid = true; }
  if (temperatura_valid && humedad_valid) {
    Serial.printf("✔️ DHT22 OK (reintento): T=%.1f°C H=%.1f%%\n", temperatura_c, humedad_pct);
  } else {
    Serial.println("⚠️ DHT22: lectura fallida (se mantendrán últimos valores válidos si existen)");
  }
}

void leerKY037() {
  nivel_sonido = analogRead(KY037_PIN);
  // normalizar o escalar si deseas; aquí mostramos raw
  Serial.printf("🎤 Nivel sonido (raw): %d\n", nivel_sonido);
}

String decidirEstado(float dist) {
  if (isnan(dist)) {
    // si no podemos medir, mantenemos previo (evita disparos)
    Serial.println("⚠️ Distancia inválida (NaN): manteniendo estado previo");
    return estadoPrevioParaDebounce;
  }

  // lógica con hysteresis y confirmación temporal (debounceCounter)
  String nuevo;
  if (dist > DIST_LIBRE) nuevo = "Libre";
  else if (dist > DIST_OCUPADO) nuevo = "Aproximacion";
  else nuevo = "Cercano"; // candidato a "Ocupado"

  if (nuevo == "Cercano") {
    // contar lecturas estables
    debounceCounter++;
    Serial.printf("⏱ Lecturas cercanas consecutivas: %d/%d\n", debounceCounter, STABILIZATION_COUNT);
    if (debounceCounter >= STABILIZATION_COUNT) {
      estadoPrevioParaDebounce = "Ocupado";
      debounceCounter = STABILIZATION_COUNT; // no crece más
    } else {
      estadoPrevioParaDebounce = "Maniobra";
    }
  } else {
    // reiniciamos contador y pasamos a estado correspondiente
    debounceCounter = 0;
    estadoPrevioParaDebounce = nuevo == "Aproximacion" ? "Aproximacion" : "Libre";
  }
  return estadoPrevioParaDebounce;
}

void manejarActuadores(String estado, bool cambio) {
  // LEDs
  if (estado == "Libre") {
    digitalWrite(LED_VERDE, HIGH);
    digitalWrite(LED_ROJO, LOW);
  } else {
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_ROJO, HIGH);
  }

  // Buzzer: si hay cambio a Aproximacion u Ocupado -> pitido corto
  if (cambio && (estado == "Aproximacion" || estado == "Ocupado")) {
    Serial.println("🔊 Beep! (detección de vehículo)");
    // patrón: 2 pitidos cortos
    setBuzzerPattern(1500, 90, 90); // freq, on, off
  }

  // Si ruido alto, alarma distinta
  if (nivel_sonido > RUIDO_UMBRAL) {
    Serial.println("🚨 Ruido alto detectado -> alarma");
    setBuzzerPattern(2500, 200, 150);
  }
}

void setBuzzerPattern(int freq, unsigned long onMs, unsigned long offMs) {
  buzzerFreq = freq;
  buzzerOnDuration = onMs;
  buzzerOffDuration = offMs;
  buzzerEnabled = true;
  buzzerOn = false;
  buzzerOffUntil = millis(); // arranca pronto
}

void handleBuzzer() {
  if (!buzzerEnabled) return;
  unsigned long now = millis();
  if (buzzerOn) {
    if (now >= buzzerUntil) {
      noTone(BUZZER_PIN);
      buzzerOn = false;
      buzzerOffUntil = now + buzzerOffDuration;
    }
  } else {
    if (now >= buzzerOffUntil) {
      tone(BUZZER_PIN, buzzerFreq);
      buzzerOn = true;
      buzzerUntil = now + buzzerOnDuration;
    }
  }
}

// timestamp ISO
String getTimestampISO() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return String("1970-01-01T00:00:00Z");
  char buf[30];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buf);
}

void enviarAFirebase(bool enviarLog) {
  // construir JSON seguro
  FirebaseJson json;

  String ts = getTimestampISO();
  json.set("estado", estadoActual);
  json.set("distancia_cm", String(distancia_cm, 2));
  if (temperatura_valid) {
    json.set("temperatura_c", String(temperatura_c, 1));
    json.set("temperatura_valid", true);
  } else {
    json.set("temperatura_valid", false);
  }
  if (humedad_valid) {
    json.set("humedad_pct", String(humedad_pct, 1));
    json.set("humedad_valid", true);
  } else {
    json.set("humedad_valid", false);
  }
  json.set("nivel_sonido", nivel_sonido);
  json.set("timestamp", ts);

  // imprimir JSON en serial (para confirmar exactamente qué se envía)
  String jsonStr;
  json.toString(jsonStr);
  Serial.println("📤 JSON preparado para Firebase:");
  Serial.println(jsonStr);

  // 1) escribir estado actual (setJSON)
  String statusPath = "/Estacionamiento/status"; // coincide con tu estructura
  Serial.printf("📡 Intentando setJSON en '%s' ...\n", statusPath.c_str());
  if (Firebase.RTDB.setJSON(&fbdo, statusPath.c_str(), &json)) {
    Serial.println("✅ setJSON OK -> estado actualizado en Firebase.");
    Serial.printf("  fbdo.path: %s\n", fbdo.dataPath().c_str());
    Serial.printf("  fbdo.payload: %s\n", fbdo.payload().c_str());
  } else {
    Serial.printf("❌ setJSON falló: %s\n", fbdo.errorReason().c_str());
  }

  // 2) push log (solo si enviarLog==true)
  if (enviarLog) {
    Serial.println("📡 Push de log a /Estacionamiento/logs ...");
    if (Firebase.RTDB.pushJSON(&fbdo_log, "/Estacionamiento/logs", &json)) {
      Serial.println("✅ pushJSON OK -> log guardado.");
      Serial.printf("  fbdo_log.path: %s\n", fbdo_log.dataPath().c_str());
      Serial.printf("  fbdo_log.key: %s\n", fbdo_log.pushName().c_str());
    } else {
      Serial.printf("❌ pushJSON falló: %s\n", fbdo_log.errorReason().c_str());
    }
  }

  // 3) lectura de verificación (leer estadoActual desde Firebase)
  Serial.println("🔎 Verificando valor guardado (lectura)...");
  if (Firebase.RTDB.getJSON(&fbdo, statusPath.c_str())) {
    Serial.println("📥 Lectura OK. Contenido:");
    Serial.println(fbdo.payload());
  } else {
    Serial.printf("⚠️ Lectura de verificación falló: %s\n", fbdo.errorReason().c_str());
  }
}

void verificarEnFirebase(String path, String key, String expected) {
  // helper si quieres leer una key específica
  if (Firebase.RTDB.getString(&fbdo, (path + "/" + key).c_str())) {
    Serial.printf("Verif %s/%s = %s\n", path.c_str(), key.c_str(), fbdo.stringData().c_str());
    if (fbdo.stringData() == expected) Serial.println("✅ Coincide con esperado");
    else Serial.println("❌ No coincide");
  } else {
    Serial.printf("❌ No se pudo leer %s/%s : %s\n", path.c_str(), key.c_str(), fbdo.errorReason().c_str());
  }
}
