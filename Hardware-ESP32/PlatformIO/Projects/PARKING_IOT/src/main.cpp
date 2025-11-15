// ======================================================================
// VERSIÓN FINAL CORREGIDA (CON DOS OBJETOS FBDO)
// ======================================================================

// --- LIBRERÍAS ---
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "DHT.h"
#include <time.h>
#include <algorithm>
// 1. CONFIGURACIÓN DE RED Y FIREBASE
const char* WIFI_SSID = "INFINITUM2CB5";
const char* WIFI_PASSWORD = "X7e4upMA4Y";
const char* DATABASE_SECRET = "fvNtqyqklwhW3Zy35ed352460uvlbgPPJLf8XOkD";
const char* DATABASE_URL ="https://parking-iot-a0b8e-default-rtdb.firebaseio.com/";

const char* FB_PATH_STATUS = "/parking_status";
const char* FB_PATH_LOGS = "/parking_logs";

const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = 0; 
const int DAYLIGHT_OFFSET_SEC = 0; 

// 2. PINES Y SENSORES
#define DHTPIN 4
#define DHTTYPE DHT22
#define TRIG_PIN 5
#define ECHO_PIN 18
#define KY037_PIN 34
#define BUZZER_PIN 21
#define LED_VERDE 22
#define LED_ROJO 23

DHT dht(DHTPIN, DHTTYPE);

// 3. UMBRALES Y TEMPORIZADORES
const unsigned long SENSOR_READ_INTERVAL = 500;
const unsigned long FIREBASE_SEND_INTERVAL = 15000;
const unsigned long WIFI_RECONNECT_INTERVAL = 20000;

const float DIST_OCUPADO = 50.0;
const float DIST_LIBRE = 120.0;
const int RUIDO_UMBRAL = 1500;

// 4. OBJETOS Y VARIABLES GLOBALES
FirebaseData fbdo_status; // <-- ¡CAMBIO #1A! Renombrado a fbdo_status
FirebaseData fbdo_logs;   // <-- ¡CAMBIO #1B! Añadido segundo objeto para logs
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastSensorRead = 0;
unsigned long lastFirebaseSend = 0;
unsigned long lastWifiCheck = 0;

float distancia_cm = -1.0;
float distanciaAnterior_cm = -1.0;
float temperatura_c = -100.0;
float humedad_pct = -1.0;
int nivel_sonido = 0;

const int RUIDO_MUESTRAS = 60;
int lecturasRuido[RUIDO_MUESTRAS];
int indiceRuido = 0;
long sumaRuido = 0;
float ruidoBaselineAvg = 0.0;
String estadoRuido = "Normal";
String estadoActual = "Inicializando";
String estadoAnterior = "";

bool alarmaRuidoActiva = false;
unsigned long alarmaRuidoTimestamp = 0;
const unsigned long ALARMA_RUIDO_DURACION = 500;

// --- DECLARACIÓN DE PROTOTIPOS DE FUNCIONES ---
void iniciarWiFi();
void iniciarFirebase();
void leerSensores();
float leerDistanciaFiltrada_Promedio();
String decidirEstado(float dist);
void manejarHardwareFijo(const String& estado, int ruido);
void manejarBuzzer(const String& estado, float dist);
String getTimestampLocal();
void enviarAFirebase();
void analizarRuido();

// 5. SETUP INICIAL
void setup() {
    Serial.begin(115200);
    Serial.println("\n\n--- INICIANDO SISTEMA (PlataformIO) ---");

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(KY037_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_VERDE, OUTPUT);
    pinMode(LED_ROJO, OUTPUT);

    dht.begin();
    for (int i = 0; i < RUIDO_MUESTRAS; i++) {
        lecturasRuido[i] = 50;
    }
    sumaRuido = 50 * RUIDO_MUESTRAS;
    ruidoBaselineAvg = 50.0;
    
    iniciarWiFi();

    Serial.print("🕒 Sincronizando hora (NTP)");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo)) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("\n✅ Hora sincronizada correctamente.");

    iniciarFirebase();

    Serial.println("-------------------------------------------");
    Serial.println("🚀 Sistema listo para operar. Iniciando loop...");
    Serial.println("-------------------------------------------");
}

