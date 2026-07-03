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

// ==================== CHART INITIALIZATION ====================
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
            fill: true
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
            fill: true
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
            }
        }
    }
});

// ==================== MQTT CLIENT ====================
const client = new Paho.MQTT.Client(
    MQTT_CONFIG.broker,
    MQTT_CONFIG.clientId
);

client.onConnectionLost = (response) => {
    isConnected = false;
    updateConnectionStatus(false);
    console.log('MQTT Connection Lost:', response.errorMessage);
};

client.onMessageArrived = (message) => {
    try {
        const data = JSON.parse(message.payloadString);
        updateDashboard(data);
    } catch (e) {
        console.error('Error parsing MQTT message:', e);
    }
};

function connectMQTT() {
    const options = {
        timeout: 3,
        onSuccess: () => {
            isConnected = true;
            updateConnectionStatus(true);
            console.log('MQTT Connected');
            
            // Subscribe to topics
            MQTT_CONFIG.topics.forEach(topic => {
                client.subscribe(topic);
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
    
    client.connect(options);
}

// ==================== DASHBOARD UPDATE ====================
function updateDashboard(data) {
    // Update timestamp
    document.getElementById('lastUpdate').textContent = 
        `Last Update: ${new Date().toLocaleString('id-ID')}`;
    
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
    if (data.ph) {
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
    if (data.tds) {
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
    if (data.turbidity) {
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
    if (data.temperature) {
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
    
    // Filter Health
    if (data.health !== undefined) {
        const health = data.health;
        document.getElementById('filterHealth').textContent = `${health.toFixed(0)}%`;
        
        const bar = document.getElementById('healthBar');
        bar.style.width = `${health}%`;
        
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
    data.values.push(value);
    
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

// ==================== INITIALIZE ====================
console.log('Dashboard starting...');
connectMQTT();

// Auto reconnect every 30 seconds if disconnected
setInterval(() => {
    if (!isConnected) {
        console.log('Attempting to reconnect...');
        connectMQTT();
    }
}, 30000);
