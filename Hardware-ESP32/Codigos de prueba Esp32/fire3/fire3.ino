#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "DHT.h"
#include <time.h>

// ===== CONFIGURACIÓN DE RED =====
#define WIFI_SSID "IBARRA 25"
#define WIFI_PASSWORD "COSMO3625"

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
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// ===== VARIABLES SENSOR/LOGICA =====
long duracion;
int distancia = 999;
int distanciaAnterior = 999;
int contadorQuieto = 0;

const int UMBRAL_MOV = 4;
const int CICLOS_QUIETO = 5;
bool alertaMostrada = false;

float temp = NAN;
float hum = NAN;
unsigned long ultimoDHT = 0;
const unsigned long intervaloDHT = 2000; // 2s

unsigned long tiempoAnterior = 0;
const unsigned long intervaloLoop = 100; // base loop cada 100ms

String ultimoEstado = "";

// ===== TELEMETRIA / FIREBASE =====
unsigned long ultimoEnvioFirebase = 0;
const unsigned long intervaloEnvio = 5000; // enviar telemetría cada 5s
const unsigned long minEnvioEntreCambios = 2000; // throttle (2s)

// ===== BUZZER NO BLOQUEANTE =====
bool buzzerEnabled = false;
bool buzzerOn = false;
unsigned long buzzerUntil = 0;
unsigned long buzzerOffUntil = 0;
unsigned long buzzerOnDuration = 0;
unsigned long buzzerOffDuration = 0;
int buzzerFreq = 1000;

// ===== BLINK OCCUPIED NO BLOQUEANTE =====
bool blinking = false;
int blinkToggles = 0;
int blinkTargetToggles = 0; // 2 toggles = 1 full cycle
unsigned long nextBlinkToggle = 0;
const unsigned long blinkInterval = 300; // ms

// ===== FUNCION CALLBACK TOKEN (opcional para debug) =====
void tokenStatusCallback(FirebaseConfig *config) {
  Serial.println("Token status changed.");
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
  digitalWrite(LED_ROJO, LOW);
  digitalWrite(LED_VERDE, LOW);
  noTone(BUZZER);

  dht.begin();

  // Conectar WiFi
  Serial.print("Conectando a WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long startWiFi = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startWiFi < 15000) {
    Serial.print(".");
    delay(300);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n⚠️ No se pudo conectar a WiFi en 15s. Seguirá intentando en background.");
  }

  // Sincronizar hora (NTP)
  Serial.print("Sincronizando hora NTP");
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  unsigned long start = millis();
  while (millis() - start < 10000) {
    time_t now = time(nullptr);
    if (now > 1600000000) break;
    Serial.print(".");
    delay(500);
  }

  time_t now = time(nullptr);
  if (now > 1600000000) {
    Serial.println("\n✅ Hora sincronizada");
  } else {
    Serial.println("\n⚠️ No se sincronizó la hora. SSL puede fallar (revisar NTP/WiFi).");
  }

  // Configurar Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // SignUp anónimo
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("🔐 Inicio de sesión anónimo exitoso!");
  } else {
    Serial.printf("❌ Error en inicio de sesión: %s\n", config.signer.signupError.message.c_str());
  }

  ultimoEnvioFirebase = millis();
}

// ===== FUNCIONES AUXILIARES =====
int medirDistanciaNonBlocking() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long dur = pulseIn(ECHO_PIN, HIGH, 30000);
  if (dur == 0) {
    Serial.println("⚠️ No se recibió eco del HC-SR04");
    return distanciaAnterior;
  }
  int d = (int)(dur * 0.034 / 2.0);
  return d;
}

void leerDHTnonBlocking() {
  unsigned long ahora = millis();
  if (ahora - ultimoDHT >= intervaloDHT) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      temp = t;
      hum = h;
    } else {
      Serial.println("⚠️ Lectura DHT fallida (NaN). Manteniendo últimos valores.");
    }
    ultimoDHT = ahora;
  }
}

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
    bool current = digitalRead(LED_ROJO);
    digitalWrite(LED_ROJO, !current);
    blinkToggles++;
    nextBlinkToggle = ahora + blinkInterval;
    if (blinkToggles >= blinkTargetToggles) {
      blinking = false;
      digitalWrite(LED_ROJO, LOW);
    }
  }
}

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

void enviarFirebaseIfNeeded(const String &estado, bool forceSend = false) {
  unsigned long ahora = millis();
  if (!forceSend && (ahora - ultimoEnvioFirebase < intervaloEnvio)) 
  return;
/parking_status/          → Estado actual (actualizable en tiempo real)
/parking_logs/2025-10-05_17-22-30 → Registro histórico con fecha


  Serial.println("📡 Enviando datos a Firebase...");

  bool okEstado = Firebase.RTDB.setString(&fbdo, "parking_status/estado", estado);
  if (!okEstado)
    Serial.printf("❌ Error estado -> httpCode: %d, reason: %s\n", fbdo.httpCode(), fbdo.errorReason().c_str());
  else
    Serial.println("✅ Estado enviado");

  bool okDist = Firebase.RTDB.setFloat(&fbdo, "parking_status/distancia", distancia);
  if (!okDist)
    Serial.printf("❌ Error distancia -> httpCode: %d, reason: %s\n", fbdo.httpCode(), fbdo.errorReason().c_str());
  else
    Serial.println("✅ Distancia enviada");

  if (!isnan(temp)) {
    bool okTemp = Firebase.RTDB.setFloat(&fbdo, "parking_status/temperatura", temp);
    if (!okTemp)
      Serial.printf("❌ Error temperatura -> httpCode: %d, reason: %s\n", fbdo.httpCode(), fbdo.errorReason().c_str());
    else
      Serial.println("✅ Temperatura enviada");
  }

  if (!isnan(hum)) {
    bool okHum = Firebase.RTDB.setFloat(&fbdo, "parking_status/humedad", hum);
    if (!okHum)
      Serial.printf("❌ Error humedad -> httpCode: %d, reason: %s\n", fbdo.httpCode(), fbdo.errorReason().c_str());
    else
      Serial.println("✅ Humedad enviada");
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

  // Lecturas no bloqueantes
  distancia = medirDistanciaNonBlocking();

  // Detección de movimiento
  if (abs(distancia - distanciaAnterior) <= UMBRAL_MOV) {
    contadorQuieto++;
  } else {
    contadorQuieto = 0;
    alertaMostrada = false;
  }
  distanciaAnterior = distancia;

  leerDHTnonBlocking();

  // Determinar estado
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
    int duracionOff = duracionOn;
    setBuzzerPattern(frecuencia, duracionOn, duracionOff);

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

  // Serial
  Serial.print("Distancia: ");
  Serial.print(distancia);
  Serial.print(" cm | Estado: ");
  Serial.print(estado);
  if (!isnan(temp) && !isnan(hum)) {
    Serial.print(" | Temp: ");
    Serial.print(temp);
    Serial.print(" °C | Hum: ");
    Serial.print(hum);
    Serial.print(" %");
  }
  Serial.println();

  // Enviar a Firebase
  bool forceSend = (estado != ultimoEstado) && (millis() - ultimoEnvioFirebase >= minEnvioEntreCambios);
  if (forceSend) enviarFirebaseIfNeeded(estado, true);
  else enviarFirebaseIfNeeded(estado, false);

  if (estado != ultimoEstado) ultimoEstado = estado;
}
