// ============================================================
//   DASHBOARD SCRIPT - FIXED VERSION
//   MQTT over WebSocket dengan Paho MQTT
// ============================================================

// ==================== CONFIGURATION ====================
const MQTT_CONFIG = {
    broker: 'wss://broker.hivemq.com:8000/mqtt',
    clientId: 'dashboard_' + Math.random().toString(16).substr(2, 8),
    topics: ['watermon/all']
};

// ==================== STATE ====================
let chartData = {
    ph: {
        labels: [],
        values: []
    },
    tds: {
        labels: [],
        values: []
    }
};

let isConnected = false;
let mqttClient = null;

// ==================== CHART INITIALIZATION ====================
function initCharts() {
    const ctxPh = document.getElementById('phChart').getContext('2d');
    const ctxTds = document.getElementById('tdsChart').getContext('2d');

    const phChart = new Chart(ctxPh, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'pH',
                data: [],
                borderColor: '#4299e1',
                backgroundColor: 'rgba(66, 153, 225, 0.1)',
                tension: 0.3,
                fill: true,
                pointRadius: 2
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: { display: false }
            },
            scales: {
                y: {
                    min: 0,
                    max: 14,
                    title: {
                        display: true,
                        text: 'pH'
                    }
                },
                x: {
                    ticks: {
                        maxTicksLimit: 10,
                        font: { size: 8 }
                    }
                }
            }
        }
    });

    const tdsChart = new Chart(ctxTds, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'TDS',
                data: [],
                borderColor: '#48bb78',
                backgroundColor: 'rgba(72, 187, 120, 0.1)',
                tension: 0.3,
                fill: true,
                pointRadius: 2
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: { display: false }
            },
            scales: {
                y: {
                    min: 0,
                    max: 500,
                    title: {
                        display: true,
                        text: 'ppm'
                    }
                },
                x: {
                    ticks: {
                        maxTicksLimit: 10,
                        font: { size: 8 }
                    }
                }
            }
        }
    });

    return { phChart, tdsChart };
}

// ==================== INITIALIZE ====================
const { phChart, tdsChart } = initCharts();

// ==================== MQTT CLIENT ====================
function initMQTT() {
    try {
        // Check if Paho is loaded
        if (typeof Paho === 'undefined') {
            console.error('Paho MQTT library not loaded!');
            updateConnectionStatus(false);
            setTimeout(initMQTT, 3000);
            return;
        }

        mqttClient = new Paho.MQTT.Client(
            MQTT_CONFIG.broker,
            MQTT_CONFIG.clientId
        );

        mqttClient.onConnectionLost = (response) => {
            isConnected = false;
            updateConnectionStatus(false);
            console.log('MQTT Connection Lost:', response.errorMessage);
            // Auto reconnect after 5 seconds
            setTimeout(initMQTT, 5000);
        };

        mqttClient.onMessageArrived = (message) => {
            try {
                const data = JSON.parse(message.payloadString);
                updateDashboard(data);
            } catch (e) {
                console.error('Error parsing MQTT message:', e);
            }
        };

        connectMQTT();
    } catch (e) {
        console.error('Error initializing MQTT:', e);
        updateConnectionStatus(false);
        setTimeout(initMQTT, 5000);
    }
}

function connectMQTT() {
    if (!mqttClient) return;

    const options = {
        timeout: 10,
        onSuccess: () => {
            isConnected = true;
            updateConnectionStatus(true);
            console.log('MQTT Connected');
            
            // Subscribe to topics
            MQTT_CONFIG.topics.forEach(topic => {
                mqttClient.subscribe(topic);
                console.log(`Subscribed to: ${topic}`);
            });
        },
        onFailure: (error) => {
            isConnected = false;
            updateConnectionStatus(false);
            console.error('MQTT Connection Failed:', error.errorMessage);
            
            // Reconnect after 5 seconds
            setTimeout(connectMQTT, 5000);
        }
    };
    
    try {
        mqttClient.connect(options);
    } catch (e) {
        console.error('MQTT Connect error:', e);
        setTimeout(connectMQTT, 5000);
    }
}

