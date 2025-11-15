# --- ANÁLISIS 2: EVENTOS "OCUPADO" POR HORA DEL DÍA ---

datos_por_hora <- datos_limpios %>%
  mutate(hora_del_dia = hour(timestamp)) %>%
  filter(estado == "Ocupado") %>%
  count(hora_del_dia)

# 2. Creamos la gráfica
ggplot(data = datos_por_hora, aes(x = as.factor(hora_del_dia), y = n)) +
  geom_bar(stat = "identity", fill = "#0d6efd") +
  labs(title = "Horas Pico de Ocupación",
       x = "Hora del Día (0-23h)",
       y = "Total de Registros 'Ocupado'") +
  theme_minimal()