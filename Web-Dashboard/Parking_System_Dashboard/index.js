
const express = require('express');
const admin = require('firebase-admin');
const path = require('path');

// --- 1. CONFIGURACIÓN DE FIREBASE ---
const serviceAccount = require('./serviceAccountKey.json');
admin.initializeApp({
  credential: admin.credential.cert(serviceAccount),
  databaseURL: "https://parking-iot-a0b8e-default-rtdb.firebaseio.com/"
});
const db = admin.database();
const app = express();
const port = process.env.PORT || 3000; // Render usará process.env.PORT

// --- 2. CONFIGURACIÓN DE EXPRESS ---
app.set('view engine', 'ejs'); // Usar EJS como motor de plantillas
app.set('views', path.join(__dirname, 'views')); // Directorio de vistas
app.use(express.static(path.join(__dirname, 'public'))); // Servir archivos estáticos

// --- 3. RUTAS DE VISTAS (HTML) ---

// Ruta principal que muestra el dashboard "En Vivo"
app.get('/', (req, res) => {
  res.render('index'); // Renderiza el archivo views/index.ejs
});

// Ruta que muestra la página de "Análisis"
app.get('/analisis', (req, res) => {
  res.render('analisis'); // Renderiza el nuevo archivo views/analisis.ejs
});


// --- 4. RUTAS DE API (DATOS JSON) ---

// API para obtener el estado actual
app.get('/api/status', async (req, res) => {
  try {
    const statusRef = db.ref('parking_status'); 
    const snapshot = await statusRef.once('value');
    res.json(snapshot.val() || { estado: "Sin Datos" });
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// API para obtener los últimos 10 logs
app.get('/api/logs', async (req, res) => {
  try {
    const logsRef = db.ref('parking_logs').orderByKey().limitToLast(10); 
    const snapshot = await logsRef.once('value');
    
    const logs = [];
    snapshot.forEach(child => {
      logs.push(child.val());
    });
    res.json(logs.reverse()); // Enviar los más recientes primero
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// API para TODOS los logs
app.get('/api/logs/all', async (req, res) => {
    try {
        const logsRef = db.ref('parking_logs'); 
        const snapshot = await logsRef.once('value');
        
        const logs = [];
        snapshot.forEach(child => {
            logs.push(child.val());
        });
        res.json(logs); // Enviar todos los logs
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});
// --- 5. INICIO DEL SERVIDOR ---
app.listen(port, () => {
  console.log(`🚀 Dashboard listo y escuchando en http://localhost:${port}`);
  console.log('Este servidor está listo para desplegar.');
});