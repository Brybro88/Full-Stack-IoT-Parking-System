package com.example.parking_iot_app.repositorio
import android.util.Log // ¡Añade esta línea!
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseError
import com.google.firebase.database.ValueEventListener
import com.google.firebase.database.database
import com.example.parking_iot_app.data.ParkingData
import com.google.firebase.Firebase
import android.content.Context
import com.example.parking_iot_app.utils.NotificationHelper

// ¡CAMBIO! Ahora requiere un Contexto
class FirebaseRepository(private val context: Context) {

    private val statusRef = Firebase.database.getReference("parking_status")

    private val _parkingStatus = MutableLiveData<ParkingData>()
    val parkingStatus: LiveData<ParkingData> = _parkingStatus

    // ¡NUEVO! Instancia del helper y almacenamiento del estado anterior
    private val notificationHelper = NotificationHelper(context)
    private var previousData: ParkingData? = null

    // --- Umbrales de Sensores Anormales (igual que en tu Cloud Function) ---
    private val TEMP_MAX = 35.0f
    private val TEMP_MIN = 5.0f
    private val HUM_MAX = 80.0f

    init {
        // ¡NUEVO! Crear el canal de notificación al iniciar
        notificationHelper.createNotificationChannel()
        listenForParkingStatus()
    }

    private fun listenForParkingStatus() {
        statusRef.addValueEventListener(object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val dataAhora = snapshot.getValue(ParkingData::class.java)

                // Actualizar la UI (como antes)
                _parkingStatus.postValue(dataAhora ?: ParkingData(estado = "Sin Datos"))

                // --- ¡NUEVA LÓGICA DE NOTIFICACIÓN! ---
                // Compara los datos nuevos con los anteriores para ver si disparamos una alerta
                checkAndSendNotification(previousData, dataAhora)

                // Guardar los datos actuales como "viejos" para la próxima comparación
                previousData = dataAhora
            }

            override fun onCancelled(error: DatabaseError) {
                _parkingStatus.postValue(ParkingData(estado = "Error"))
            }
        })
    }

    /**
     * ¡NUEVO! Esta función contiene la lógica de tus disparadores
     */
    private fun checkAndSendNotification(dataAntes: ParkingData?, dataAhora: ParkingData?) {
        // --- INICIO DEL LOG DE DEPURACIÓN ---
        Log.d("ParkingAppDebug", "--- CheckAndSendNotification INICIADO ---")

        if (dataAhora == null) {
            Log.d("ParkingAppDebug", "dataAhora es NULL. Omitiendo notificaciones.")
            return
        }
        if (dataAntes == null) {
            Log.d("ParkingAppDebug", "dataAntes es NULL (es la primera carga). Omitiendo notificaciones.")
            return
        }
                "Alerta" -> notificationHelper.sendNotification(
                    "¡Alerta de Claxon Detectada!",
                    "Se detectó un ruido de ${dataAhora.nivel_sonido}. Podría ser un claxon o alarma."
                )
                "Elevado" -> notificationHelper.sendNotification(
                    "Ruido Anormal Detectado",
                    "Nivel de ruido (${dataAhora.nivel_sonido}) es más alto que el promedio."
                )
            }
        }

        // --- DISPARADOR 3: Temperatura Anormal ---
        val tempAhora = dataAhora.temperatura_c.toFloatOrNull()
        val tempAntes = dataAntes.temperatura_c.toFloatOrNull()

        if (tempAhora != null && tempAntes != null) {
            if (tempAhora > TEMP_MAX && tempAntes <= TEMP_MAX) {
                Log.d("ParkingAppDebug", "¡DISPARADOR 3 (TEMP ALTA) DETECTADO! Enviando notificación.")
                notificationHelper.sendNotification(
                    "Alerta de Temperatura Alta",
                    "El sensor registra ${tempAhora}°C. ¡Riesgo de calor extremo!"
                )
            }
            if (tempAhora < TEMP_MIN && tempAntes >= TEMP_MIN) {
                Log.d("ParkingAppDebug", "¡DISPARADOR 3 (TEMP BAJA) DETECTADO! Enviando notificación.")
                notificationHelper.sendNotification(
                    "Alerta de Temperatura Baja",
                    "El sensor registra ${tempAhora}°C. ¡Riesgo de frío extremo!"
                )
            }
        }

        // --- DISPARADOR 4: Humedad Anormal ---
        val humAhora = dataAhora.humedad_pct.toFloatOrNull()
        val humAntes = dataAntes.humedad_pct.toFloatOrNull()

        if (humAhora != null && humAntes != null) {
            if (humAhora > HUM_MAX && humAntes <= HUM_MAX) {
                Log.d("ParkingAppDebug", "¡DISPARADOR 4 (HUMEDAD ALTA) DETECTADO! Enviando notificación.")
                notificationHelper.sendNotification(
                    "Alerta de Humedad Alta",
                    "El sensor registra ${humAhora}%. Riesgo de condensación."
                )
            }
        }
    }
}