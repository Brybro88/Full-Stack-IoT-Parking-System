# Full-Stack-IoT-Parking-System
Sistema de monitoreo de estacionamiento con ESP32, Firebase, Node.js (Web), app en Android (Kotlin) y análisis de datos en R.
# Sistema Inteligente de Monitoreo de Estacionamiento (Full-Stack IoT)

Un proyecto completo de IoT que monitorea un espacio de estacionamiento en tiempo real, envía datos a la nube, muestra el estado en un dashboard web y una app móvil, y analiza los datos históricos de ocupación.

##  Sobre el Proyecto

Este proyecto es una solución integral "end-to-end" que demuestra el flujo completo de datos desde un sensor físico hasta un análisis de Business Intelligence. El sistema detecta el estado de un estacionamiento (Libre, Ocupado, Maniobra) y monitorea el ambiente (Temperatura, Humedad, Ruido) en tiempo real.

## Stack Tecnológico (Arquitectura)

Este proyecto está dividido en 5 componentes principales:

1.  **Hardware (Dispositivo IoT):**
    * **Microcontrolador:** ESP32-WROOM-32
    * **Sensores:** HC-SR04 (Distancia), DHT22 (Temp/Hum), KY-037 (Sonido).
    * **Entorno:** PlatformIO en VS Code (C++).

2.  **Base de Datos (Nube):**
    * **Google Firebase** (Realtime Database) para ingesta de datos en tiempo real.

3.  **Dashboard Web (Backend & Frontend):**
    * **Backend:** Node.js + Express (para la API REST).
    * **Frontend:** EJS (HTML dinámico), CSS3 y JavaScript (ES6+).
    * **Visualización:** ApexCharts.js (Gráficas) y Gauge.js (Medidores).

4.  **Aplicación Móvil (Nativa):**
    * **Plataforma:** Android (Nativa).
    * **Lenguaje:** Kotlin.
    * **Arquitectura:** MVVM (ViewModel, LiveData, Repository).
    * **Conexión:** SDK de Firebase.

5.  **Análisis de Datos:**
    * **Entorno:** RStudio.
    * **Librerías:** Tidyverse (`dplyr`, `ggplot2`) y `jsonlite`.

##  Características Principales

* **Monitoreo en Vivo:** El dashboard web y la app de Android se actualizan en tiempo real.
* **Análisis Histórico:** Página de "Análisis" con filtros de fecha (Hoy, 7 Días) para estudiar patrones.
* **Visualización Avanzada:**
    * Mapa de calor (Heatmap) 24x7 para identificar horas pico.
    * Gráficas de tendencia para temperatura, humedad y ruido ambiental.
    * Gráficas de dona para ver la distribución de estados (Libre vs. Ocupado).
* **Alertas Inteligentes:** Detección de ruido "Anormal" (comparado con el promedio) y alertas de claxon.
* **Hardware Personalizado:** Case impreso en 3D (`.stl` incluido) y diseño de placa fenólica.
