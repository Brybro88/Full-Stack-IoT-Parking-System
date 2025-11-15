# --- 1. INSTALAR Y CARGAR HERRAMIENTAS ---

library(tidyverse)
library(jsonlite)
library(lubridate) # <- Añadida para manejar fechas fácilmente

# --- 2. CARGAR Y TRANSFORMAR LOS DATOS ---

# Cargar el archivo JSON crudo
datos_crudos <- fromJSON("parking_logs.json")

datos <- bind_rows(datos_crudos)

# ¡Inspecciona los datos!
cat("--- 1. DATOS CARGADOS (AÚN SUCIOS) ---\n")
glimpse(datos)


# --- 3. LIMPIEZA DE DATOS (DATA WRANGLING) ---

datos_limpios <- datos %>%
  mutate(
    # Convertir las columnas de texto a números
    distancia_cm  = as.numeric(distancia_cm),
    temperatura_c = as.numeric(temperatura_c),
    humedad_pct   = as.numeric(humedad_pct),
    nivel_sonido  = as.integer(nivel_sonido),
    ruido_baseline= as.numeric(ruido_baseline),
    
    # Convertir el texto del timestamp a un objeto de fecha-hora real
    timestamp = as.POSIXct(timestamp, format="%Y-%m-%dT%H:%M:%SZ"),
    
    # Convertir texto a "categorías" (factores)
    estado = as.factor(estado),
    estado_ruido = as.factor(estado_ruido)
  ) %>%
  
  # Seleccionar solo las columnas que nos importan
  select(
    timestamp, estado, distancia_cm, temperatura_c, humedad_pct, 
    nivel_sonido, estado_ruido, ruido_baseline
  )

# ¡Inspecciona los datos limpios!
cat("\n\n--- 2. DATOS LIMPIOS Y LISTOS ---\n")
glimpse(datos_limpios)


# --- 4. ANÁLISIS ESTADÍSTICO RÁPIDO ---

cat("\n\n--- 3. RESUMEN ESTADÍSTICO AUTOMÁTICO ---\n")
# R te dará el Mínimo, Máximo, Promedio, Mediana, etc.
summary(datos_limpios)


# --- 5. VISUALIZACIÓN DE DATOS (LAS GRÁFICAS) ---

cat("\nGenerando Gráfica 1: Distribución de Estados...")
ggplot(data = datos_limpios, aes(x = estado, fill = estado)) +
  geom_bar() +
  scale_fill_manual(values = c("Libre" = "#28a745", 
                               "Ocupado" = "#dc3545", 
                               "Aproximacion" = "#ffc107", 
                               "Maniobra" = "#fd7e14",
                               "Bajo" = "#0d6efd",
                               "Normal" = "#6c757d")) +
  labs(title = "Distribución de Estados del Estacionamiento",
       x = "Estado",
       y = "Cantidad de Registros (Logs)") +
  theme_light()


# GRÁFICA 2: ¿Cómo cambió la temperatura y humedad en el tiempo? (Líneas)

cat("\nGenerando Gráfica 2: Tendencia Ambiental...")
ggplot(data = datos_limpios, aes(x = timestamp)) +
  geom_line(aes(y = temperatura_c, color = "Temperatura")) +
  geom_line(aes(y = humedad_pct, color = "Humedad")) +
  labs(title = "Análisis de Tendencia: Temperatura y Humedad",
       x = "Fecha y Hora",
       y = "Valor del Sensor",
       color = "Métrica") +
  theme_minimal()


# GRÁFICA 3: ¿A qué hora del día hubo más ruido? (Boxplot)

cat("\nGenerando Gráfica 3: Ruido por Hora...")
ggplot(data = datos_limpios, aes(x = as.factor(hour(timestamp)), y = nivel_sonido)) +
  geom_boxplot(fill = "blue") +
  labs(title = "Distribución de Ruido por Hora del Día",
       x = "Hora del Día (0-23h)",
       y = "Nivel de Ruido (Raw)") +
  theme_minimal()

cat("\n\n--- ANÁLISIS COMPLETO ---")