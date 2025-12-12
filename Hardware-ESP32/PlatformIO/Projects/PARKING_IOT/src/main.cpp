
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "DHT.h"
#include <time.h>
#include <algorithm> 

// 1. CONFIGURACIÓN DE RED Y FIREBASE
const char* WIFI_SSID = "LEONEZEQUIEL";
const char* WIFI_PASSWORD = "begiakhi88";

// Credenciales de Firebase 
const char* DATABASE_SECRET = "fvNtqyqklwhW3Zy35ed352460uvlbgPPJLf8XOkD";
const char* DATABASE_URL = "https://parking-iot-a0b8e-default-rtdb.firebaseio.com/";

// Rutas de la base de datos
const char* FB_PATH_STATUS = "/parking_status";
const char* FB_PATH_LOGS = "/parking_logs";

// Configuración de Hora (UTC/GMT+0 para compatibilidad con dashboards)
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
const unsigned long SENSOR_READ_INTERVAL = 500;      // Leer cada 500ms
const unsigned long FIREBASE_SEND_INTERVAL = 15000;  // Enviar cada 15s (heartbeat)
const unsigned long WIFI_RECONNECT_INTERVAL = 20000; // Reintentar wifi cada 20s

const float DIST_OCUPADO = 50.0; // cm
const float DIST_LIBRE = 120.0;  // cm
const int RUIDO_UMBRAL = 1500;   // Valor ADC (0-4095) para alerta fuerte

//  
// 4. OBJETOS Y VARIABLES GLOBALES
//  
// ¡IMPORTANTE! Dos objetos separados para evitar conflictos de escritura
FirebaseData fbdo_status; 
FirebaseData fbdo_logs;   

FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastSensorRead = 0;
unsigned long lastFirebaseSend = 0;
unsigned long lastWifiCheck = 0;

// Variables de Sensores (Inicializadas con valores seguros)
float distancia_cm = 130.0; 
float distanciaAnterior_cm = 130.0;
float temperatura_c = 0.0;
float humedad_pct = 0.0;
int nivel_sonido = 0;

// Variables de Análisis de Ruido (Promedio Móvil)
const int RUIDO_MUESTRAS = 60;
int lecturasRuido[RUIDO_MUESTRAS];
int indiceRuido = 0;
long sumaRuido = 0;
float ruidoBaselineAvg = 0.0;
String estadoRuido = "Normal";

// Estado del Sistema
String estadoActual = "Inicializando";
String estadoAnterior = "";

// Alarmas
bool alarmaRuidoActiva = false;
unsigned long alarmaRuidoTimestamp = 0;
const unsigned long ALARMA_RUIDO_DURACION = 500;

//  
// 5. PROTOTIPOS DE FUNCIONES
//  
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
void tokenStatusCallback(FirebaseConfig* config) { }

//  
// 6. SETUP
//  
void setup() {
    Serial.begin(115200);
    Serial.println("\n\n--- INICIANDO SISTEMA (VERSIÓN FINAL) ---");

    // Configurar Pines
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(KY037_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_VERDE, OUTPUT);
    pinMode(LED_ROJO, OUTPUT);

    // Iniciar DHT
    dht.begin();

    // Pre-llenar array de ruido para evitar promedios erróneos al inicio
    for (int i = 0; i < RUIDO_MUESTRAS; i++) {
        lecturasRuido[i] = 50;
    }
    sumaRuido = 50 * RUIDO_MUESTRAS;
    ruidoBaselineAvg = 50.0;
    
    // Conectar a la Red
    iniciarWiFi();

    // Sincronizar Hora (Bloqueante al inicio para asegurar timestamps válidos)
    Serial.print("🕒 Sincronizando hora (NTP)");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo)) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("\n✅ Hora sincronizada correctamente.");

    // Iniciar Firebase
    iniciarFirebase();

    Serial.println("-------------------------------------------");
    Serial.println("🚀 Sistema listo. Iniciando bucle principal...");
    Serial.println("-------------------------------------------");
}

//  
// 7. LOOP PRINCIPAL
//  
void loop() {
    unsigned long now = millis();

    // Tarea 1: Asegurar WiFi
    if (now - lastWifiCheck > WIFI_RECONNECT_INTERVAL) {
        lastWifiCheck = now;
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("🔌 WiFi desconectado. Reintentando...");
            WiFi.reconnect();
        }
    }

    // Tarea 2: Leer Sensores y Lógica (500ms)
    if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
        lastSensorRead = now;

        estadoAnterior = estadoActual;
        distanciaAnterior_cm = distancia_cm; 

        leerSensores();
        analizarRuido();
        estadoActual = decidirEstado(distancia_cm); 

        // Diagnóstico en Serial
        Serial.printf("📏 %.1f cm | 🌡 %.1f°C | 💧 %.1f%% | 🎤 %d (Avg: %.0f) | %s | 🔊 %s\n",
                      distancia_cm, temperatura_c, humedad_pct, nivel_sonido, ruidoBaselineAvg, estadoActual.c_str(), estadoRuido.c_str());

        manejarHardwareFijo(estadoActual, nivel_sonido);
    }

    // Tarea 3: Buzzer (Ejecución rápida)
    manejarBuzzer(estadoActual, distancia_cm);

    // Tarea 4: Enviar a Firebase
    bool estadoCambiado = (estadoActual != estadoAnterior && estadoActual != "Inicializando");
    bool tiempoDeEnvio = (now - lastFirebaseSend >= FIREBASE_SEND_INTERVAL);

    if (estadoCambiado || tiempoDeEnvio) {
        if (Firebase.ready() && WiFi.status() == WL_CONNECTED) {
            lastFirebaseSend = now;
            enviarAFirebase(); 
        } else if (!Firebase.ready()) {
             // Solo loguear si falla, no bloquear
             Serial.println("⚠️ Firebase no listo. Reintentando en siguiente ciclo.");
        }
    }
}