// 6. BUCLE PRINCIPAL (NO BLOQUEANTE)
void loop() {
    unsigned long now = millis();

    // Tarea 1: Asegurar WiFi
    if (now - lastWifiCheck > WIFI_RECONNECT_INTERVAL) {
        lastWifiCheck = now;
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("🔌 WiFi desconectado. Intentando reconectar...");
            WiFi.reconnect();
        }
    }

    // Tarea 2: Leer Sensores y Lógica de Estado
    if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
        lastSensorRead = now;

        estadoAnterior = estadoActual;
        distanciaAnterior_cm = distancia_cm; 

        leerSensores();
        analizarRuido();
        estadoActual = decidirEstado(distancia_cm); 

        Serial.printf("📏 Dist: %.1f cm | 🌡 Temp: %.1f°C | 💧 Hum: %.1f%% | 🎤 Sonido: %d (Avg: %.0f) | 🅿️ Estado: %s | 🔊 Ruido: %s\n",
                      distancia_cm, temperatura_c, humedad_pct, nivel_sonido, ruidoBaselineAvg, estadoActual.c_str(), estadoRuido.c_str());

        manejarHardwareFijo(estadoActual, nivel_sonido);
    }

    // Tarea 3: Manejar Buzzer
    manejarBuzzer(estadoActual, distancia_cm);

    // Tarea 4: Enviar a Firebase
    bool estadoCambiado = (estadoActual != estadoAnterior && estadoActual != "Inicializando");
    bool tiempoDeEnvio = (now - lastFirebaseSend >= FIREBASE_SEND_INTERVAL);

    if ((estadoCambiado || tiempoDeEnvio) && Firebase.ready() && WiFi.status() == WL_CONNECTED) {
        lastFirebaseSend = now;
        enviarAFirebase(); 
    } else if ((estadoCambiado || tiempoDeEnvio) && !Firebase.ready()) {
         Serial.println("❌ ERROR: Intento de envío fallido. Firebase.ready() es FALSO.");
         Serial.println("   Revisa REGLAS, API_KEY o DATABASE_URL.");
         lastFirebaseSend = now;
    }
}

// 7. DEFINICIÓN DE FUNCIONES

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
    Serial.print("\n✅ WiFi Conectado: ");
    Serial.println( WiFi.localIP().toString());
}

void iniciarFirebase() {
    config.signer.tokens.legacy_token = DATABASE_SECRET;
    config.database_url = DATABASE_URL;
    Firebase.reconnectWiFi(true);
    Firebase.begin(&config, &auth);
    Serial.println("🔥 Intentando conexión con DATABASE SECRET...");

    if (Firebase.ready()) {
         Serial.println("🔥 Conexión con Firebase configurada y lista.");
    } else {
         Serial.println("❌ ERROR FATAL al iniciar Firebase. Revisa credenciales y REGLAS.");
        Serial.print("     Razón: ");
        // Usamos fbdo_status (o cualquier fbdo) para leer el error de inicio
        Serial.println(fbdo_status.errorReason()); 
    }
}

void leerSensores() {
    float temp_val = dht.readTemperature();
    float hum_val = dht.readHumidity();
    if (!isnan(temp_val)) temperatura_c = temp_val;
    if (!isnan(hum_val)) humedad_pct = hum_val;

    float dist_filtrada = leerDistanciaFiltrada_Promedio(); 
    if (dist_filtrada != -1.0) {
        distancia_cm = dist_filtrada;
    } else {
        distancia_cm = distanciaAnterior_cm;
    }
    
    sumaRuido = sumaRuido - lecturasRuido[indiceRuido];
    nivel_sonido = analogRead(KY037_PIN);
    lecturasRuido[indiceRuido] = nivel_sonido;
    sumaRuido = sumaRuido + nivel_sonido;
    indiceRuido = (indiceRuido + 1) % RUIDO_MUESTRAS;
    ruidoBaselineAvg = (float)sumaRuido / RUIDO_MUESTRAS;
}

float leerDistanciaFiltrada_Promedio() {
    const int N = 5;
    const int MIN_VALID = 5;
    const int MAX_VALID = 300;
    int valid = 0;
    long suma = 0;

    for (int i = 0; i < N; i++) {
        digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
        digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
        digitalWrite(TRIG_PIN, LOW);

        long duracion = pulseIn(ECHO_PIN, HIGH, 25000);
        int d = duracion * 0.034 / 2;

        if (d >= MIN_VALID && d <= MAX_VALID) {
            suma += d;
            valid++;
        }
        delay(10);
    }

    if (valid == 0) return -1.0;
    return (float)suma / valid;
}

