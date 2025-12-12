// ==================================================
// MEJORAS DE CLASE MUNDIAL PARA CHART.JS
// ==================================================

// Esta función reemplaza la función updateHistoryChart original
// Incluye gradientes futuristas degradados y mejor diseño visual

/**
 * Actualiza la gráfica de historial principal con gradientes futuristas.
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
        
        // Crear gradientes futuristas degradados
        const distanciaGradient = ctx.createLinearGradient(0, 0, 0, 250);
        distanciaGradient.addColorStop(0, 'rgba(13, 110, 253, 0.4)');   // Azul intenso arriba
        distanciaGradient.addColorStop(0.5, 'rgba(13, 110, 253, 0.2)'); // Medio
        distanciaGradient.addColorStop(1, 'rgba(13, 110, 253, 0.0)');   // Transparente abajo
        
        const ruidoGradient = ctx.createLinearGradient(0, 0, 0, 250);
        ruidoGradient.addColorStop(0, 'rgba(220, 53, 69, 0.4)');   // Rojo intenso arriba
        ruidoGradient.addColorStop(0.5, 'rgba(220, 53, 69, 0.2)'); // Medio
        ruidoGradient.addColorStop(1, 'rgba(220, 53, 69, 0.0)');   // Transparente abajo

        historyChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: labels,
                datasets: [
                    {
                        label: 'Distancia (cm)',
                        data: distData,
                        borderColor: '#0d6efd',
                        backgroundColor: distanciaGradient, // Gradiente degradado futurista
                        fill: true,
                        yAxisID: 'yDist',
                        tension: 0.4, // Curvas suaves para look futurista
                        borderWidth: 2,
                        pointRadius: 3,
                        pointHoverRadius: 5,
                        pointBackgroundColor: '#0d6efd',
                        pointBorderColor: '#fff',
                        pointBorderWidth: 2,
                    },
                    {
                        label: 'Nivel de Ruido',
                        data: ruidoData,
                        borderColor: '#dc3545',
                        backgroundColor: ruidoGradient, // Gradiente degradado futurista
                        fill: true,
                        yAxisID: 'yRuido',
                        tension: 0.4, // Curvas suaves para look futurista
                        borderWidth: 2,
                        pointRadius: 3,
                        pointHoverRadius: 5,
                        pointBackgroundColor: '#dc3545',
                        pointBorderColor: '#fff',
                        pointBorderWidth: 2,
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                interaction: {
                    mode: 'index',
                    intersect: false,
                },
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
                    legend: { labels: { color: labelColor } },
                    tooltip: {
                        backgroundColor: isDark ? 'rgba(30, 41, 59, 0.95)' : 'rgba(255, 255, 255, 0.95)',
                        titleColor: labelColor,
                        bodyColor: labelColor,
                        borderColor: isDark ? 'rgba(71, 85, 105, 0.5)' : 'rgba(226, 232, 240, 0.8)',
                        borderWidth: 1,
                    }
                }
            }
        });
    } else {
        // Actualizar gradientes al cambiar los datos
        const ctx = historyChart.ctx;
        
        const distanciaGradient = ctx.createLinearGradient(0, 0, 0, 250);
        distanciaGradient.addColorStop(0, 'rgba(13, 110, 253, 0.4)');
        distanciaGradient.addColorStop(0.5, 'rgba(13, 110, 253, 0.2)');
        distanciaGradient.addColorStop(1, 'rgba(13, 110, 253, 0.0)');
        
        const ruidoGradient = ctx.createLinearGradient(0, 0, 0, 250);
        ruidoGradient.addColorStop(0, 'rgba(220, 53, 69, 0.4)');
        ruidoGradient.addColorStop(0.5, 'rgba(220, 53, 69, 0.2)');
        ruidoGradient.addColorStop(1, 'rgba(220, 53, 69, 0.0)');

        historyChart.data.labels = labels;
        historyChart.data.datasets[0].data = distData;
        historyChart.data.datasets[0].backgroundColor = distanciaGradient;
        historyChart.data.datasets[1].data = ruidoData;
        historyChart.data.datasets[1].backgroundColor = ruidoGradient;
        historyChart.update();
    }
}
