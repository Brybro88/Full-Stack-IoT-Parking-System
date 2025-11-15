#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "DHT.h"
#include <time.h>

// ===== CONFIGURACIÓN DE RED =====
#define WIFI_SSID "Bryan's Galaxy Z Flip3 5G"
#define WIFI_PASSWORD "begiakhi88"

// ===== CONFIGURACIÓN DE FIREBASE =====
#define API_KEY "AIzaSyAe_GED12ttFaO68TkFBAq45RQMC9bB96Q"
#define DATABASE_URL "https://parking-test-14cef-default-rtdb.firebaseio.com/"

// ===== OBJETOS FIREBASE =====
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ===== PINES =====
#define TRIG_PIN 5
#define ECHO_PIN 18
#define LED_VERDE 22
#define LED_ROJO 23
#define BUZZER 21
#define DHTPIN 4
#define DHTTYPE DHT22
#define KY037_PIN 34   // Micrófono KY-037 (AO analógico)

// ===== OBJETOS =====
DHT dht(DHTPIN, DHTTYPE);

// ===== VARIABLES =====
long duracion;
int distancia = 999;
int distanciaAnterior = 999;
int contadorQuieto = 0;
const int UMBRAL_MOV = 4;
const int CICLOS_QUIETO = 5;
bool alertaMostrada = false;

float temp = NAN;
float hum = NAN;
int sonido = 0;

unsigned long ultimoDHT = 0;
const unsigned long intervaloDHT = 2500;
unsigned long tiempoAnterior = 0;
const unsigned long intervaloLoop = 100;

String ultimoEstado = "";

// ===== FIREBASE =====
unsigned long ultimoEnvioFirebase = 0;
const unsigned long intervaloEnvio = 5000;
const unsigned long minEnvioEntreCambios = 2000;

// ===== BUZZER NO BLOQUEANTE =====
bool buzzerEnabled = false;
bool buzzerOn = false;
unsigned long buzzerUntil = 0;
unsigned long buzzerOffUntil = 0;
unsigned long buzzerOnDuration = 0;
unsigned long buzzerOffDuration = 0;
int buzzerFreq = 1000;

// ===== BLINK =====
bool blinking = false;
int blinkToggles = 0;
int blinkTargetToggles = 0;
unsigned long nextBlinkToggle = 0;
const unsigned long blinkInterval = 300;

// ===== CALLBACK TOKEN =====
void tokenStatusCallback(FirebaseConfig *config) {
  Serial.println("🔑 Token actualizado correctamente.");
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(50);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(KY037_PIN, INPUT);
  digitalWrite(LED_ROJO, LOW);
  digitalWrite(LED_VERDE, LOW);
  noTone(BUZZER);

  dht.begin();

  // ===== Conexión WiFi =====
  Serial.print("Conectando a WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long startWiFi = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startWiFi < 15000) {
    Serial.print(".");
    delay(300);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi conectado correctamente.");
    Serial.print("IP local: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n⚠️ No se pudo conectar a la red WiFi.");
  }

  // ===== Sincronizar hora NTP =====
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  Serial.print("⏳ Sincronizando hora NTP");
  unsigned long start = millis();
  while (millis() - start < 10000) {
    time_t now = time(nullptr);
    if (now > 1600000000) break;
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n⏱️ Hora sincronizada correctamente.");

  // ===== Configurar Firebase =====
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("🔐 Inicio de sesión anónimo exitoso en Firebase!");
  } else {
    Serial.printf("❌ Error de autenticación: %s\n", config.signer.signupError.message.c_str());
  }

  ultimoEnvioFirebase = millis();
}

// ===== FUNCIONES =====

// 🟢 Mejora: Lectura filtrada del ultrasonido
int medirDistanciaFiltrada() {
  const int NUM_LECTURAS = 4;
  const int MAX_VALIDO = 400;
  const int MIN_VALIDO = 2;
  int lecturasValidas = 0;
  long suma = 0;

  for (int i = 0; i < NUM_LECTURAS; i++) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long dur = pulseIn(ECHO_PIN, HIGH, 25000);
    if (dur > 0) {
      int d = dur * 0.034 / 2;
      if (d >= MIN_VALIDO && d <= MAX_VALIDO) {
        suma += d;
        lecturasValidas++;
      }
    }
    delay(8);
  }

  if (lecturasValidas == 0) {
    return distanciaAnterior;
  }

  int promedio = suma / lecturasValidas;
  distanciaAnterior = promedio;
  return promedio;
}

// 🟢 Mejora: Lectura DHT más estable con reintento
void leerDHTnonBlocking() {
  unsigned long ahora = millis();
  if (ahora - ultimoDHT >= intervaloDHT) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      temp = t;
      hum = h;
    } else {
      Serial.println("⚠️ Error leyendo DHT, reintentando...");
      delay(100);
      t = dht.readTemperature();
      h = dht.readHumidity();
      if (!isnan(t)) temp = t;
      if (!isnan(h)) hum = h;
    }
    ultimoDHT = ahora;
  }
}