String decidirEstado(float dist) {
    static int contadorEstable = 0;
    String estado;

    if (dist >= 400 || dist <= 0) {
        dist = distanciaAnterior_cm;
    }

    if (dist > DIST_LIBRE) {
        estado = "Libre";
        contadorEstable = 0;
    } else if (dist > DIST_OCUPADO && dist <= DIST_LIBRE) {
        estado = "Aproximacion";
        contadorEstable = 0;
    } else { 
        if (abs(dist - distanciaAnterior_cm) <= 3.0) {
            contadorEstable++;
        } else {
            contadorEstable = 0;
        }

        if (contadorEstable >= 3) {
            estado = "Ocupado";
        } else {
            estado = "Maniobra";
        }
    }
    return estado;
}

void manejarHardwareFijo(const String& estado, int ruido) {
    if (estado == "Libre") {
        digitalWrite(LED_VERDE, HIGH);
        digitalWrite(LED_ROJO, LOW);
    } else {
        digitalWrite(LED_VERDE, LOW);
        digitalWrite(LED_ROJO, HIGH);
    }
    if (ruido > RUIDO_UMBRAL && !alarmaRuidoActiva) {
        alarmaRuidoActiva = true;
        alarmaRuidoTimestamp = millis();
    }
}

void analizarRuido() {
    if (nivel_sonido > RUIDO_UMBRAL) {
        estadoRuido = "Alerta";
    } else if (nivel_sonido > (ruidoBaselineAvg * 2.5) && nivel_sonido > 300) {
        estadoRuido = "Elevado";
    } else if (nivel_sonido < 100) {
        estadoRuido = "Bajo";
    } else {
        estadoRuido = "Normal";
    }
}

void manejarBuzzer(const String& estado, float dist) {
    static unsigned long lastBeepTime = 0;
    static bool isBuzzerOn = false;
    unsigned long now = millis();

    if (alarmaRuidoActiva) {
        if (now - alarmaRuidoTimestamp > ALARMA_RUIDO_DURACION) {
            alarmaRuidoActiva = false;
            noTone(BUZZER_PIN);
            isBuzzerOn = false;
        } else {
            if (!isBuzzerOn) {
                tone(BUZZER_PIN, 3000);
                isBuzzerOn = true;
            }
        }
        return;
    }

    if (estado != "Aproximacion" && estado != "Maniobra") {
        if (isBuzzerOn) {
            noTone(BUZZER_PIN);
            isBuzzerOn = false;
        }
        return;
    }

    float dist_constrain = constrain(dist, 0, DIST_LIBRE);
    long intervalo_silencio = map(dist_constrain, DIST_LIBRE, 0, 700, 50);
    long duracion_beep = map(dist_constrain, DIST_LIBRE, 0, 50, 200);
    long freq = map(dist_constrain, DIST_LIBRE, 0, 500, 2500);

    if (isBuzzerOn) {
        if (now - lastBeepTime > duracion_beep) {
            noTone(BUZZER_PIN);
            isBuzzerOn = false;
            lastBeepTime = now;
        }
    } else {
        if (now - lastBeepTime > intervalo_silencio) {
            tone(BUZZER_PIN, freq);
            isBuzzerOn = true;
            lastBeepTime = now;
        }
    }
}

String getTimestampLocal() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "2025-01-01T00:00:00Z"; // Fallback en formato ISO
    }
    char buf[30];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return String(buf);
}

// Reemplaza tu función anterior con esta:
void enviarAFirebase() {
    FirebaseJson json;
    String ts_local = getTimestampLocal();

    json.set("timestamp", ts_local);
    json.set("estado", estadoActual);
    json.set("distancia_cm", String(distancia_cm, 1));
    json.set("temperatura_c", String(temperatura_c, 1));
    json.set("humedad_pct", String(humedad_pct, 1));
    json.set("nivel_sonido", nivel_sonido);
    json.set("estado_ruido", estadoRuido);
    json.set("ruido_baseline", String(ruidoBaselineAvg, 0));

    Serial.println("📤 Enviando datos a Firebase...");

    // --- ¡CAMBIO #2A! Usar el objeto fbdo_status para /status
    if (Firebase.RTDB.setJSON(&fbdo_status, FB_PATH_STATUS, &json)) {
        Serial.println("✅ Estado actual actualizado.");
    } else {
        Serial.print("❌ Error al enviar estado: ");
        Serial.println(fbdo_status.errorReason());
    }

    // --- ¡CAMBIO #2B! Usar el objeto fbdo_logs para /logs
    if (Firebase.RTDB.pushJSON(&fbdo_logs, FB_PATH_LOGS, &json)) {
        Serial.println("🗃️ Log de datos guardado en Firebase.");
    } else {
      Serial.print("❌ Error al enviar log: ");
      Serial.println(fbdo_logs.errorReason());
    }
}