
// --- CONFIGURACIÓN ---
const UPDATE_INTERVAL = 3000; // Actualizar cada 3 segundos

// --- ELEMENTOS DEL DOM ---
const body = document.body;
const themeToggle = document.getElementById('theme-toggle');
const statusDisplay = document.getElementById('status-display');
const statusText = document.getElementById('status-text');
const lastUpdate = document.getElementById('last-update');
const distanciaValue = document.getElementById('distancia-value');
const temperaturaValue = document.getElementById('temperatura-value');
const humedadValue = document.getElementById('humedad-value');
const ruidoValue = document.getElementById('ruido-value');
const logsTableBody = document.querySelector('#logs-table tbody');
const ruidoAlertIcon = document.getElementById('ruido-alert-icon');

// --- OBJETOS GLOBALES ---
let distanciaGauge, temperaturaGauge, humedadGauge, ruidoGauge;
let historyChart;
let globalLogsData = [];
let isFirstLoad = true;

// =================================================
// SISTEMA DE NOTIFICACIONES TOAST
// =================================================

let toastContainer;

function initToastContainer() {
    if (!toastContainer) {
        toastContainer = document.createElement('div');
        toastContainer.className = 'toast-container';
        toastContainer.setAttribute('aria-live', 'polite');
        toastContainer.setAttribute('aria-atomic', 'true');
        document.body.appendChild(toastContainer);
    }
}

function showToast(message, type = 'info', title = '', duration = 4000) {
    initToastContainer();

    const icons = {
        'success': 'fa-circle-check',
        'error': 'fa-circle-xmark',
        'warning': 'fa-triangle-exclamation',
        'info': 'fa-circle-info'
    };

    const titles = {
        'success': title || '¡Éxito!',
        'error': title || 'Error',
        'warning': title || 'Advertencia',
        'info': title || 'Información'
    };

    const toast = document.createElement('div');
    toast.className = `toast toast-${type}`;
    toast.innerHTML = `
        <div class="toast-icon">
            <i class="fa-solid ${icons[type]}"></i>
        </div>
        <div class="toast-content">
            <div class="toast-title">${titles[type]}</div>
            <div class="toast-message">${message}</div>
        </div>
        <button class="toast-close" aria-label="Cerrar">
            <i class="fa-solid fa-xmark"></i>
        </button>
        <div class="toast-progress"></div>
    `;

    toastContainer.appendChild(toast);
    setTimeout(() => toast.classList.add('show'), 100);

    const closeBtn = toast.querySelector('.toast-close');
    closeBtn.addEventListener('click', () => removeToast(toast));

    setTimeout(() => removeToast(toast), duration);

    toast.addEventListener('click', (e) => {
        if (!e.target.closest('.toast-close')) {
            removeToast(toast);
        }
    });
}

function removeToast(toast) {
    if (!toast || !toast.parentElement) return;
    toast.classList.remove('show');
    setTimeout(() => {
        if (toast.parentElement) toast.remove();
    }, 400);
}

function updateGaugeSmooth(gauge, targetValue) {
    if (!gauge) return;

    const currentValue = gauge.value || 0;
    const difference = Math.abs(targetValue - currentValue);

    if (difference < 0.5) {
        gauge.set(targetValue);
        return;
    }

    const duration = Math.min(1000, difference * 20);
    const startTime = Date.now();
    const startValue = currentValue;

    function animate() {
        const now = Date.now();
        const elapsed = now - startTime;
        const progress = Math.min(elapsed / duration, 1);
        const easeProgress = 1 - Math.pow(1 - progress, 3);
        const newValue = startValue + (targetValue - startValue) * easeProgress;

        gauge.set(newValue);

        if (progress < 1) {
            requestAnimationFrame(animate);
        }
    }

    requestAnimationFrame(animate);
}

// =================================================
// FUNCIONES DE INICIALIZACIÓN
// =================================================

