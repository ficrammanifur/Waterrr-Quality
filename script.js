<script>
// ============================================================
// REAL-TIME DASHBOARD - ESP32 MQTT (Optimized for GitHub Pages)
// ============================================================

const CONFIG = {
    mqtt: {
        broker: 'wss://broker.hivemq.com:8000/mqtt',
        clientId: 'dashboard_' + Math.random().toString(16).substr(2, 10),
        topics: ['watermon/all']
    },
    maxDataPoints: 60,
    reconnectInterval: 5000,
    timeout: 12000
};

let state = {
    connected: false,
    mqttClient: null,
    dataCount: 0,
    lastData: null,
    charts: {
        ph: { labels: [], values: [] },
        tds: { labels: [], values: [] }
    },
    isDemo: false
};

const DOM = {
    mqttStatus: document.getElementById('mqttStatus'),
    espStatus: document.getElementById('espStatus'),
    connectionStatus: document.getElementById('connectionStatus'),
    lastUpdate: document.getElementById('lastUpdate'),
    lastMessage: document.getElementById('lastMessage'),
    dataCount: document.getElementById('dataCount'),
    filterStatus: document.getElementById('filterStatus'),
   
    waterStatus: document.getElementById('waterStatus'),
    statusDetail: document.getElementById('statusDetail'),
   
    filterHealth: document.getElementById('filterHealth'),
    healthBar: document.getElementById('healthBar'),
    daysLeft: document.getElementById('daysLeft'),
    volumeTotal: document.getElementById('volumeTotal'),
   
    phValue: document.getElementById('phValue'),
    phStatus: document.getElementById('phStatus'),
    tdsValue: document.getElementById('tdsValue'),
    tdsStatus: document.getElementById('tdsStatus'),
    turbidityValue: document.getElementById('turbidityValue'),
    turbidityStatus: document.getElementById('turbidityStatus'),
    tempValue: document.getElementById('tempValue'),
    tempStatus: document.getElementById('tempStatus'),
    flowRate: document.getElementById('flowRate'),
    volumeValue: document.getElementById('volumeValue')
};

// ==================== CHART INIT ====================
function initCharts() {
    const phCtx = document.getElementById('phChart').getContext('2d');
    const tdsCtx = document.getElementById('tdsChart').getContext('2d');

    const phChart = new Chart(phCtx, {
        type: 'line',
        data: { labels: [], datasets: [{ label: 'pH', data: [], borderColor: '#4299e1', backgroundColor: 'rgba(66, 153, 225, 0.1)', borderWidth: 2, tension: 0.4, fill: true }] },
        options: { responsive: true, maintainAspectRatio: false, animation: { duration: 300 }, scales: { y: { min: 0, max: 14 }, x: { grid: { display: false } } } }
    });

    const tdsChart = new Chart(tdsCtx, {
        type: 'line',
        data: { labels: [], datasets: [{ label: 'TDS', data: [], borderColor: '#48bb78', backgroundColor: 'rgba(72, 187, 120, 0.1)', borderWidth: 2, tension: 0.4, fill: true }] },
        options: { responsive: true, maintainAspectRatio: false, animation: { duration: 300 }, scales: { y: { min: 0, max: 500 }, x: { grid: { display: false } } } }
    });

    return { phChart, tdsChart };
}

const charts = initCharts();

// ==================== MQTT ====================
function initMQTT() {
    try {
        state.mqttClient = new Paho.MQTT.Client(CONFIG.mqtt.broker, CONFIG.mqtt.clientId);
        state.mqttClient.onConnectionLost = onConnectionLost;
        state.mqttClient.onMessageArrived = onMessageArrived;
        connectMQTT();
    } catch (e) {
        console.error('MQTT Init Error:', e);
        setTimeout(initMQTT, 3000);
    }
}

function connectMQTT() {
    if (!state.mqttClient) return;

    const options = {
        timeout: 10,
        onSuccess: () => {
            console.log('✅ MQTT Connected to HiveMQ!');
            state.connected = true;
            updateConnectionStatus(true);

            CONFIG.mqtt.topics.forEach(topic => {
                state.mqttClient.subscribe(topic);
                console.log(`📡 Subscribed: ${topic}`);
            });
        },
        onFailure: (err) => {
            console.warn('❌ MQTT Connect Failed:', err.errorMessage);
            state.connected = false;
            updateConnectionStatus(false);
            setTimeout(connectMQTT, CONFIG.reconnectInterval);
        }
    };

    try {
        state.mqttClient.connect(options);
    } catch (e) {
        console.error(e);
        setTimeout(connectMQTT, CONFIG.reconnectInterval);
    }
}

function onConnectionLost(response) {
    state.connected = false;
    updateConnectionStatus(false);
    console.log('⚠️ Connection lost:', response.errorMessage);
    setTimeout(connectMQTT, CONFIG.reconnectInterval);
}

