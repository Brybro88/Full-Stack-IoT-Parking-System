// ======================================================================
// VERSIÓN HÍBRIDA (CON ALARMA DE PROXIMIDAD PROPORCIONAL)
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
// !! USA TUS CREDENCIALES ACTUALES !!
const char* WIFI_SSID = "INFINITUM2CB5";
const char* WIFI_PASSWORD = "X7e4upMA4Y";
const char* API_KEY = "AIzaSyA7kOahrIAr_XImPq05EhbJ7CZSPATIZUs";
const char* DATABASE_URL = "https://parking-iot-a0b8e-default-rtdb.firebaseio.com/";

const char* FB_PATH_STATUS = "/parking_status";
const char* FB_PATH_LOGS = "/parking_logs";

const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = -21600; // -6 horas (CST)
const int DAYLIGHT_OFFSET_SEC = 0;

// ==================================================
// 2. PINES Y SENSORES
// ==================================================
#define DHTPIN 4
#define DHTTYPE DHT22
#define TRIG_PIN 5
#define ECHO_PIN 18
#define KY037_PIN 34
#define BUZZER_PIN 21 // Pin del buzzer pasivo
#define LED_VERDE 22  // Pin LED verde
#define LED_ROJO 23   // Pin LED rojo

DHT dht(DHTPIN, DHTTYPE);

// ==================================================
// 3. UMBRALES Y TEMPORIZADORES
// ==================================================
const unsigned long SENSOR_READ_INTERVAL = 500;
const unsigned long FIREBASE_SEND_INTERVAL = 15000;
const unsigned long WIFI_RECONNECT_INTERVAL = 20000;

// --- ¡CAMBIO SOLICITADO! ---
const float DIST_OCUPADO = 30.0; // <<-- Ocupado se detecta a 30cm o menos
const float DIST_LIBRE = 120.0; // A partir de aquí empieza la "Aproximacion"
const int RUIDO_UMBRAL = 3500; // Umbral para alarma de claxon

// ==================================================
// 4. OBJETOS Y VARIABLES GLOBALES
// ==================================================
FirebaseData fbdo;
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
String estadoActual = "Inicializando";
String estadoAnterior = "";

// --- NUEVAS VARIABLES PARA MANEJO DE ALARMAS ---
bool alarmaRuidoActiva = false;
unsigned long alarmaRuidoTimestamp = 0;
const unsigned long ALARMA_RUIDO_DURACION = 500; // 500ms

// ==================================================
// 5. SETUP INICIAL
// ==================================================
void setup() {
    Serial.begin(115200);
    Serial.println("\n\n--- INICIANDO SISTEMA (CON ALARMA DE PROXIMIDAD) ---");

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(KY037_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_VERDE, OUTPUT);
    pinMode(LED_ROJO, OUTPUT);

    dht.begin();
    
    iniciarWiFi();
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    iniciarFirebase();

    Serial.println("-------------------------------------------");
    Serial.println("🚀 Sistema listo para operar. Iniciando loop...");
    Serial.println("-------------------------------------------");
}

// ==================================================
// 6. BUCLE PRINCIPAL (NO BLOQUEANTE)
// ==================================================
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

    // Tarea 2: Leer Sensores y Lógica de Estado (cada 500ms)
    if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
        lastSensorRead = now;

        estadoAnterior = estadoActual;
        distanciaAnterior_cm = distancia_cm; 
        
        leerSensores(); // Actualiza distancia_cm, nivel_sonido, etc.
        
        estadoActual = decidirEstado(distancia_cm); 
        
        // --- ¡¡LÍNEA CORREGIDA!! ---
        // Ahora incluye la humedad "Hum: %.1f%%"
        Serial.printf("📏 Dist: %.1f cm | 🌡 Temp: %.1f°C | 💧 Hum: %.1f%% | 🎤 Sonido: %d | 🅿 Estado: %s\n",
                      distancia_cm, temperatura_c, humedad_pct, nivel_sonido, estadoActual.c_str());

        // Esta función ahora solo maneja LEDs y DISPARA la alarma de ruido
        manejarHardwareFijo(estadoActual, nivel_sonido);
    }

    // --- ¡NUEVA TAREA 3! ---
    // Tarea 3: Manejar Buzzer (se ejecuta en CADA loop para ser fluido)
    // Esta función maneja AMBAS alarmas: la de prioridad (ruido) y la de proximidad
    manejarBuzzer(estadoActual, distancia_cm);


    // Tarea 4: Enviar a Firebase
    bool estadoCambiado = (estadoActual != estadoAnterior && estadoActual != "Inicializando");
    bool tiempoDeEnvio = (now - lastFirebaseSend >= FIREBASE_SEND_INTERVAL);

    if ((estadoCambiado || tiempoDeEnvio) && Firebase.ready() && WiFi.status() == WL_CONNECTED) {
        lastFirebaseSend = now;
        enviarAFirebase(estadoCambiado); 
    }
}

// ==================================================
// 7. FUNCIONES AUXILIARES (FUSIONADAS)
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
    
    nivel_sonido = analogRead(KY037_PIN);
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
// --- (Fin de las funciones sin cambios) ---


