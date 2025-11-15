# --- ANÁLISIS 3: RUIDO BASELINE vs. ESTADO (LIBRE/OCUPADO) ---

# 1. Filtramos los datos para quedarnos solo con los estados estables
datos_ruido_filtrados <- datos_limpios %>%
  filter(estado == "Libre" | estado == "Ocupado")

ggplot(data = datos_ruido_filtrados, aes(x = estado, y = ruido_baseline, fill = estado)) +
  geom_boxplot() +
  scale_fill_manual(values = c("Libre" = "#28a745", "Ocupado" = "#dc3545")) +
  labs(title = "Comparación del Ruido Ambiental Promedio",
       subtitle = "¿Es más ruidoso el ambiente cuando el lugar está ocupado?",
       x = "Estado del Estacionamiento",
       y = "Ruido Ambiental Promedio (Baseline)") +
  theme_light()