function initGauges() {
    const isDark = document.body.classList.contains('dark-theme');
    const pointerColor = isDark ? '#e4e6eb' : '#333333';
    const labelColor = isDark ? '#b0b3b8' : '#606770';
    const strokeColor = isDark ? '#3a3b3c' : '#E0E0E0';

    const gaugeOptions = {
        angle: -0.2,
        lineWidth: 0.2,
        radiusScale: 1,
        pointer: { length: 0.5, strokeWidth: 0.035, color: pointerColor },
        limitMax: false,
        limitMin: false,
        strokeColor: strokeColor,
        generateGradient: true,
        highDpiSupport: true,
        staticLabels: { font: `12px 'Inter'`, labels: [], fractionDigits: 0, color: labelColor },
    };

    // --- 1. Gauge de Distancia (Calibrado) ---
    const distGaugeEl = document.getElementById('distancia-gauge');
    distanciaGauge = new Gauge(distGaugeEl).setOptions(gaugeOptions);
    distanciaGauge.setOptions({
        staticLabels: { ...gaugeOptions.staticLabels, labels: [0, 120, 300] },
        staticZones: [
            { strokeStyle: "#dc3545", min: 0, max: 30 },
            { strokeStyle: "#ffc107", min: 30, max: 120 },
            { strokeStyle: "#28a745", min: 120, max: 300 }
        ]
    });
    distanciaGauge.maxValue = 300;
    distanciaGauge.set(0);

    // --- 2. Gauge de Temperatura (Calibrado) ---
    const tempGaugeEl = document.getElementById('temperatura-gauge');
    temperaturaGauge = new Gauge(tempGaugeEl).setOptions(gaugeOptions);
    temperaturaGauge.setOptions({
        staticLabels: { ...gaugeOptions.staticLabels, labels: [-10, 20, 50] },
        staticZones: [
            { strokeStyle: "#0d6efd", min: -10, max: 10 },
            { strokeStyle: "#28a745", min: 10, max: 30 },
            { strokeStyle: "#dc3545", min: 30, max: 50 }
        ]
    });
    temperaturaGauge.maxValue = 50;
    temperaturaGauge.set(0);

    // --- 3. Gauge de Humedad (Calibrado) ---
    const humGaugeEl = document.getElementById('humedad-gauge');
    humedadGauge = new Gauge(humGaugeEl).setOptions(gaugeOptions);
    humedadGauge.setOptions({
        staticLabels: { ...gaugeOptions.staticLabels, labels: [0, 50, 100] },
        staticZones: [
            { strokeStyle: "#ff9800", min: 0, max: 30 },
            { strokeStyle: "#28a745", min: 30, max: 70 },
            { strokeStyle: "#0d6efd", min: 70, max: 100 }
        ]
    });
    humedadGauge.maxValue = 100;
    humedadGauge.set(0);

    // --- 4. Gauge de Ruido (Calibrado) ---
    const ruidoGaugeEl = document.getElementById('ruido-gauge');
    ruidoGauge = new Gauge(ruidoGaugeEl).setOptions(gaugeOptions);
    ruidoGauge.setOptions({
        staticLabels: { ...gaugeOptions.staticLabels, labels: [0, 4095] },
        staticZones: [
            { strokeStyle: "#28a745", min: 0, max: 3500 },
            { strokeStyle: "#dc3545", min: 3500, max: 4095 }
        ]
    });
    ruidoGauge.maxValue = 4095;
    ruidoGauge.set(0);
}

/**
 * Configura el interruptor del tema y actualiza los gráficos al cambiar.
 */
function setupThemeToggle() {
    themeToggle.addEventListener('change', () => {
        body.classList.toggle('dark-theme', themeToggle.checked);
        body.classList.toggle('light-theme', !themeToggle.checked);

        const isDark = themeToggle.checked;
        const pointerColor = isDark ? '#e4e6eb' : '#333333';
        const labelColor = isDark ? '#b0b3b8' : '#606770';
        const strokeColor = isDark ? '#3a3b3c' : '#E0E0E0';
        const gridColor = isDark ? 'rgba(255, 255, 255, 0.1)' : 'rgba(0, 0, 0, 0.1)';

        // Actualizar gauges
        [distanciaGauge, temperaturaGauge, humedadGauge, ruidoGauge].forEach(gauge => {
            if (gauge) {
                gauge.setOptions({
                    pointer: { color: pointerColor },
                    staticLabels: { color: labelColor },
                    strokeColor: strokeColor
                });
            }
        });

        // Actualizar gráfica de historial
        if (historyChart) {
            historyChart.options.scales.x.ticks.color = labelColor;
            historyChart.options.scales.x.grid.color = gridColor;
            historyChart.options.scales.yDist.ticks.color = labelColor;
            historyChart.options.scales.yDist.grid.color = gridColor;
            historyChart.options.scales.yDist.title.color = labelColor;
            historyChart.options.scales.yRuido.ticks.color = labelColor;
            historyChart.options.scales.yRuido.title.color = labelColor;
            historyChart.options.plugins.legend.labels.color = labelColor;
            historyChart.update('none');
        }
    });
}
/**
 * Llama a las APIs y actualiza el UI.
 */