// --- ¡FUNCIÓN MODIFICADA! ---
// Esta lógica ahora usa DIST_OCUPADO = 30cm
String decidirEstado(float dist) {
    static int contadorEstable = 0;
    String estado;

    if (dist >= 400 || dist <= 0) {
        dist = distanciaAnterior_cm;
    }

    // Rango 1: Libre (dist > 120)
    if (dist > DIST_LIBRE) {
        estado = "Libre";
        contadorEstable = 0;
    // Rango 2: Aproximación (30 < dist <= 120)
    } else if (dist > DIST_OCUPADO && dist <= DIST_LIBRE) {
        estado = "Aproximacion";
        contadorEstable = 0;
    // Rango 3: Ocupado/Maniobra (dist <= 30)
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


// --- ¡FUNCIÓN MODIFICADA! ---
// Esta función ahora solo maneja LEDs y DISPARA la alarma de ruido
void manejarHardwareFijo(const String& estado, int ruido) {
    // 1. Control de LEDs (Pines 22 y 23)
    if (estado == "Libre") {
        digitalWrite(LED_VERDE, HIGH);
        digitalWrite(LED_ROJO, LOW);
    } else {
        digitalWrite(LED_VERDE, LOW);
        digitalWrite(LED_ROJO, HIGH);
    }

    // 2. DISPARADOR de Alarma de Ruido
    if (ruido > RUIDO_UMBRAL && !alarmaRuidoActiva) {
        // Si hay ruido fuerte Y la alarma no está ya sonando
        alarmaRuidoActiva = true;
        alarmaRuidoTimestamp = millis();
    }
}

// --- ¡NUEVA FUNCIÓN DE BUZZER! ---
// Se ejecuta en cada loop para gestionar las alarmas de forma no bloqueante
void manejarBuzzer(const String& estado, float dist) {
    static unsigned long lastBeepTime = 0; // Temporizador para el ciclo de beep
    static bool isBuzzerOn = false;     // Estado actual del buzzer
    unsigned long now = millis();

    // --- SECCIÓN 1: Alarma de Ruido (Prioridad Alta) ---
    if (alarmaRuidoActiva) {
        if (now - alarmaRuidoTimestamp > ALARMA_RUIDO_DURACION) {
            // Fin de la alarma de ruido
            alarmaRuidoActiva = false;
            noTone(BUZZER_PIN);
            isBuzzerOn = false;
        } else {
            // Mantener la alarma de ruido sonando
            if (!isBuzzerOn) { // Llama a tone() solo una vez
               tone(BUZZER_PIN, 3000); // Tono agudo de claxon
               isBuzzerOn = true;
            }
        }
        return; // IMPORTANTE: No ejecutar la lógica de proximidad si la alarma de ruido está activa
    }

    // --- SECCIÓN 2: Alarma de Proximidad ---
    // Solo suena si nos estamos acercando ("Aproximacion") o maniobrando ("Maniobra")
    if (estado != "Aproximacion" && estado != "Maniobra") {
        if (isBuzzerOn) { // Si estaba sonando por proximidad, apágalo
            noTone(BUZZER_PIN);
            isBuzzerOn = false;
        }
        return; // No hacer nada si está "Libre" u "Ocupado"
    }

    // Lógica de proximidad (mapeo inverso)
    // Limita la distancia al rango de 0 a DIST_LIBRE
    float dist_constrain = constrain(dist, 0, DIST_LIBRE);
    
    // Intervalo de silencio: más corto cuanto más cerca (de 700ms a 50ms)
    long intervalo_silencio = map(dist_constrain, DIST_LIBRE, 0, 700, 50);
    // Duración del beep: más largo cuanto más cerca (de 50ms a 200ms)
    long duracion_beep = map(dist_constrain, DIST_LIBRE, 0, 50, 200);
    // Frecuencia (tono): más agudo cuanto más cerca (de 500Hz a 2500Hz)
    long freq = map(dist_constrain, DIST_LIBRE, 0, 500, 2500);

    if (isBuzzerOn) {
        // El buzzer está SONANDO. ¿Debería apagarse?
        if (now - lastBeepTime > duracion_beep) {
            noTone(BUZZER_PIN);
            isBuzzerOn = false;
            lastBeepTime = now; // Reinicia timer para el silencio
        }
    } else {
        // El buzzer está APAGADO. ¿Debería sonar?
        if (now - lastBeepTime > intervalo_silencio) {
            tone(BUZZER_PIN, freq); // ¡Sin duración! Lo controlamos manualmente
            isBuzzerOn = true;
            lastBeepTime = now; // Reinicia timer para la duración
        }
    }
}


// --- (getTimestampLocal y enviarAFirebase) ---
// --- (Funciones limpiadas de caracteres invisibles) ---
String getTimestampLocal() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "N/A";
    }
    char buf[30];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buf);
}

void enviarAFirebase(bool enviarLog) {
    FirebaseJson json;
    String ts_local = getTimestampLocal();
    
    json.set("timestamp", ts_local);
    json.set("estado", estadoActual);
    json.set("distancia_cm", String(distancia_cm, 1));
    json.set("temperatura_c", String(temperatura_c, 1));
    json.set("humedad_pct", String(humedad_pct, 1));
    json.set("nivel_sonido", nivel_sonido);

    Serial.println("📤 Enviando datos a Firebase...");
    
    if (Firebase.RTDB.setJSON(&fbdo, FB_PATH_STATUS, &json)) {
        Serial.println("✅ Estado actual actualizado.");
    } else {
        Serial.println("❌ Error al enviar estado: " + fbdo.errorReason());
    }

    if (enviarLog) {
        String pathHist = String(FB_PATH_LOGS) + "/" + ts_local; 
        
        if (Firebase.RTDB.setJSON(&fbdo, pathHist.c_str(), &json)) {
            Serial.println("🗃 ¡Cambio de estado! Log guardado en: " + pathHist);
        } else {
            Serial.println("❌ Error al enviar log: " + fbdo.errorReason());
        }
    }
}
