package com.example.parking_iot_app.viewmodel
import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.LiveData
import com.example.parking_iot_app.data.ParkingData
import com.example.parking_iot_app.repositorio.FirebaseRepository


class ParkingViewModel(application: Application) : AndroidViewModel(application) {

    // ¡CAMBIO! Pasamos el "application.applicationContext" al repositorio
    private val repository = FirebaseRepository(application.applicationContext)

    // La UI observará esta variable (sin cambios)
    val parkingStatus: LiveData<ParkingData> = repository.parkingStatus
}