async function fetchDataAndUpdate() {
    try {
        const [statusRes, logsRes] = await Promise.all([
            fetch('/api/status'),
            fetch('/api/logs')
        ]);

        if (!statusRes.ok || !logsRes.ok) {
            throw new Error(`Error de red: ${statusRes.statusText}`);
        }

        const statusData = await statusRes.json();
        const logsData = await logsRes.json();

        globalLogsData = logsData;
        updateUI(statusData, logsData);

    } catch (error) {
        console.error("Error al actualizar datos:", error);
        checkHeartbeat(null);
        updateUI({ estado: "Error" }, []);
    }
}


/**
 * Función maestra que actualiza todos los componentes del UI.
 */
function updateUI(status, logs) {


    const estado = status.estado || "Inicializando";

    // 1. Tarjeta de estado
    statusText.textContent = estado;
    statusDisplay.className = `status-box ${estado.toLowerCase()}`;
    lastUpdate.textContent = status.timestamp ? new Date(status.timestamp).toLocaleString() : '--:--:--';

    // 2. Gauges y valores de texto
    const dist = parseFloat(status.distancia_cm) || 0;
    if (distanciaGauge) updateGaugeSmooth(distanciaGauge, dist);
    distanciaValue.textContent = `${status.distancia_cm || '--'} cm`;

    const temp = parseFloat(status.temperatura_c) || 0;
    if (temperaturaGauge) updateGaugeSmooth(temperaturaGauge, temp);
    temperaturaValue.textContent = `${status.temperatura_c || '--'} °C`;

    const hum = parseFloat(status.humedad_pct) || 0;
    if (humedadGauge) updateGaugeSmooth(humedadGauge, hum);
    humedadValue.textContent = `${status.humedad_pct || '--'} %`;

    const ruido = parseInt(status.nivel_sonido, 10) || 0;
    if (ruidoGauge) updateGaugeSmooth(ruidoGauge, ruido);
    ruidoValue.textContent = status.nivel_sonido || '--';

    // 3. Alerta de Ruido
    const estadoRuido = status.estado_ruido || "normal";
    if (ruidoAlertIcon) {
        ruidoAlertIcon.className = `fa-solid ${estadoRuido.toLowerCase()}`;
        if (estadoRuido === "Normal") {
            ruidoAlertIcon.classList.add('fa-bell-slash');
        } else {
            ruidoAlertIcon.classList.add('fa-bell');
        }
    }

    // 4. Gráfica de Historial y Tabla
    updateHistoryChart(logs);
    updateLogsTable(logs);

    // 5. Lógica para desactivar esqueletos
    if (isFirstLoad && status.estado !== "Inicializando" && status.estado !== "Error") {
        document.getElementById('status-display').classList.remove('skeleton');
        document.getElementById('last-update').classList.remove('skeleton', 'skeleton-text');
        distanciaValue.classList.remove('skeleton', 'skeleton-text');
        temperaturaValue.classList.remove('skeleton', 'skeleton-text');
        humedadValue.classList.remove('skeleton', 'skeleton-text');
        ruidoValue.classList.remove('skeleton', 'skeleton-text');

        // Notificación de bienvenida
        showToast('Dashboard conectado y listo', 'success', '¡Bienvenido!', 3000);
        isFirstLoad = false;
    }
}
/**
 * Actualiza la tabla de historial de logs.
 */
function updateLogsTable(logs) {
    logsTableBody.innerHTML = '';
    if (!logs || logs.length === 0) {
        logsTableBody.innerHTML = '<tr><td colspan="3">No hay eventos registrados.</td></tr>';
        return;
    }
    logs.forEach(log => {
        const row = `<tr>
            <td>${new Date(log.timestamp).toLocaleString()}</td>
            <td class="estado-${log.estado.toLowerCase()}">${log.estado}</td>
            <td>${log.distancia_cm} cm</td>
        </tr>`;
        logsTableBody.innerHTML += row;
    });
}
/**
 * Actualiza la gráfica de historial principal.
 */
