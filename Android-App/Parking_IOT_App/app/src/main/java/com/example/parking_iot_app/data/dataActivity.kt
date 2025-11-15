package com.example.parking_iot_app.data

import com.google.firebase.database.IgnoreExtraProperties

// IgnoreExtraProperties evita que la app crashee si Firebase
// tiene un campo que no está definido aquí.
@IgnoreExtraProperties
data class ParkingData(
    val timestamp: String = "",
    val estado: String = "Inicializando",
    val distancia_cm: String = "--",
    val temperatura_c: String = "--",
    val humedad_pct: String = "--",
    val nivel_sonido: Int = 0,
    val estado_ruido: String = "Normal",
    val ruido_baseline: String = "0"
)
