#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <DHT.h>
#include <time.h>

// --- TUS CREDENCIALES ---
#define WIFI_SSID "ARTR 2579"
#define WIFI_PASSWORD "11111111"
#define API_KEY "AIzaSyA7kOahrIAr_XImPq05EhbJ7CZSPATIZUs"
#define DATABASE_URL "https://parking-iot-a0b8e-default-rtdb.firebaseio.com/"

// --- PINES ---
#define TRIG_PIN 5
#define ECHO_PIN 18
#define LED_VERDE 22
#define LED_ROJO 23
#define BUZZER 21
#define DHTPIN 4
#define DHTTYPE DHT22
#define KY037_PIN 34

// --- UMBRALES Y CONSTANTES ---
const int UMBRAL_RUIDO = 2700;
const int DISTANCIA_LIBRE = 120;
const int DISTANCIA_OCUPADO = 35;
const int ESTABILIZACION_COUNT = 5;
const unsigned long INTERVALO_DHT = 2500;
const unsigned long INTERVALO_ENVIO_BASE = 5000;
const unsigned long MIN_ENVIO_ENTRE_CAMBIOS = 2000;
const int ULTRASOUND_N_READINGS = 7;
const int ULTRASOUND_DELAY = 10;

// --- OBJETOS GLOBALES ---
FirebaseData fbdo;
FirebaseData fbdo_verifier;
FirebaseAuth auth;
FirebaseConfig config;
DHT dht(DHTPIN, DHTTYPE);

// --- VARIABLES DE ESTADO ---
int distancia_cm = 999;
int distanciaAnterior_cm = 999;
float temp_c = NAN, hum_pct = NAN;
int nivel_sonido = 0;
String estadoActual = "Inicializando";
String ultimoEstadoEnviado = "";
int estadoEstableContador = 0;
unsigned long ultimoEnvioFirebase = 0;
unsigned long ultimoDHT = 0;
unsigned long ultimoCambioEstado = 0;

// --- PROTOTIPOS ---
void ensureWiFi();
void ensureFirebase();
void syncTime();
int medirDistanciaFiltrada();
void leerDHT();
bool ruidoExcesivo();
void buzzerBeep(int freq, int durMs);
String decidirEstado(int dist);
void verificarDatoEnFirebase(String path, String key, String expectedValue);
void enviarFirebase(String estado);


// ===================== IMPLEMENTACIÓN DE FUNCIONES =====================

void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.print("🔁 Conectando a WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start < 10000)) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Conectado. IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n❌ Falló WiFi. Reintentando...");
  }
}

// 💡 FUNCIÓN NUEVA para sincronizar la hora (diagnóstico clave)
void syncTime() {
    Serial.print("🕒 Sincronizando hora con servidor NTP...");
    configTime(0, 0, "pool.ntp.org");
    time_t now = time(nullptr);
    int retries = 0;
    while (now < 8 * 3600 * 2 && retries < 10) { // Espera hasta que la hora sea válida
        delay(500);
        now = time(nullptr);
        Serial.print(".");
        retries++;
    }
    if (now > 8 * 3600 * 2) {
        Serial.println("\n✅ Hora sincronizada correctamente.");
    } else {
        Serial.println("\n❌ Falló la sincronización NTP. ¿Hay conexión a Internet?");
    }
}

void ensureFirebase() {
  if (Firebase.ready()) return;
  
  Serial.print("🔥 Inicializando Firebase...");
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = ""; 
  auth.user.password = "";
  Firebase.reconnectWiFi(true);
  
  Firebase.begin(&config, &auth);
  
  if (Firebase.ready()) {
    Serial.println("\n✅ Firebase Listo.");
  } else {
    Serial.println("\n❌ Falló Firebase. Verifique credenciales y reglas de la base de datos.");
  }
}

// (Las demás funciones como medirDistanciaFiltrada, leerDHT, etc. se mantienen igual)

int medirDistanciaFiltrada() {
  const int MIN_VALID = 5;
  const int MAX_VALID = 400; 
  long suma = 0;
  int valid_count = 0;

  for (int i = 0; i < ULTRASOUND_N_READINGS; i++) {
    digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duracion = pulseIn(ECHO_PIN, HIGH, 30000); 
    int d = (duracion * 0.034) / 2;

    if (d >= MIN_VALID && d <= MAX_VALID) {
      suma += d;
      valid_count++;
    }
    delay(ULTRASOUND_DELAY);
  }

  if (valid_count == 0) return distanciaAnterior_cm;
  
  int nuevaDistancia = suma / valid_count;
  if (abs(nuevaDistancia - distanciaAnterior_cm) > 150) { 
      nuevaDistancia = distanciaAnterior_cm;
  }
  distanciaAnterior_cm = nuevaDistancia;
  return nuevaDistancia;
}

void leerDHT() {
  unsigned long now = millis();
  if (now - ultimoDHT < INTERVALO_DHT) return;
  ultimoDHT = now;

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t)) {
      Serial.println("⚠️ DHT: Falló lectura de Temp. Revisa cableado y resistencia pull-up.");
  } else {
      temp_c = t;
  }

  if (isnan(h)) {
      Serial.println("⚠️ DHT: Falló lectura de Hum. Revisa cableado.");
  } else {
      hum_pct = h;
  }
}

