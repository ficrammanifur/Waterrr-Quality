// ==================== CONFIG ====================
const MQTT_BROKER = "wss://broker.hivemq.com:8884/mqtt";
const MQTT_TOPIC = "watermon/all";

let client = null;
let messageCount = 0;

const DOM = {
    connectionStatus: document.getElementById('connectionStatus'),
    mqttStatus: document.getElementById('mqttStatus'),
    espStatus: document.getElementById('espStatus'),
    lastUpdate: document.getElementById('lastUpdate'),
    dataCount: document.getElementById('dataCount'),
    lastMessage: document.getElementById('lastMessage'),
    waterStatus: document.getElementById('waterStatus'),
    statusDetail: document.getElementById('statusDetail'),
    filterHealth: document.getElementById('filterHealth'),
    healthBar: document.getElementById('healthBar'),
    daysLeft: document.getElementById('daysLeft'),
    volumeTotal: document.getElementById('volumeTotal'),
    phValue: document.getElementById('phValue'),
    tdsValue: document.getElementById('tdsValue'),
    turbidityValue: document.getElementById('turbidityValue'),
    tempValue: document.getElementById('tempValue'),
    flowRate: document.getElementById('flowRate'),
    volumeValue: document.getElementById('volumeValue')
};

let charts = null;

// ==================== CHARTS ====================
function initCharts() {
    charts = {
        ph: new Chart(document.getElementById('phChart'), {
            type: 'line',
            data: { labels: [], datasets: [{ label: 'pH', data: [], borderColor: '#4299e1', tension: 0.4, fill: true }] },
            options: { responsive: true, scales: { y: { min: 0, max: 14 } } }
        }),
        tds: new Chart(document.getElementById('tdsChart'), {
            type: 'line',
            data: { labels: [], datasets: [{ label: 'TDS', data: [], borderColor: '#48bb78', tension: 0.4, fill: true }] },
            options: { responsive: true, scales: { y: { min: 0, max: 500 } } }
        })
    };
}

// ==================== MQTT ====================
function connectToMQTT() {
    console.log("🔌 Connecting to HiveMQ...");

    client = mqtt.connect(MQTT_BROKER, {
        clientId: 'dashboard_' + Math.random().toString(16).substr(2, 8),
        reconnectPeriod: 3000
    });

    client.on("connect", () => {
        console.log("✅ Connected to MQTT Broker!");
        DOM.mqttStatus.textContent = "MQTT: Online";
        DOM.mqttStatus.className = "status-badge online";
        DOM.connectionStatus.textContent = "● Online";
        DOM.connectionStatus.className = "badge badge-success";

        client.subscribe(MQTT_TOPIC);
    });

    client.on("message", (topic, message) => {
        try {
            const data = JSON.parse(message.toString());
            console.log("📨 Data received:", data);

            messageCount++;
            DOM.dataCount.textContent = `📊 ${messageCount} data points`;
            DOM.lastMessage.textContent = `📨 Last: ${new Date().toLocaleTimeString('id-ID')}`;

            updateDashboard(data);
            updateCharts(data);
        } catch (e) {
            console.error("Parse error:", e);
        }
    });
}

// ==================== UPDATE DASHBOARD ====================
function updateDashboard(data) {
    DOM.lastUpdate.textContent = `⏱️ ${new Date().toLocaleTimeString('id-ID')}`;

    if (data.ph !== undefined) DOM.phValue.textContent = data.ph.toFixed(2);
    if (data.tds !== undefined) DOM.tdsValue.textContent = data.tds.toFixed(0);
    if (data.turbidity !== undefined) DOM.turbidityValue.textContent = data.turbidity.toFixed(0);
    if (data.temperature !== undefined) DOM.tempValue.textContent = data.temperature.toFixed(1);
    if (data.flow_rate !== undefined) DOM.flowRate.textContent = data.flow_rate.toFixed(1);
    if (data.volume !== undefined) {
        DOM.volumeValue.textContent = data.volume.toFixed(1);
        DOM.volumeTotal.textContent = `💧 ${data.volume.toFixed(1)} L`;
    }

    // Status Air
    const statusText = DOM.waterStatus.querySelector('.status-text');
    if (statusText) {
        if (data.status === 'LAYAK') {
            statusText.textContent = 'LAYAK';
            statusText.className = 'status-text layak';
        } else {
            statusText.textContent = 'TIDAK LAYAK';
            statusText.className = 'status-text tidak-layak';
        }
    }
}

function updateCharts(data) {
    const now = new Date().toLocaleTimeString('id-ID', {hour:'2-digit', minute:'2-digit'});
    if (data.ph !== undefined && charts) {
        charts.ph.data.labels.push(now);
        charts.ph.data.datasets[0].data.push(data.ph);
        if (charts.ph.data.labels.length > 60) {
            charts.ph.data.labels.shift();
            charts.ph.data.datasets[0].data.shift();
        }
        charts.ph.update('none');
    }
    if (data.tds !== undefined && charts) {
        charts.tds.data.labels.push(now);
        charts.tds.data.datasets[0].data.push(data.tds);
        if (charts.tds.data.labels.length > 60) {
            charts.tds.data.labels.shift();
            charts.tds.data.datasets[0].data.shift();
        }
        charts.tds.update('none');
    }
}

// ==================== START ====================
document.addEventListener("DOMContentLoaded", () => {
    console.log("🚀 Smart RO Dashboard Started");
    initCharts();
    connectToMQTT();
});
