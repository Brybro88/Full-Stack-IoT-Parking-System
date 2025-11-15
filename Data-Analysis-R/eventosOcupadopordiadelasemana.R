# --- ANÁLISIS 1: EVENTOS "OCUPADO" POR DÍA DE LA SEMANA ---

library(lubridate)

# 1. Preparamos los datos:
#    - Añadimos una nueva columna "dia_semana"
#    - Filtramos solo los eventos "Ocupado"
#    - Contamos cuántos eventos hubo por día
datos_por_dia <- datos_limpios %>%
  mutate(dia_semana = wday(timestamp, label = TRUE, week_start = 1, abbr = FALSE)) %>%
  filter(estado == "Ocupado") %>%
  count(dia_semana)

# 2. Creamos la gráfica
ggplot(data = datos_por_dia, aes(x = dia_semana, y = n, fill = dia_semana)) +
  geom_bar(stat = "identity") + # "stat='identity'" usa los valores de 'n' directamente
  labs(title = "Eventos de Ocupación por Día de la Semana",
       x = "Día de la Semana",
       y = "Total de Registros 'Ocupado'") +
  theme_minimal() +
  theme(legend.position = "none")