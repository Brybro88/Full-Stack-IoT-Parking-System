// ======================================================================
// SISTEMA DE ESTACIONAMIENTO - VERSIÓN FINAL DEFINITIVA (A PRUEBA DE FALLOS)
// ======================================================================

// --- LIBRERÍAS ---
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "DHT.h"
#include <time.h>
#include <algorithm>

// ==================================================
// 1. CONFIGURACIÓN DE RED Y FIREBASE
// ==================================================
const char* WIFI_SSID = "INFINITUM2CB5";
const char* WIFI_PASSWORD = "X7e4upMA4Y";
const char* API_KEY = "AIzaSyA7kOahrIAr_XImPq05EhbJ7CZSPATIZUs";
const char* DATABASE_URL = "https://parking-iot-a0b8e-default-rtdb.firebaseio.com/";

const char* FB_PATH_STATUS = "/Estacionamiento/status";
const char* FB_PATH_LOGS = "/Estacionamiento/logs";
const char* NTP_SERVER = "pool.ntrop.org";
const long GMT_OFFSET_SEC = -21600;
const int DAYLIGHT_OFFSET_SEC = 3600;

// ==================================================
// 2. PINES Y SENSORES
// ==================================================
#define DHTPIN 4
#define DHTTYPE DHT22
#define TRIG_PIN 5
#define ECHO_PIN 18
#define BUZZER_PIN 19
#define KY037_PIN 34

DHT dht(DHTPIN, DHTTYPE);

// ==================================================
// 3. UMBRALES Y TEMPORIZADORES (NO BLOQUEANTES)
// ==================================================
const unsigned long SENSOR_READ_INTERVAL = 500; // Leer sensores 2 veces por segundo
const unsigned long FIREBASE_SEND_INTERVAL = 15000;
const unsigned long WIFI_RECONNECT_INTERVAL = 20000;

const float DIST_LIBRE = 120.0;
const float DIST_OCUPADO = 35.0;
const int RUIDO_UMBRAL = 3500;
const int STABILIZATION_COUNT = 4;

// ==================================================
// 4. OBJETOS Y VARIABLES GLOBALES
// ==================================================
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastSensorRead = 0;
unsigned long lastFirebaseSend = 0;
unsigned long lastWifiCheck = 0;

float distancia_cm = -1.0; // Inicializar en -1 para saber si ya se leyó
float temperatura_c = -100.0;
float humedad_pct = -1.0;
int nivel_sonido = 0;
String estadoActual = "Inicializando";
String estadoAnterior = "";

int ocupadoCounter = 0;

// ==================================================
// 5. SETUP INICIAL
// ==================================================
void setup() {
    Serial.begin(115200);
    Serial.println("\n\n--- INICIANDO SISTEMA DE ESTACIONAMIENTO ---");

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(KY037_PIN, INPUT);

    dht.begin();
    
    iniciarWiFi();
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    iniciarFirebase();

    Serial.println("\n--- VERIFICACIÓN DE SENSORES INICIAL ---");
    // Hacemos una primera lectura para verificar el hardware
    leerSensores();
    if (temperatura_c == -100.0) {
        Serial.println("❌ ERROR CRÍTICO: No se pudo leer el sensor DHT22. Revisa el cableado.");
    } else {
        Serial.println("✅ Sensor DHT22 detectado correctamente.");
    }
    if (distancia_cm == -1.0) {
        Serial.println("❌ ERROR CRÍTICO: No se pudo leer el sensor HC-SR04. Revisa el cableado (VCC a 5V/VIN y divisor de voltaje).");
    } else {
        Serial.println("✅ Sensor HC-SR04 detectado correctamente.");
    }

    Serial.println("-------------------------------------------");
    Serial.println("🚀 Sistema listo para operar. Iniciando loop...");
    Serial.println("-------------------------------------------");
}

// ==================================================
// 6. BUCLE PRINCIPAL (CON DIAGNÓSTICO)
// ==================================================
void loop() {
    unsigned long now = millis();
    
    // --- DIAGNÓSTICO: Heartbeat del Loop ---
    // Si ves esto, el loop está corriendo y no está bloqueado.
    Serial.print("."); 

    // Tarea 1: Asegurar WiFi
    if (now - lastWifiCheck > WIFI_RECONNECT_INTERVAL) {
        lastWifiCheck = now;
        if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
    }

    // Tarea 2: Leer Sensores
    if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
        lastSensorRead = now;
        Serial.println("\n--- Ciclo de Lectura ---");

        estadoAnterior = estadoActual;
        leerSensores();
        estadoActual = decidirEstado(distancia_cm);
        
        // --- Tarea de Diagnóstico: Mostrar lecturas ---
        Serial.printf("📏 Dist: %.1f cm | 🌡️ Temp: %.1f°C | 💧 Hum: %.1f%% | 🎤 Sonido: %d | 🅿️ Estado: %s\n",
                      distancia_cm, temperatura_c, humedad_pct, nivel_sonido, estadoActual.c_str());

        manejarBuzzer(estadoActual, estadoAnterior, nivel_sonido);
    }

    // Tarea 3: Enviar a Firebase
    bool estadoCambiado = (estadoActual != estadoAnterior && estadoActual != "Inicializando");
    bool tiempoDeEnvio = (now - lastFirebaseSend >= FIREBASE_SEND_INTERVAL);

    if ((estadoCambiado || tiempoDeEnvio) && Firebase.ready() && WiFi.status() == WL_CONNECTED) {
        lastFirebaseSend = now;
        enviarAFirebase(estadoCambiado);
    }
}

