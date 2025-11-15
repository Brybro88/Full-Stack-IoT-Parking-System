// --- Variables Globales ---
let allLogs = [];
let heatmapChart;
let envTrendChart;
let noiseEventChart;
let stateDonutChart;
let timeDonutChart;

let isDark = document.body.classList.contains('dark-theme');

// --- Elementos del DOM ---
const statLibre = document.getElementById('stat-libre');
const statOcupado = document.getElementById('stat-ocupado');
const statManiobra = document.getElementById('stat-maniobra');
const filterButtons = document.querySelectorAll('.filter-btn');
const statTempAvg = document.getElementById('stat-temp-avg');
const statTempMax = document.getElementById('stat-temp-max');
const statHumAvg = document.getElementById('stat-hum-avg');
const statHumMax = document.getElementById('stat-hum-max');

/**
 * Función principal
 */
async function initAnalytics() {
    try {
        const response = await fetch('/api/logs/all');
        allLogs = await response.json();
        processData('today');
    } catch (error) {
        console.error("Error al cargar todos los logs:", error);
    }
}

/**
 * Filtra los logs y actualiza todos los componentes del dashboard
 */
function processData(filterType) {
    const filteredLogs = filterLogs(allLogs, filterType);
    const stats = calculateStats(filteredLogs);
    
    renderStats(stats);
    renderDonutChart(stats);
    renderTimeDonutChart(stats); 
    
    const heatmapData = prepareHeatmapData(filteredLogs);
    renderHeatmap(heatmapData);
    const envStats = calculateEnvironmentalStats(filteredLogs);
    renderEnvironmentalStats(envStats);
    const trendData = prepareTrendData(filteredLogs);
    renderTrendChart(trendData);
    const noiseEventData = prepareNoiseEventData(filteredLogs);
    renderNoiseEventChart(noiseEventData);
    
    filterButtons.forEach(btn => {
        btn.classList.toggle('active', btn.getAttribute('data-filter') === filterType);
    });
}

/**
 * Filtra el array de logs basado en la fecha UTC
 */
function filterLogs(logs, filterType) {
    const now = new Date();
    const today = new Date(now.getFullYear(), now.getMonth(), now.getDate());
    const sevenDaysAgo = new Date(now.getFullYear(), now.getMonth(), now.getDate() - 7);

    if (filterType === 'all') {
        return logs;
    }
    return logs.filter(log => {
        try {
            const logDate = new Date(log.timestamp);
            if (filterType === 'today') return logDate >= today;
            if (filterType === '7days') return logDate >= sevenDaysAgo;
            return false;
        } catch(e) { return false; }
    });
}

/**
 * Cuenta los eventos de estado (Libre, Ocupado, etc.)
 */
function calculateStats(logs) {
    const stats = { libre: 0, ocupado: 0, maniobra: 0 };
    logs.forEach(log => {
        if (log.estado === 'Libre') stats.libre++;
        if (log.estado === 'Ocupado') stats.ocupado++;
        if (log.estado === 'Aproximacion' || log.estado === 'Maniobra') stats.maniobra++;
    });
    return stats;
}

/**
 * Muestra las estadísticas de estado en las tarjetas
 */
function renderStats(stats) {
    statLibre.textContent = stats.libre;
    statOcupado.textContent = stats.ocupado;
    statManiobra.textContent = stats.maniobra;
}

/**
 * Agrega los logs en una matriz de 7 días x 24 horas
 */
function prepareHeatmapData(logs) {
    const data = Array(7).fill(0).map(() => Array(24).fill(0));
    logs.forEach(log => {
        if (log.estado === 'Ocupado') {
            try {
                const logDate = new Date(log.timestamp);
                const dayOfWeek = logDate.getDay();
                const hourOfDay = logDate.getHours();
                data[dayOfWeek][hourOfDay]++;
            } catch(e) { /* ignorar */ }
        }
    });
    const series = [
        { name: 'Domingo', data: data[0] }, { name: 'Lunes', data: data[1] },
        { name: 'Martes', data: data[2] }, { name: 'Miércoles', data: data[3] },
        { name: 'Jueves', data: data[4] }, { name: 'Viernes', data: data[5] },
        { name: 'Sábado', data: data[6] }
    ];
    return series;
}