//  
// 8. IMPLEMENTACIÓN DE FUNCIONES
//  

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
        Serial.println("\n❌ Falló WiFi. Reiniciando...");
        delay(1000); ESP.restart();
    }
    Serial.print("\n✅ WiFi Conectado: ");
    Serial.println(WiFi.localIP().toString());
}

void iniciarFirebase() {
    config.signer.tokens.legacy_token = DATABASE_SECRET;
    config.database_url = DATABASE_URL;
    config.token_status_callback = tokenStatusCallback;
    
    Firebase.reconnectWiFi(true);
    Firebase.begin(&config, &auth);
    
    Serial.println("🔥 Inicializando Firebase...");
}

void leerSensores() {
    // Lectura DHT
    float temp_val = dht.readTemperature();
    float hum_val = dht.readHumidity();
    if (!isnan(temp_val)) temperatura_c = temp_val;
    if (!isnan(hum_val)) humedad_pct = hum_val;

    // Lectura Ultrasonido
    float dist_filtrada = leerDistanciaFiltrada_Promedio(); 
    if (dist_filtrada != -1.0) {
        distancia_cm = dist_filtrada;
    } else {
        distancia_cm = distanciaAnterior_cm; // Ignorar lecturas fallidas
    }
    
    // Lectura Ruido (Promedio Móvil)
    sumaRuido = sumaRuido - lecturasRuido[indiceRuido];
    nivel_sonido = analogRead(KY037_PIN);
    lecturasRuido[indiceRuido] = nivel_sonido;
    sumaRuido = sumaRuido + nivel_sonido;
    indiceRuido = (indiceRuido + 1) % RUIDO_MUESTRAS;
    ruidoBaselineAvg = (float)sumaRuido / RUIDO_MUESTRAS;
}

float leerDistanciaFiltrada_Promedio() {
    const int N = 5;
    const int MIN_VALID = 2;
    const int MAX_VALID = 400;
    int valid = 0;
    long suma = 0;

    for (int i = 0; i < N; i++) {
        digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
        digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
        digitalWrite(TRIG_PIN, LOW);

        long duracion = pulseIn(ECHO_PIN, HIGH, 25000); // 25ms timeout
        int d = duracion * 0.034 / 2;

        if (d >= MIN_VALID && d <= MAX_VALID) {
            suma += d;
            valid++;
        }
        delay(10); // Pequeña pausa para estabilidad
    }

    if (valid == 0) return -1.0; // Error
    return (float)suma / valid;
}

String decidirEstado(float dist) {
    static int contadorEstable = 0;
    
    if (dist == -1.0) return estadoActual; // Mantener estado si hay error de lectura

    if (dist > DIST_LIBRE) {
        contadorEstable = 0;
        return "Libre";
    } else if (dist > DIST_OCUPADO) {
        contadorEstable = 0;
        return "Aproximacion";
    } else { 
        // Lógica de estabilización
        if (abs(dist - distanciaAnterior_cm) <= 5.0) {
            contadorEstable++;
        } else {
            contadorEstable = 0;
        }

        if (contadorEstable >= 3) {
            return "Ocupado";
        } else {
            return "Maniobra";
        }
    }
}

void manejarHardwareFijo(const String& estado, int ruido) {
    if (estado == "Libre") {
        digitalWrite(LED_VERDE, HIGH);
        digitalWrite(LED_ROJO, LOW);
    } else {
        digitalWrite(LED_VERDE, LOW);
        digitalWrite(LED_ROJO, HIGH);
    }
    
    // Disparador Alarma Ruido
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

    // Prioridad 1: Alarma de Ruido
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

    // Apagar si no hay movimiento relevante
    if (estado != "Aproximacion" && estado != "Maniobra") {
        if (isBuzzerOn) {
            noTone(BUZZER_PIN);
            isBuzzerOn = false;
        }
        return;
    }

    // Prioridad 2: Proximidad
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
        return "2025-01-01T00:00:00Z"; 
    }
    char buf[30];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo); // Formato UTC ISO8601
    return String(buf);
}

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

    Serial.println("📤 Enviando a Firebase...");

    // 1. Status Actual
    if (Firebase.RTDB.setJSON(&fbdo_status, FB_PATH_STATUS, &json)) {
        Serial.println("   ✅ Status OK");
    } else {
        Serial.printf("   ❌ Status ERROR: %s\n", fbdo_status.errorReason().c_str());
    }

    // 2. Historial (Logs)
    if (Firebase.RTDB.pushJSON(&fbdo_logs, FB_PATH_LOGS, &json)) {
        Serial.println("   ✅ Log Guardado OK");
    } else {
        Serial.printf("   ❌ Log ERROR: %s\n", fbdo_logs.errorReason().c_str());
    }
}
