
#Cargar y Limpiar Datos de Firebase

# 1. Cargar las librerías
library(tidyverse)
library(jsonlite)

# 2. Cargar los datos crudos desde el archivo JSON
datos_crudos <- fromJSON("parking_logs.json")

# 3. Convertir la estructura anidada de Firebase en una tabla limpia

datos <- bind_rows(datos_crudos, .id = NULL)

# 4.Inspecciona tus datos
print(datos)
glimpse(datos)
