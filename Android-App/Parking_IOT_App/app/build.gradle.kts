plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.google.gms.google.services)
}

android {
    buildFeatures {
        viewBinding = true
    }

    namespace = "com.example.parking_iot_app"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.example.parking_iot_app"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    kotlinOptions {
        jvmTarget = "11"
    }
}

dependencies {

    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("com.google.android.material:material:1.12.0")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")

    // 1. Firebase BOM (Bill of Materials)
    // Esta línea gestiona las versiones de TODAS las demás librerías de Firebase.
    implementation(platform("com.google.firebase:firebase-bom:33.1.0"))

    // 2. Librerías de Firebase (SIN versión específica)
    implementation("com.google.firebase:firebase-database-ktx")

    // 3. Arquitectura (ViewModel y LiveData)
    implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.8.1")
    implementation("androidx.lifecycle:lifecycle-livedata-ktx:2.8.1")
    implementation("androidx.activity:activity-ktx:1.9.0") // Para 'by viewModels' y permisos

    // 4. UI (GridLayout)
    implementation("androidx.gridlayout:gridlayout:1.0.0")
    implementation(libs.androidx.lifecycle.viewmodel.ktx)
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.1.5")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.5.1")
}