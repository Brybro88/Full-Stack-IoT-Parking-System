Sistema Integral de Monitoreo de Estacionamientos (IoT Full-Stack)

Un sistema completo de ingeniería IoT diseñado para monitorear, gestionar y analizar la ocupación de espacios de estacionamiento en tiempo real. Este proyecto integra hardware embebido, computación en la nube, interfaces web/móviles y ciencia de datos.

📋 Resumen Ejecutivo

El sistema resuelve la problemática de la gestión ineficiente de espacios mediante un nodo sensor autónomo. Provee visualización en tiempo real con una latencia <2s y herramientas de análisis histórico para la toma de decisiones basada en datos.

🏗️ Arquitectura del Sistema

El proyecto consta de 5 módulos interconectados:

Hardware (Edge Computing):

Microcontrolador: ESP32 (Programado en C++ con PlatformIO).

Sensores: HC-SR04 (Ultrasonido), DHT22 (Ambiente), KY-037 (Ruido).

Funciones: Filtrado de señal, gestión de conexión WiFi/Firebase y lógica de estados (Libre/Ocupado).

Ubicación: /1-Hardware-ESP32

Backend & Cloud:

Base de Datos: Google Firebase Realtime Database.

Seguridad: Autenticación por tokens y reglas de seguridad estrictas.

Dashboard Web (Administración):

Stack: Node.js, Express, EJS.

Aplicación Móvil (Usuario Final):

Stack: Android Nativo (Kotlin), MVVM Architecture.

Features: Monitoreo en tiempo real y notificaciones locales en segundo plano (Foreground Service).


Ciencia de Datos (Analytics):

Stack: Lenguaje R (RStudio, Tidyverse).

Análisis: Procesamiento de logs históricos para mapas de calor (ocupación por hora) y correlación de variables ambientales.


🚀 Características Destacadas

Sincronización Total: El ESP32 utiliza NTP para sincronización de tiempo UTC, garantizando coherencia en los logs globales.

Robustez: Implementación de reconexión automática y manejo de errores en el firmware.

UI Avanzada: Uso de indicadores visuales (Gauges) y gráficas interactivas en ambas plataformas (Web y Móvil).

Eficiencia: El sistema utiliza un método híbrido de transmisión de datos (Estado en Tiempo Real + Logs Históricos) para optimizar el ancho de banda.

🛠️ Instalación y Despliegue

Requisitos Previos

Cuenta de Google Firebase.

ESP32 y sensores compatibles.

Node.js instalado localmente.

Instrucciones Rápidas

Hardware: Abrir la carpeta 1-Hardware-ESP32 en VS Code (PlatformIO), configurar credenciales WiFi en main.cpp y subir al dispositivo.

Web: Navegar a 2-Web-Dashboard, colocar el archivo serviceAccountKey.json en la raíz, ejecutar npm install y luego node index.js.

Móvil: Abrir 3-Android-App en Android Studio y sincronizar Gradle.

📊 Galería


Autor: Brayan Ezequiel García Ibarra
**Contacto +1 (979)422-0880