/**
 * Dibuja o actualiza el gráfico Heatmap con ApexCharts
 */
function renderHeatmap(data) {
    const axisAndLegendColor = isDark ? '#e4e6eb' : '#1c1e21';
    const hours = Array(24).fill(0).map((_, i) => `${i.toString().padStart(2, '0')}h`);
    const options = {
        series: data,
        chart: { type: 'heatmap', height: 350, toolbar: { show: true } },
        plotOptions: {
            heatmap: {
                shadeIntensity: 0.5, radius: 0,
                colorScale: {
                    ranges: [
                        { from: 0, to: 0, name: 'Vacío', color: isDark ? '#3a3b3c' : '#E0E0E0' },
                        { from: 1, to: 5, name: 'Bajo', color: '#00A100' },
                        { from: 6, to: 10, name: 'Medio', color: '#FFB200' },
                        { from: 11, to: 999, name: 'Alto', color: '#D73737' }
                    ]
                }
            }
        },
        dataLabels: { enabled: false },
        xaxis: { categories: hours, labels: { style: { colors: axisAndLegendColor } } },
        yaxis: { labels: { style: { colors: axisAndLegendColor } } },
        legend: {
            labels: { colors: axisAndLegendColor, useSeriesColors: false },
            markers: { radius: 5 },
            onItemClick: { toggleDataSeries: true },
            onItemHover: { highlightDataSeries: true }
        },
        tooltip: { y: { formatter: (val) => `${val} eventos` } }
    };
    
    if (heatmapChart) {
        heatmapChart.updateOptions(options);
    } else {
        heatmapChart = new ApexCharts(document.querySelector("#heatmap-chart"), options);
        heatmapChart.render();
    }
}

/**
 * Calcula Min/Max/Promedio para Temperatura y Humedad
 */
function calculateEnvironmentalStats(logs) {
    const stats = {
        tempSum: 0, tempCount: 0, tempMax: -Infinity,
        humSum: 0, humCount: 0, humMax: -Infinity,
    };
    logs.forEach(log => {
        const temp = parseFloat(log.temperatura_c);
        const hum = parseFloat(log.humedad_pct);
        if (!isNaN(temp)) {
            stats.tempSum += temp; stats.tempCount++;
            if (temp > stats.tempMax) stats.tempMax = temp;
        }
        if (!isNaN(hum)) {
            stats.humSum += hum; stats.humCount++;
            if (hum > stats.humMax) stats.humMax = hum;
        }
    });
    return {
        tempAvg: stats.tempCount > 0 ? (stats.tempSum / stats.tempCount).toFixed(1) : '--',
        tempMax: stats.tempMax === -Infinity ? '--' : stats.tempMax.toFixed(1),
        humAvg: stats.humCount > 0 ? (stats.humSum / stats.humCount).toFixed(1) : '--',
        humMax: stats.humMax === -Infinity ? '--' : stats.humMax.toFixed(1),
    };
}

/**
 * Muestra las estadísticas ambientales en las tarjetas
 */
function renderEnvironmentalStats(stats) {
    statTempAvg.textContent = `${stats.tempAvg} °C`;
    statTempMax.textContent = `${stats.tempMax} °C`;
    statHumAvg.textContent = `${stats.humAvg} %`;
    statHumMax.textContent = `${stats.humMax} %`;
}

/**
 * Prepara los datos para la gráfica de tendencias (Temp/Hum)
 */
function prepareTrendData(logs) {
    const sortedLogs = [...logs].sort((a, b) => new Date(a.timestamp) - new Date(b.timestamp));
    const series = [
        {
            name: 'Temperatura',
            data: sortedLogs.map(log => ({
                x: new Date(log.timestamp).getTime(),
                y: parseFloat(log.temperatura_c).toFixed(1)
            }))
        },
        {
            name: 'Humedad',
            data: sortedLogs.map(log => ({
                x: new Date(log.timestamp).getTime(),
                y: parseFloat(log.humedad_pct).toFixed(1)
            }))
        }
    ];
    return series;
}