// ==================== DASHBOARD UPDATE ====================
function updateDashboard(data) {
    try {
        // Update timestamp
        const now = new Date();
        document.getElementById('lastUpdate').textContent = 
            `Last Update: ${now.toLocaleString('id-ID')}`;
        
        // Status Air
        const statusElement = document.getElementById('waterStatus');
        const statusText = statusElement.querySelector('.status-text');
        const statusIcon = statusElement.querySelector('.status-icon');
        
        if (data.status === 'LAYAK') {
            statusText.textContent = 'LAYAK';
            statusText.className = 'status-text layak';
            statusIcon.textContent = '✅';
            document.getElementById('statusDetail').textContent = 'Air layak minum';
        } else {
            statusText.textContent = 'TIDAK LAYAK';
            statusText.className = 'status-text tidak-layak';
            statusIcon.textContent = '❌';
            document.getElementById('statusDetail').textContent = 'Air tidak layak minum';
        }
        
        // pH
        if (data.ph !== undefined) {
            document.getElementById('phValue').textContent = data.ph.toFixed(2);
            const phStatus = document.getElementById('phStatus');
            if (data.ph >= 6.5 && data.ph <= 8.5) {
                phStatus.textContent = 'Normal';
                phStatus.className = 'status-indicator normal';
            } else {
                phStatus.textContent = 'Tidak Normal';
                phStatus.className = 'status-indicator danger';
            }
            updateChart('ph', data.ph);
        }
        
        // TDS
        if (data.tds !== undefined) {
            document.getElementById('tdsValue').textContent = data.tds.toFixed(0);
            const tdsStatus = document.getElementById('tdsStatus');
            if (data.tds <= 500) {
                tdsStatus.textContent = 'Normal';
                tdsStatus.className = 'status-indicator normal';
            } else {
                tdsStatus.textContent = 'Tinggi';
                tdsStatus.className = 'status-indicator danger';
            }
            updateChart('tds', data.tds);
        }
        
        // Turbidity
        if (data.turbidity !== undefined) {
            document.getElementById('turbidityValue').textContent = data.turbidity.toFixed(0);
            const turbStatus = document.getElementById('turbidityStatus');
            if (data.turbidity >= 50) {
                turbStatus.textContent = 'Jernih';
                turbStatus.className = 'status-indicator normal';
            } else {
                turbStatus.textContent = 'Keruh';
                turbStatus.className = 'status-indicator danger';
            }
        }
        
        // Temperature
        if (data.temperature !== undefined) {
            document.getElementById('tempValue').textContent = data.temperature.toFixed(1);
            const tempStatus = document.getElementById('tempStatus');
            if (data.temperature >= 15 && data.temperature <= 35) {
                tempStatus.textContent = 'Normal';
                tempStatus.className = 'status-indicator normal';
            } else {
                tempStatus.textContent = 'Tidak Normal';
                tempStatus.className = 'status-indicator warning';
            }
        }
        
        // Flow Rate
        if (data.flow_rate !== undefined) {
            document.getElementById('flowRate').textContent = data.flow_rate.toFixed(1);
        }
        
        // Filter Health
        if (data.health !== undefined) {
            const health = parseFloat(data.health);
            document.getElementById('filterHealth').textContent = `${health.toFixed(0)}%`;
            
            const bar = document.getElementById('healthBar');
            bar.style.width = `${Math.min(health, 100)}%`;
            
            if (health >= 70) {
                bar.className = 'progress-fill good';
            } else if (health >= 40) {
                bar.className = 'progress-fill warning';
            } else {
                bar.className = 'progress-fill danger';
            }
        }
        
        // Days Left
        if (data.days_left !== undefined) {
            document.getElementById('daysLeft').textContent = 
                `Estimasi: ${data.days_left} Hari`;
        }
        
        // Volume
        if (data.volume !== undefined) {
            document.getElementById('volumeTotal').textContent = 
                `Volume: ${data.volume.toFixed(1)} L`;
        }
        
        // Pump Status
        if (data.pump) {
            const pumpElement = document.getElementById('pumpStatus');
            if (data.pump === 'ON') {
                pumpElement.textContent = '🟢 ON';
                pumpElement.className = 'pump-badge on';
            } else {
                pumpElement.textContent = '🔴 OFF';
                pumpElement.className = 'pump-badge off';
            }
        }
    } catch (e) {
        console.error('Error updating dashboard:', e);
    }
}

function updateConnectionStatus(connected) {
    const status = document.getElementById('mqttStatus');
    if (connected) {
        status.textContent = 'MQTT: Online';
        status.className = 'status-badge online';
    } else {
        status.textContent = 'MQTT: Offline';
        status.className = 'status-badge offline';
    }
}

// ==================== CHART FUNCTIONS ====================
function updateChart(type, value) {
    const now = new Date().toLocaleTimeString('id-ID');
    const data = chartData[type];
    
    data.labels.push(now);
    data.values.push(parseFloat(value));
    
    // Keep only last 30 data points
    if (data.labels.length > 30) {
        data.labels.shift();
        data.values.shift();
    }
    
    // Update chart
    const chart = type === 'ph' ? phChart : tdsChart;
    chart.data.labels = data.labels;
    chart.data.datasets[0].data = data.values;
    chart.update();
}

// ==================== DEMO DATA (if no MQTT) ====================
function generateDemoData() {
    const demo = {
        ph: 7.0 + (Math.random() - 0.5) * 0.3,
        tds: 100 + Math.random() * 50,
        turbidity: 70 + Math.random() * 20,
        temperature: 25 + (Math.random() - 0.5) * 2,
        status: Math.random() > 0.2 ? 'LAYAK' : 'TIDAK LAYAK',
        health: 80 + Math.random() * 15,
        days_left: Math.floor(Math.random() * 30) + 10,
        volume: 15000 + Math.random() * 5000,
        flow_rate: 2 + Math.random() * 3,
        pump: Math.random() > 0.3 ? 'ON' : 'OFF'
    };
    return demo;
}

// ==================== MAIN ====================
console.log('Dashboard starting...');
console.log('Chart.js version:', Chart.version);
console.log('Paho MQTT available:', typeof Paho !== 'undefined');

// Initialize MQTT
initMQTT();

// Fallback: Jika MQTT tidak bisa connect, gunakan demo data
let demoInterval = null;

setTimeout(() => {
    if (!isConnected) {
        console.log('Using demo data (MQTT not connected)');
        demoInterval = setInterval(() => {
            const demoData = generateDemoData();
            updateDashboard(demoData);
            updateConnectionStatus(false);
        }, 3000);
    }
}, 10000);

// Cleanup interval if connected
setInterval(() => {
    if (isConnected && demoInterval) {
        clearInterval(demoInterval);
        demoInterval = null;
        console.log('Demo data stopped, using real MQTT data');
    }
}, 5000);
