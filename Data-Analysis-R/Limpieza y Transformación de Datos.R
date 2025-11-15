# 5. Limpieza y Transformación de Datos 

datos_limpios <- datos %>%
  mutate(
    distancia_cm  = as.numeric(distancia_cm),
    temperatura_c = as.numeric(temperatura_c),
    humedad_pct   = as.numeric(humedad_pct),
    nivel_sonido  = as.integer(nivel_sonido),
    ruido_baseline= as.numeric(ruido_baseline),
    
    timestamp = as.POSIXct(timestamp, format="%Y-%m-%dT%H:%M:%SZ"),
    
    estado = as.factor(estado),
    estado_ruido = as.factor(estado_ruido)
  ) %>%
  
  select(timestamp, estado, distancia_cm, temperatura_c, humedad_pct, 
         nivel_sonido, estado_ruido, ruido_baseline)

summary(datos_limpios)