/**
 * Dibuja o actualiza la gráfica de tendencias (Temp/Hum)
 */
function renderTrendChart(data) {
    const themeColor = isDark ? '#e4e6eb' : '#1c1e21';
    const options = {
        series: data,
        chart: { type: 'line', height: 350, zoom: { enabled: true }, toolbar: { show: true }, animations: { enabled: true } },
        colors: ['#dc3545', '#0d6efd'],
        dataLabels: { enabled: false },
        stroke: { curve: 'smooth', width: 2 },
        xaxis: { type: 'datetime', labels: { style: { colors: themeColor } } },
        yaxis: [
            {
                seriesName: 'Temperatura',
                title: { text: 'Temperatura (°C)', style: { color: '#dc3545' } },
                labels: { style: { colors: themeColor } }
            },
            {
                seriesName: 'Humedad',
                opposite: true,
                title: { text: 'Humedad (%)', style: { color: '#0d6efd' } },
                labels: { style: { colors: themeColor } }
            }
        ],
        legend: { labels: { colors: themeColor } },
        tooltip: { x: { format: 'dd MMM yyyy - HH:mm' } }
    };

    if (envTrendChart) {
        envTrendChart.updateOptions(options);
    } else {
        envTrendChart = new ApexCharts(document.querySelector("#env-trend-chart"), options);
        envTrendChart.render();
    }
}

/**
 * Prepara los datos para la gráfica de eventos de ruido
 */
function prepareNoiseEventData(logs) {
    const hourlySums = Array(24).fill(0);
    const hourlyCounts = Array(24).fill(0);
    logs.forEach(log => {
        try {
            const hourOfDay = new Date(log.timestamp).getHours();
            const baseline = parseFloat(log.ruido_baseline);
            if (!isNaN(baseline)) {
                hourlySums[hourOfDay] += baseline;
                hourlyCounts[hourOfDay]++;
            }
        } catch(e) { /* ignorar */ }
    });
    const averageData = hourlySums.map((sum, hour) => {
        if (hourlyCounts[hour] === 0) return 0;
        return (sum / hourlyCounts[hour]).toFixed(0);
    });
    return [{ name: 'Ruido Promedio', data: averageData }];
}

/**
 * Dibuja o actualiza la gráfica de eventos de ruido
 */
function renderNoiseEventChart(data) {
    const themeColor = isDark ? '#e4e6eb' : '#1c1e21';
    const hours = Array(24).fill(0).map((_, i) => `${i.toString().padStart(2, '0')}h`);
    const options = {
        series: data,
        chart: { type: 'line', height: 350, toolbar: { show: true } },
        colors: ['#ffc107'],
        dataLabels: { enabled: false },
        stroke: { curve: 'smooth', width: 2 },
        xaxis: { categories: hours, labels: { style: { colors: themeColor } } },
        yaxis: {
            title: { text: 'Nivel de Ruido Promedio', style: { color: themeColor } },
            labels: { style: { colors: themeColor } }
        },
        legend: { labels: { colors: themeColor } },
        fill: { opacity: 1 },
        tooltip: { y: { formatter: (val) => `${val}` } }
    };

    if (noiseEventChart) {
        noiseEventChart.updateOptions(options);
    } else {
        noiseEventChart = new ApexCharts(document.querySelector("#noise-event-chart"), options);
        noiseEventChart.render();
    }
}

/**
 * Dibuja o actualiza la gráfica de Dona (Distribución de Estados)
 */
function renderDonutChart(stats) {
    const themeColor = isDark ? '#e4e6eb' : '#1c1e21';
    const options = {
        series: [stats.libre, stats.ocupado, stats.maniobra],
        chart: { type: 'donut', height: 380 },
        labels: ['Eventos "Libre"', 'Eventos "Ocupado"', 'Eventos "Aprox/Maniobra"'],
        colors: ['#28a745', '#dc3545', '#ffc107'],
        legend: {
            position: 'bottom',
            labels: { colors: themeColor }
        },
        dataLabels: {
            enabled: true,
            formatter: function (val) { return val.toFixed(1) + "%" }
        },
        tooltip: { y: { formatter: (val) => `${val} eventos` } }
    };

    if (stateDonutChart) {
        stateDonutChart.updateOptions(options);
    } else {
        stateDonutChart = new ApexCharts(document.querySelector("#state-donut-chart"), options);
        stateDonutChart.render();
    }
}
/**
 * Dibuja la gráfica de dona para el Porcentaje de Tiempo
 */