function updateHistoryChart(logs) {
    const isDark = document.body.classList.contains('dark-theme');
    const gridColor = isDark ? 'rgba(255, 255, 255, 0.1)' : 'rgba(0, 0, 0, 0.1)';
    const labelColor = isDark ? '#b0b3b8' : '#606770';

    const reversedLogs = [...logs].reverse();

    const labels = reversedLogs.map(log => new Date(log.timestamp.replace(" ", "T")).toLocaleTimeString());
    const distData = reversedLogs.map(log => parseFloat(log.distancia_cm));
    const ruidoData = reversedLogs.map(log => parseInt(log.nivel_sonido));

    if (!historyChart) {
        const ctx = document.getElementById('historyChart').getContext('2d');
        historyChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: labels,
                datasets: [
                    {
                        label: 'Distancia (cm)',
                        data: distData,
                        borderColor: '#0d6efd',
                        backgroundColor: 'rgba(13, 110, 253, 0.1)',
                        fill: true,
                        yAxisID: 'yDist',
                    },
                    {
                        label: 'Nivel de Ruido',
                        data: ruidoData,
                        borderColor: '#dc3545',
                        backgroundColor: 'rgba(220, 53, 69, 0.1)',
                        fill: true,
                        yAxisID: 'yRuido',
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    x: {
                        ticks: { color: labelColor },
                        grid: { color: gridColor }
                    },
                    yDist: {
                        type: 'linear',
                        position: 'left',
                        title: { display: true, text: 'Distancia (cm)', color: labelColor },
                        ticks: { color: labelColor },
                        grid: { color: gridColor }
                    },
                    yRuido: {
                        type: 'linear',
                        position: 'right',
                        title: { display: true, text: 'Nivel de Ruido', color: labelColor },
                        ticks: { color: labelColor },
                        grid: { display: false }
                    }
                },
                plugins: {
                    legend: { labels: { color: labelColor } }
                }
            }
        });
    } else {
        historyChart.data.labels = labels;
        historyChart.data.datasets[0].data = distData;
        historyChart.data.datasets[1].data = ruidoData;
        historyChart.update();
    }
}
// ==================================================
// FUNCIÓN DEL MODAL AVANZADO (CON GRÁFICAS INTELIGENTES)
// ==================================================
function openAdvancedModal(sensorType) {
    let title, dataKey, label, borderColor, chartType;
    let chartOptions = {};

    switch (sensorType) {
        case 'distancia':
            title = 'Monitoreo Avanzado: Distancia';
            dataKey = 'distancia_cm';
            label = 'Distancia (cm)';
            borderColor = '#0d6efd';
            chartType = 'line';
            chartOptions = { fill: true, tension: 0.1 };
            break;
        case 'temperatura':
            title = 'Monitoreo Avanzado: Temperatura';
            dataKey = 'temperatura_c';
            label = 'Temperatura (°C)';
            borderColor = '#dc3545';
            chartType = 'line';
            chartOptions = { fill: true, tension: 0.1 };
            break;
        case 'humedad':
            title = 'Monitoreo Avanzado: Humedad';
            dataKey = 'humedad_pct';
            label = 'Humedad (%)';
            borderColor = '#ffc107';
            chartType = 'line';
            chartOptions = { fill: true, tension: 0.1 };
            break;
        case 'ruido':
            title = 'Monitoreo Avanzado: Nivel de Ruido';
            dataKey = 'nivel_sonido';
            label = 'Nivel de Ruido';
            borderColor = '#198754';
            chartType = 'bar';
            chartOptions = {};
            break;
        default:
            return;
    }

    const reversedLogs = [...globalLogsData].reverse();
    const labels = reversedLogs.map(log => new Date(log.timestamp.replace(" ", "T")).toLocaleTimeString());
    const data = reversedLogs.map(log => parseFloat(log[dataKey]));
    const isDark = document.body.classList.contains('dark-theme');
    const gridColor = isDark ? 'rgba(255, 255, 255, 0.1)' : 'rgba(0, 0, 0, 0.1)';
    const labelColor = isDark ? '#b0b3b8' : '#606770';
    const backgroundColor = chartType === 'bar' ? borderColor : 'rgba(13, 110, 253, 0.1)';

    Swal.fire({
        title: title,
        html: '<div class="modal-chart-container"><canvas id="modalChart"></canvas></div>',
        width: '70%',
        didOpen: () => {
            const ctx = document.getElementById('modalChart').getContext('2d');
            new Chart(ctx, {
                type: chartType,
                data: {
                    labels: labels,
                    datasets: [{
                        label: label,
                        data: data,
                        borderColor: borderColor,
                        backgroundColor: backgroundColor,
                        ...chartOptions
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        x: { ticks: { color: labelColor }, grid: { color: gridColor } },
                        y: { ticks: { color: labelColor }, grid: { color: gridColor } }
                    },
                    plugins: {
                        legend: { display: false }
                    }
                }
            });
        }
    });
}

// ==================================================
// INICIO DE LA APLICACIÓN
// ==================================================
document.addEventListener('DOMContentLoaded', () => {
    initGauges();
    setupThemeToggle();
    fetchDataAndUpdate();
    setInterval(fetchDataAndUpdate, UPDATE_INTERVAL);

    document.addEventListener('click', function (event) {
        const expandIcon = event.target.closest('.expand-icon');
        if (expandIcon) {
            const sensorType = expandIcon.getAttribute('data-sensor');
            openAdvancedModal(sensorType);
        }
    });
});