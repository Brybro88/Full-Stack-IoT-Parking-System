package com.example.parking_iot_app.utlils

import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import androidx.core.app.NotificationCompat
import com.example.parking_iot_app.R // Importa tu R

class NotificationHelper(private val context: Context) {

    private val channelId = "parking_alerts"
    private val channelName = "Alertas de Estacionamiento"
    private val notificationManager = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager

    // Un ID único para cada tipo de notificación para que no se sobreescriban
    private var notificationIdCounter = 1

    /**
     * Crea el canal de notificación. Esencial para Android 8.0+
     */
    fun createNotificationChannel() {
        val channel = NotificationChannel(
            channelId,
            channelName,
            NotificationManager.IMPORTANCE_HIGH
        ).apply {
            description = "Notificaciones para eventos críticos del estacionamiento"
        }
        notificationManager.createNotificationChannel(channel)
    }

    /**
     * Construye y muestra la notificación local.
     */
    fun sendNotification(title: String, body: String) {
        val notificationBuilder = NotificationCompat.Builder(context, channelId)
            .setSmallIcon(R.mipmap.ic_launcher) // Reemplaza con tu ícono
            .setContentTitle(title)
            .setContentText(body)
            .setStyle(NotificationCompat.BigTextStyle().bigText(body)) // Para texto largo
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setAutoCancel(true)

        // Usamos un ID incremental para que se muestren varias notificaciones
        notificationManager.notify(notificationIdCounter++, notificationBuilder.build())
    }
}