function onMessageArrived(message) {
    try {
        const data = JSON.parse(message.payloadString);
        console.log('📨 Data diterima:', data);

        state.lastData = data;
        state.dataCount++;
        
        DOM.dataCount.textContent = `📊 ${state.dataCount} data points`;
        DOM.lastMessage.textContent = `📨 Last: ${new Date().toLocaleTimeString('id-ID')}`;

        updateDashboard(data);
        updateCharts(data);
        updateESPStatus(data);

    } catch (e) {
        console.error('❌ Parse error:', e);
    }
}

// ==================== UPDATE FUNCTIONS ====================
function updateConnectionStatus(connected) {
    if (connected) {
        DOM.connectionStatus.textContent = '● Online';
        DOM.connectionStatus.className = 'badge badge-success';
        DOM.mqttStatus.textContent = 'MQTT: Online';
        DOM.mqttStatus.className = 'status-badge online';
    } else {
        DOM.connectionStatus.textContent = '● Connecting...';
        DOM.connectionStatus.className = 'badge badge-warning';
        DOM.mqttStatus.textContent = 'MQTT: Offline';
        DOM.mqttStatus.className = 'status-badge offline';
    }
}

function updateESPStatus(data) {
    DOM.espStatus.textContent = 'ESP32: Online';
    DOM.espStatus.className = 'status-badge online';
}

// ... (updateDashboard dan updateCharts tetap sama seperti kode sebelumnya, tapi saya ringkas di bawah)

function updateDashboard(data) {
    DOM.lastUpdate.textContent = `⏱️ ${new Date().toLocaleTimeString('id-ID')}`;

    // Status Air
    const statusText = DOM.waterStatus.querySelector('.status-text');
    const statusIcon = DOM.waterStatus.querySelector('.status-icon');
    
    if (data.status === 'LAYAK') {
        statusText.textContent = 'LAYAK';
        statusText.className = 'status-text layak';
        statusIcon.textContent = '✅';
        DOM.statusDetail.textContent = '✨ Air layak minum';
        DOM.statusDetail.style.color = '#48bb78';
    } else {
        statusText.textContent = 'TIDAK LAYAK';
        statusText.className = 'status-text tidak-layak';
        statusIcon.textContent = '❌';
        DOM.statusDetail.textContent = '⚠️ Air tidak layak minum';
        DOM.statusDetail.style.color = '#fc8181';
    }

    // Filter Health
    if (data.health !== undefined) {
        const health = parseFloat(data.health);
        DOM.filterHealth.textContent = `${health.toFixed(0)}%`;
        DOM.healthBar.style.width = `${Math.min(health, 100)}%`;
        DOM.healthBar.className = `progress-fill ${health >= 70 ? 'good' : health >= 40 ? 'warning' : 'danger'}`;
    }

    if (data.days_left !== undefined) DOM.daysLeft.textContent = `📅 ${data.days_left} Hari`;
    if (data.volume !== undefined) {
        DOM.volumeTotal.textContent = `💧 ${data.volume.toFixed(1)} L`;
        DOM.volumeValue.textContent = data.volume.toFixed(1);
    }

    // pH, TDS, Turbidity, Temp, Flow (sama seperti kode lama kamu)
    if (data.ph !== undefined) {
        DOM.phValue.textContent = data.ph.toFixed(2);
        // ... (lanjutkan sesuai kode lama)
    }
    // (Anda bisa copy bagian updateDashboard dari kode lama Anda untuk sisanya)
}

// Update Charts (sama seperti sebelumnya)
function updateCharts(data) {
    const now = new Date().toLocaleTimeString('id-ID', {hour:'2-digit', minute:'2-digit'});

    if (data.ph !== undefined) {
        state.charts.ph.labels.push(now);
        state.charts.ph.values.push(parseFloat(data.ph));
        if (state.charts.ph.labels.length > CONFIG.maxDataPoints) {
            state.charts.ph.labels.shift();
            state.charts.ph.values.shift();
        }
        charts.phChart.data.labels = state.charts.ph.labels;
        charts.phChart.data.datasets[0].data = state.charts.ph.values;
        charts.phChart.update('none');
    }

    if (data.tds !== undefined) {
        state.charts.tds.labels.push(now);
        state.charts.tds.values.push(parseFloat(data.tds));
        if (state.charts.tds.labels.length > CONFIG.maxDataPoints) {
            state.charts.tds.labels.shift();
            state.charts.tds.values.shift();
        }
        charts.tdsChart.data.labels = state.charts.tds.labels;
        charts.tdsChart.data.datasets[0].data = state.charts.tds.values;
        charts.tdsChart.update('none');
    }
}

// ==================== INIT ====================
console.log('🚀 Smart RO Dashboard Started');
initMQTT();

setTimeout(() => {
    if (!state.connected) {
        console.warn('⚠️ Starting Demo Mode...');
        // startDemoMode(); // uncomment jika ingin demo
    }
}, CONFIG.timeout);
</script>
