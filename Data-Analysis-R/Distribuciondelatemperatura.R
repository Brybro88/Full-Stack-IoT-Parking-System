# --- ANÁLISIS 4: DISTRIBUCIÓN DE LA TEMPERATURA ---

# 1. Calculamos el promedio de temperatura para mostrarlo
temp_promedio <- mean(datos_limpios$temperatura_c, na.rm = TRUE)

# 2. Creamos el histograma
ggplot(data = datos_limpios, aes(x = temperatura_c)) +
  
  # "binwidth=0.5" agrupa las temps en rangos de 0.5 grados
  geom_histogram(binwidth = 0.5, fill = "#dc3545", color = "black") +
  geom_vline(aes(xintercept = temp_promedio), 
             color = "blue", linetype = "dashed", linewidth = 1) +
  
  labs(title = "Histograma de Lecturas de Temperatura",
       subtitle = paste("El promedio de temperatura es:", round(temp_promedio, 2), "°C"),
       x = "Temperatura (°C)",
       y = "Cantidad de Registros") +
  theme_minimal()