void leerKY037() {
  sonido = analogRead(KY037_PIN);
}

// ===== LED BLINK =====
void arrancarBlink(int cycles) {
  blinking = true;
  blinkToggles = 0;
  blinkTargetToggles = cycles * 2;
  nextBlinkToggle = millis();
  digitalWrite(LED_ROJO, LOW);
}

void handleBlink() {
  if (!blinking) return;
  unsigned long ahora = millis();
  if (ahora >= nextBlinkToggle) {
    digitalWrite(LED_ROJO, !digitalRead(LED_ROJO));
    blinkToggles++;
    nextBlinkToggle = ahora + blinkInterval;
    if (blinkToggles >= blinkTargetToggles) {
      blinking = false;
      digitalWrite(LED_ROJO, LOW);
    }
  }
}

// ===== BUZZER =====
void setBuzzerPattern(int freq, unsigned long onDur, unsigned long offDur) {
  buzzerFreq = freq;
  buzzerOnDuration = onDur;
  buzzerOffDuration = offDur;
  buzzerEnabled = true;
  buzzerOn = false;
  buzzerOffUntil = millis();
}

void stopBuzzer() {
  buzzerEnabled = false;
  buzzerOn = false;
  noTone(BUZZER);
}

void handleBuzzer() {
  if (!buzzerEnabled) return;
  unsigned long ahora = millis();
  if (buzzerOn) {
    if (ahora >= buzzerUntil) {
      noTone(BUZZER);
      buzzerOn = false;
      buzzerOffUntil = ahora + buzzerOffDuration;
    }
  } else {
    if (ahora >= buzzerOffUntil) {
      tone(BUZZER, buzzerFreq);
      buzzerOn = true;
      buzzerUntil = ahora + buzzerOnDuration;
    }
  }
}

// ===== ENVÍO FIREBASE =====
void enviarFirebaseIfNeeded(const String &estado, bool forceSend = false) {
  unsigned long ahora = millis();
  if (!forceSend && (ahora - ultimoEnvioFirebase < intervaloEnvio)) return;
  if (WiFi.status() != WL_CONNECTED || !Firebase.ready()) return;

  Serial.println("📡 Enviando datos a Firebase...");

  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);
  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", timeinfo);

  String basePath = "parking_status/";
  String logPath = "parking_logs/" + String(timestamp);

  FirebaseJson json;
  json.set("estado", estado);
  json.set("distancia", distancia);
  json.set("temperatura", temp);
  json.set("humedad", hum);
  json.set("nivel_sonido", sonido);
  json.set("timestamp", timestamp);

  bool okLive = Firebase.RTDB.setJSON(&fbdo, basePath.c_str(), &json);
  bool okHist = Firebase.RTDB.setJSON(&fbdo, logPath.c_str(), &json);

  if (okLive && okHist) {
    Serial.println("✅ Datos enviados correctamente a Firebase");
  } else {
    Serial.printf("❌ Error enviando datos: %s\n", fbdo.errorReason().c_str());
  }

  ultimoEnvioFirebase = ahora;
}

// ===== LOOP =====
void loop() {
  unsigned long ahora = millis();
  if (ahora - tiempoAnterior < intervaloLoop) {
    handleBlink();
    handleBuzzer();
    return;
  }
  tiempoAnterior = ahora;

  distancia = medirDistanciaFiltrada();
  leerDHTnonBlocking();
  leerKY037();

  String estado = "";

  if (distancia > 100 || distancia <= 0) {
    estado = "Libre";
    digitalWrite(LED_ROJO, LOW);
    digitalWrite(LED_VERDE, HIGH);
    stopBuzzer();
    blinking = false;
  } else if (distancia > 30 && distancia <= 100) {
    estado = "Aproximación";
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_ROJO, HIGH);
    int frecuencia = map(distancia, 100, 30, 800, 1200);
    int duracionOn = map(distancia, 100, 30, 120, 60);
    setBuzzerPattern(frecuencia, duracionOn, duracionOn);
  } else if (distancia <= 30) {
    if (contadorQuieto >= CICLOS_QUIETO) {
      estado = "Ocupado";
      digitalWrite(LED_VERDE, LOW);
      stopBuzzer();
      if (!alertaMostrada) {
        arrancarBlink(5);
        alertaMostrada = true;
      }
    } else {
      estado = "Maniobra";
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_ROJO, HIGH);
      setBuzzerPattern(1600, 150, 150);
    }
  }

  handleBlink();
  handleBuzzer();

  Serial.printf("📏 Dist: %d cm | 🌡 Temp: %.1f°C | 💧 Hum: %.1f%% | 🔊 Sonido: %d | 🚗 Estado: %s\n",
                distancia, temp, hum, sonido, estado.c_str());

  bool forceSend = (estado != ultimoEstado) &&
                   (millis() - ultimoEnvioFirebase >= minEnvioEntreCambios);
  enviarFirebaseIfNeeded(estado, forceSend);
  if (estado != ultimoEstado) ultimoEstado = estado;
}