// ==================================================
// 7. FUNCIONES AUXILIARES (Robustas)
// ==================================================

void iniciarWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("🔌 Conectando a WiFi");
    int intentos = 0;
    while (WiFi.status() != WL_CONNECTED && intentos < 30) {
        delay(500);
        Serial.print(".");
        intentos++;
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n❌ FATAL: No se pudo conectar a WiFi. Reiniciando...");
        delay(1000); ESP.restart();
    }
    Serial.println("\n✅ WiFi Conectado: " + WiFi.localIP().toString());
}

void iniciarFirebase() {
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    Firebase.reconnectWiFi(true);
    Firebase.begin(&config, &auth);
    Serial.println("🔥 Conexión con Firebase configurada.");
}

void leerSensores() {
    Serial.println("-> Leyendo DHT22...");
    float temp_val = dht.readTemperature();
    float hum_val = dht.readHumidity();
    if (!isnan(temp_val)) temperatura_c = temp_val;
    if (!isnan(hum_val)) humedad_pct = hum_val;

    Serial.println("-> Leyendo HC-SR04...");
    float dist_filtrada = leerDistanciaFiltrada();
    if (dist_filtrada != -1.0) {
        if (distancia_cm == -1.0) distancia_cm = dist_filtrada; // Primera lectura
        else distancia_cm = (distancia_cm * 0.7) + (dist_filtrada * 0.3);
    }
    
    Serial.println("-> Leyendo KY-037...");
    nivel_sonido = analogRead(KY037_PIN);
    Serial.println("-> Lectura de sensores completa.");
}

float leerDistanciaFiltrada() {
    long duracion;
    float lecturas[3];
    for (int i = 0; i < 3; i++) {
        digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
        digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
        digitalWrite(TRIG_PIN, LOW);
        
        // pulseIn con timeout para EVITAR BLOQUEOS
        duracion = pulseIn(ECHO_PIN, HIGH, 25000); // Timeout de 25ms
        
        if (duracion > 0) {
            lecturas[i] = duracion * 0.0343 / 2.0;
        } else {
            lecturas[i] = -1.0; // Marcar como lectura fallida
        }
        delay(20);
    }
    std::sort(lecturas, lecturas + 3);
    return lecturas[1]; // Devolver la mediana
}

String decidirEstado(float dist) {
    if (dist == -1.0 || dist > DIST_LIBRE) {
        ocupadoCounter = 0;
        return "Libre";
    }
    if (dist > DIST_OCUPADO) {
        ocupadoCounter = 0;
        return "Aproximacion";
    }
    if (ocupadoCounter < STABILIZATION_COUNT) {
        ocupadoCounter++;
        return "Maniobra";
    }
    return "Ocupado";
}

void manejarBuzzer(const String& estado, const String& estadoPrevio, int ruido) {
    bool cambioHaciaOcupado = (estado == "Aproximacion" && estadoPrevio == "Libre");
    if (cambioHaciaOcupado) tone(BUZZER_PIN, 1200, 150);
    else if (ruido > RUIDO_UMBRAL) tone(BUZZER_PIN, 2500, 500);
}

String getTimestamp() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "N/A";
    char buf[30];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return String(buf);
}

void enviarAFirebase(bool enviarLog) {
    FirebaseJson json;
    json.set("timestamp", getTimestamp());
    json.set("estado", estadoActual);
    json.set("distancia_cm", String(distancia_cm, 1));
    json.set("temperatura_c", String(temperatura_c, 1));
    json.set("humedad_pct", String(humedad_pct, 1));
    json.set("nivel_sonido", nivel_sonido);

    Serial.println("📤 Enviando datos a Firebase...");
    if (Firebase.RTDB.setJSON(&fbdo, FB_PATH_STATUS, &json)) {
        Serial.println("✅ Estado actual actualizado en /status.");
    } else {
        Serial.println("❌ Error al enviar estado: " + fbdo.errorReason());
    }
    if (enviarLog) {
        if (Firebase.RTDB.pushJSON(&fbdo, FB_PATH_LOGS, &json)) {
            Serial.println("🗃️ ¡Cambio de estado! Log guardado en /logs.");
        }
    }
}