function renderTimeDonutChart(stats) {
    const themeColor = isDark ? '#e4e6eb' : '#1c1e21';
    const totalEvents = stats.libre + stats.ocupado + stats.maniobra;

    // Calcular porcentajes
    const pctOcupado = totalEvents > 0 ? (stats.ocupado / totalEvents) * 100 : 0;
    const pctLibre = totalEvents > 0 ? (stats.libre / totalEvents) * 100 : 0;
    const pctManiobra = totalEvents > 0 ? (stats.maniobra / totalEvents) * 100 : 0;

    const options = {
        series: [pctLibre, pctOcupado, pctManiobra],
        chart: {
            type: 'donut',
            height: 380 // (Ajustado desde 250 a 380 para consistencia)
        },
        labels: ['% Tiempo Libre', '% Tiempo Ocupado', '% Maniobra'],
        colors: ['#28a745', '#dc3545', '#ffc107'],
        legend: {
            position: 'bottom',
            labels: { colors: themeColor }
        },
        dataLabels: { enabled: false }, 
        tooltip: {
            y: {
                formatter: (val) => val.toFixed(1) + " %"
            }
        }
    };

    // Asegurarse de que el elemento exista antes de intentar renderizar
    const chartEl = document.querySelector("#time-donut-chart");
    if (!chartEl) {
        console.error("#time-donut-chart no encontrado!");
        return;
    }

    if (timeDonutChart) {
        timeDonutChart.updateOptions(options);
    } else {
        timeDonutChart = new ApexCharts(chartEl, options);
        timeDonutChart.render();
    }
}


// --- INICIO DE LA APLICACIÓN DE ANÁLISIS ---
document.addEventListener('DOMContentLoaded', () => {
    initAnalytics();
    
    filterButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            const filter = btn.getAttribute('data-filter');
            processData(filter);
        });
    });

    const themeToggle = document.getElementById('theme-toggle');
    themeToggle.addEventListener('change', () => {
        
        document.body.classList.toggle('dark-theme', themeToggle.checked);
        document.body.classList.toggle('light-theme', !themeToggle.checked);
        isDark = themeToggle.checked;

        const axisAndLegendColor = isDark ? '#e4e6eb' : '#1c1e21';
        
        // Actualizar TODAS las gráficas existentes con los nuevos colores
        if (heatmapChart) {
            heatmapChart.updateOptions({
                xaxis: { labels: { style: { colors: axisAndLegendColor } } },
                yaxis: { labels: { style: { colors: axisAndLegendColor } } },
                legend: { labels: { colors: axisAndLegendColor } }
            });
        }
        if (envTrendChart) {
            envTrendChart.updateOptions({
                xaxis: { labels: { style: { colors: axisAndLegendColor } } },
                yaxis: [
                    { labels: { style: { colors: axisAndLegendColor } } },
                    { labels: { style: { colors: axisAndLegendColor } } }
                ],
                legend: { labels: { colors: axisAndLegendColor } }
            });
        }
        if (noiseEventChart) {
            noiseEventChart.updateOptions({
                xaxis: { labels: { style: { colors: axisAndLegendColor } } },
                yaxis: {
                    title: { style: { color: axisAndLegendColor } },
                    labels: { style: { colors: axisAndLegendColor } }
                },
                legend: { labels: { colors: axisAndLegendColor } }
            });
        }
        if (stateDonutChart) {
            stateDonutChart.updateOptions({
                legend: { labels: { colors: axisAndLegendColor } }
            });
        }
        if (timeDonutChart) {
             timeDonutChart.updateOptions({
                legend: { labels: { colors: axisAndLegendColor } }
            });
        }
    });
});s