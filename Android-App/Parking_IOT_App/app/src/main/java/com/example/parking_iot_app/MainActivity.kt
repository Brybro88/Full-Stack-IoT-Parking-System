package com.example.parking_iot_app // O tu package name

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import androidx.activity.result.contract.ActivityResultContracts
import android.os.Bundle
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.example.parking_iot_app.data.ParkingData
import com.example.parking_iot_app.databinding.ActivityMainBinding
import com.example.parking_iot_app.viewmodel.ParkingViewModel
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.TimeZone

class MainActivity : AppCompatActivity() {

    // Habilitar View Binding para acceder fácil a la UI
    private lateinit var binding: ActivityMainBinding

    // Inicializar el ViewModel
    private val viewModel: ParkingViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Inflar el layout
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Configurar los títulos de las tarjetas (solo se hace una vez)
        setupStaticUI()
        // Empezar a observar los datos
        observeViewModel()

        // ¡CORRECTO! Llamar a la función para pedir permiso
        askNotificationPermission()
    }

    // Objeto para manejar la solicitud de permiso
    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { isGranted: Boolean ->
        if (isGranted) {
            // Permiso concedido. No necesitamos hacer nada más.
            println("Permiso de notificación concedido por el usuario.")
        } else {
            // El usuario rechazó el permiso
            println("Permiso de notificación denegado.")
        }
    }

    // Función para pedir permiso (necesaria para Android 13+)
    private fun askNotificationPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                // Pedir el permiso
                requestPermissionLauncher.launch(Manifest.permission.POST_NOTIFICATIONS)
            } else {
                // Permiso ya concedido, no hacer nada.
            }
        } else {
            // No se necesita permiso en versiones antiguas de Android.
        }
    }

    // --- ¡ERROR CORREGIDO! ---
    // La función subscribeToTopic() se ha eliminado por completo
    // porque pertenece al método FCM (de pago) que no estamos usando.

    private fun setupStaticUI() {
        // Configurar Distancia
        binding.cardDistancia.tvSensorTitle.text = "Distancia"
        binding.cardDistancia.ivSensorIcon.setImageResource(R.drawable.ic_distance)
        
        // Configurar Temperatura
        binding.cardTemperatura.tvSensorTitle.text = "Temperatura"
        binding.cardTemperatura.ivSensorIcon.setImageResource(R.drawable.ic_thermostat)
        
        // Configurar Humedad
        binding.cardHumedad.tvSensorTitle.text = "Humedad"
        binding.cardHumedad.ivSensorIcon.setImageResource(R.drawable.ic_humidity)
        
        // Configurar Ruido
        binding.cardRuido.tvSensorTitle.text = "Ruido"
        binding.cardRuido.ivSensorIcon.setImageResource(R.drawable.ic_volume)
    }

    private fun observeViewModel() {
        // Observar el LiveData del ViewModel
        viewModel.parkingStatus.observe(this) { data ->
            // Cuando los datos cambien, llamar a la función de actualización
            updateUI(data)
        }
    }

    private fun updateUI(data: ParkingData) {
        // 1. Actualizar Tarjeta de Estado Principal
        binding.tvStatusCircle.text = data.estado.uppercase()
        binding.tvLastUpdate.text = "Última actualización: ${formatTimestamp(data.timestamp)}"

        // 2. Actualizar Color del Círculo
        val statusDrawable = when (data.estado.lowercase()) {
            "libre" -> R.drawable.shape_circle_status_libre
            "ocupado" -> R.drawable.shape_circle_status_ocupado
            "aproximacion" -> R.drawable.shape_circle_status_aprox
            "maniobra" -> R.drawable.shape_circle_status_maniobra
            else -> R.drawable.shape_circle_status_inicial
        }
        binding.tvStatusCircle.background = ContextCompat.getDrawable(this, statusDrawable)

        // 3. Actualizar Tarjetas de Sensores
        binding.cardDistancia.tvSensorValue.text = "${data.distancia_cm} cm"
        binding.cardTemperatura.tvSensorValue.text = "${data.temperatura_c} °C"
        binding.cardHumedad.tvSensorValue.text = "${data.humedad_pct} %"
        binding.cardRuido.tvSensorValue.text = data.nivel_sonido.toString()
    }

    // Función simple para formatear el timestamp
    private fun formatTimestamp(timestamp: String): String {
        return try {
            // Formato de parser para ISO 8601 (UTC)
            val parser = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'", Locale.getDefault())
            parser.timeZone = TimeZone.getTimeZone("UTC") // Avisa que la cadena es UTC

            // Formato de salida para la hora local del teléfono
            val formatter = SimpleDateFormat("h:mm:ss a", Locale.getDefault())
            formatter.timeZone = TimeZone.getDefault() // Usar la zona local

            val date: Date = parser.parse(timestamp) ?: Date()
            formatter.format(date)
        } catch (e: Exception) {
            timestamp // Devuelve el original si falla
        }
    }
}