bool ruidoExcesivo() {
  nivel_sonido = analogRead(KY037_PIN);
  return (nivel_sonido > UMBRAL_RUIDO);
}

void buzzerBeep(int freq, int durMs) {
  static unsigned long buzzerStartTime = 0;
  if (millis() - buzzerStartTime < durMs) return; 
  tone(BUZZER, freq, durMs); 
  buzzerStartTime = millis();
}

String decidirEstado(int dist) {
  String nuevoEstado;
  if (dist > DISTANCIA_LIBRE) {
    nuevoEstado = "Libre";
    estadoEstableContador = 0;
  } else if (dist > DISTANCIA_OCUPADO && dist <= DISTANCIA_LIBRE) {
    nuevoEstado = "Aproximacion";
    estadoEstableContador = 0;
  } else {
    if (abs(dist - distanciaAnterior_cm) > 5) { 
      nuevoEstado = "Maniobra";
      estadoEstableContador = 0;
    } else {
      estadoEstableContador++;
      if (estadoEstableContador >= ESTABILIZACION_COUNT) {
        nuevoEstado = "Ocupado";
      } else {
        nuevoEstado = "Maniobra"; 
      }
    }
  }
  if (nuevoEstado != estadoActual) ultimoCambioEstado = millis();
  estadoActual = nuevoEstado;
  return estadoActual;
}

void verificarDatoEnFirebase(String path, String key, String expectedValue) {
  if (!Firebase.ready()) return;
  if (Firebase.RTDB.getString(&fbdo_verifier, path + "/" + key)) {
    if (fbdo_verifier.stringData() == expectedValue) {
      Serial.print("✅ Verificado. ");
    } else {
      Serial.printf("⚠️ Fallo Verif: %s != %s. ", expectedValue.c_str(), fbdo_verifier.stringData().c_str());
    }
  } else {
    Serial.print("⚠️ Fallo Lectura Verif. ");
  }
}

void enviarFirebase(String estado) {
  if (WiFi.status() != WL_CONNECTED || !Firebase.ready()) {
    Serial.println("❌ Firebase/WiFi no listo para enviar.");
    return;
  }

  Serial.print("📡 Enviando a Firebase... ");
  time_t now = time(nullptr);
  if (now < 8 * 3600 * 2) {
    Serial.println("❌ Hora no sincronizada, envío cancelado.");
    return;
  }

  char ts[30];
  strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));

  FirebaseJson json;
  json.set("estado", estado.c_str());
  json.set("distancia_cm", distancia_cm);
  json.set("temperatura_c", String(temp_c, 1).c_str());
  json.set("humedad_pct", String(hum_pct, 1).c_str());
  json.set("nivel_sonido", nivel_sonido);
  json.set("timestamp", ts);

  String pathStatus = "/parking_status";
  if (Firebase.RTDB.setJSON(&fbdo, pathStatus.c_str(), &json)) {
    Serial.print("Status OK. ");
    verificarDatoEnFirebase(pathStatus, "estado", estado); 
  } else {
    Serial.printf("Status FALLÓ. Razón: %s\n", fbdo.errorReason().c_str());
  }

  String pathHist = "/parking_logs/" + String(ts);
  if (Firebase.RTDB.setJSON(&fbdo, pathHist.c_str(), &json)) {
    Serial.println("Historial OK.");
  } else {
    Serial.printf("Historial FALLÓ. Razón: %s\n", fbdo.errorReason().c_str());
  }
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  pinMode(LED_VERDE, OUTPUT); pinMode(LED_ROJO, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(KY037_PIN, INPUT);

  dht.begin();
  
  ensureWiFi();
  if(WiFi.status() == WL_CONNECTED) {
    syncTime();
    ensureFirebase();
  } else {
    Serial.println("No hay WiFi, saltando conexión a Firebase.");
  }
  
  Serial.println("🚀 Sistema listo");
}

// ===================== LOOP =====================
void loop() {
  ensureWiFi(); 

  distancia_cm = medirDistanciaFiltrada();
  leerDHT();
  bool alarma = ruidoExcesivo();
  String estado = decidirEstado(distancia_cm);

  if (estado == "Libre") {
    digitalWrite(LED_VERDE, HIGH);
    digitalWrite(LED_ROJO, LOW);
  } else {
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_ROJO, HIGH);
  }

  if (estado == "Aproximacion" || alarma) buzzerBeep(1200, 100); 

  Serial.printf("📏 %d cm | 🌡 %.1f°C | 💧 %.1f%% | 🔊 %d | Estado: %s\n",
                distancia_cm, temp_c, hum_pct, nivel_sonido, estado.c_str());

  unsigned long now = millis();
  bool cambioSignificativo = (estado != ultimoEstadoEnviado);
  
  if (now - ultimoEnvioFirebase > INTERVALO_ENVIO_BASE || cambioSignificativo) {
    // Re-chequear Firebase antes de enviar por si se desconectó
    if(!Firebase.ready()) ensureFirebase();

    if (cambioSignificativo && (now - ultimoEnvioFirebase < MIN_ENVIO_ENTRE_CAMBIOS)) {
      delay(MIN_ENVIO_ENTRE_CAMBIOS - (now - ultimoEnvioFirebase));
    }
    
    enviarFirebase(estado);
    ultimoEnvioFirebase = millis(); 
    ultimoEstadoEnviado = estado;
  }

  delay(200); 
}