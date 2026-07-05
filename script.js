// script.js
// ==================== CONFIG ====================
const MQTT_BROKER = "wss://broker.hivemq.com:8884/mqtt";
const MQTT_TOPIC = "watermon/all";

let client = null;
let messageCount = 0;

// Theme colors for JS manipulation
const THEME = {
    accent: '#00F0FF',
    success: '#00FF66',
    warning: '#FFD700',
    danger: '#FF2A54',
    bg: 'rgba(255,255,255,0.05)',
    textMuted: 'rgba(255,255,255,0.4)'
};

// Cache DOM elements
const DOM = {
    connContainer: document.getElementById('connectionContainer'),
    connDot: document.getElementById('connectionDot'),
    connText: document.getElementById('connectionText'),

    mqttBadge: document.getElementById('mqttBadge'),
    espBadge: document.getElementById('espBadge'),
    lastUpdate: document.getElementById('lastUpdate'),
    dataCount: document.getElementById('dataCount'),
    lastMessage: document.getElementById('lastMessage'),

    waterStatusText: document.getElementById('waterStatusText'),
    statusIconWrapper: document.getElementById('statusIconWrapper'),
    statusDetail: document.getElementById('statusDetail'),

    filterHealth: document.getElementById('filterHealth'),
    healthBar: document.getElementById('healthBar'),
    daysLeft: document.getElementById('daysLeft'),
    volumeTotal: document.getElementById('volumeTotal'),

    phValue: document.getElementById('phValue'),
    tdsValue: document.getElementById('tdsValue'),
    turbidityValue: document.getElementById('turbidityValue'),
    tempValue: document.getElementById('tempValue'),

    phBadge: document.getElementById('phBadge'),
    tdsBadge: document.getElementById('tdsBadge'),
    turbBadge: document.getElementById('turbBadge'),
    tempBadge: document.getElementById('tempBadge'),
};

let charts = null;

// ==================== CHARTS CONFIG ====================
// Global overrides for Chart.js to fit the cyber/glass theme
Chart.defaults.color = 'rgba(255,255,255,0.5)';
Chart.defaults.font.family = "'JetBrains Mono', monospace";
Chart.defaults.font.size = 10;

function initCharts() {
    const createChartOptions = (borderColor, bgColor) => ({
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
            legend: { display: false },
            tooltip: {
                mode: 'index',
                intersect: false,
                backgroundColor: 'rgba(6, 19, 37, 0.9)',
                titleColor: '#fff',
                bodyColor: borderColor,
                borderColor: 'rgba(255,255,255,0.1)',
                borderWidth: 1,
                padding: 10,
                displayColors: false,
                callbacks: {
                    label: function(context) {
                        return `Value: ${context.parsed.y}`;
                    }
                }
            }
        },
        scales: {
            x: {
                grid: { display: false, drawBorder: false },
                ticks: { maxTicksLimit: 6, maxRotation: 0 }
            },
            y: {
                beginAtZero: false,
                grid: { color: 'rgba(255,255,255,0.05)', borderDash: [2, 4], drawBorder: false }
            }
        },
        elements: {
            line: {
                tension: 0.4,
                borderWidth: 2,
                shadowBlur: 10,
                shadowColor: borderColor
            },
            point: {
                radius: 0,
                hitRadius: 10,
                hoverRadius: 4,
                backgroundColor: '#fff',
                borderWidth: 2,
                borderColor: borderColor
            }
        }
    });

    // Create gradient for pH chart
    const phCtx = document.getElementById('phChart').getContext('2d');
    const phGradient = phCtx.createLinearGradient(0, 0, 0, 300);
    phGradient.addColorStop(0, 'rgba(0, 240, 255, 0.4)');
    phGradient.addColorStop(1, 'rgba(0, 240, 255, 0.0)');

    // Create gradient for TDS chart
    const tdsCtx = document.getElementById('tdsChart').getContext('2d');
    const tdsGradient = tdsCtx.createLinearGradient(0, 0, 0, 300);
    tdsGradient.addColorStop(0, 'rgba(0, 255, 102, 0.4)');
    tdsGradient.addColorStop(1, 'rgba(0, 255, 102, 0.0)');

    charts = {
        ph: new Chart(phCtx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    data: [],
                    borderColor: THEME.accent,
                    backgroundColor: phGradient,
                    fill: true
                }]
            },
            options: { ...createChartOptions(THEME.accent), scales: { y: { min: 0, max: 14, ...createChartOptions(THEME.accent).scales.y } } }
        }),
        tds: new Chart(tdsCtx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    data: [],
                    borderColor: THEME.success,
                    backgroundColor: tdsGradient,
                    fill: true
                }]
            },
            options: { ...createChartOptions(THEME.success), scales: { y: { min: 0, max: 500, ...createChartOptions(THEME.success).scales.y } } }
        })
    };
}

// ==================== MQTT LOGIC ====================
function connectToMQTT() {
    client = mqtt.connect(MQTT_BROKER, {
        clientId: 'nexus_dash_' + Math.random().toString(16).substr(2, 8),
        reconnectPeriod: 3000
    });

    client.on("connect", () => {
        // Update Main Connection Status
        DOM.connText.textContent = "SYSTEM ONLINE";
        DOM.connText.className = "text-sm font-bold text-ocean-accent tracking-widest";
        DOM.connDot.className = "status-dot bg-ocean-accent shadow-neon-glow";
        DOM.connContainer.className = "flex items-center gap-3 px-4 py-2 rounded-xl bg-ocean-accent/10 border border-ocean-accent/30";

        // Update MQTT Badge
        updateMiniBadge(DOM.mqttBadge, true, "MQTT LINKED");

        // Set ESP to waiting initially
        updateMiniBadge(DOM.espBadge, false, "AWAITING NODE", THEME.warning);

        client.subscribe(MQTT_TOPIC);
    });

    client.on("offline", () => {
        DOM.connText.textContent = "CONNECTION LOST";
        DOM.connText.className = "text-sm font-bold text-ocean-danger tracking-widest";
        DOM.connDot.className = "status-dot bg-ocean-danger";
        DOM.connContainer.className = "flex items-center gap-3 px-4 py-2 rounded-xl bg-ocean-danger/10 border border-ocean-danger/30";

        updateMiniBadge(DOM.mqttBadge, false, "MQTT OFFLINE", THEME.danger);
        updateMiniBadge(DOM.espBadge, false, "NODE OFFLINE", THEME.danger);
    });

    client.on("message", (topic, message) => {
        try {
            const data = JSON.parse(message.toString());

            // ESP is definitively online when sending data
            updateMiniBadge(DOM.espBadge, true, "NODE ACTIVE", THEME.success);

            messageCount++;
            DOM.dataCount.textContent = `RX: ${messageCount} PKT`;

            const now = new Date();
            DOM.lastUpdate.textContent = now.toLocaleTimeString('en-US', { hour12: false });
            DOM.lastMessage.textContent = `Last sig: ${now.getSeconds()}s ago`;

            processIncomingData(data);

        } catch (e) {
            console.error("Payload parse error:", e);
        }
    });
}

// Helper for the small header badges
function updateMiniBadge(element, isGood, text, colorCode = THEME.success) {
    const dot = element.querySelector('.status-dot');
    const span = element.querySelector('span:last-child');

    span.textContent = text;
    if (isGood) {
        element.style.borderColor = `rgba(${hexToRgb(colorCode)}, 0.3)`;
        element.style.backgroundColor = `rgba(${hexToRgb(colorCode)}, 0.1)`;
        dot.style.backgroundColor = colorCode;
        dot.style.color = colorCode;
        span.style.color = colorCode;
    } else {
        element.style.borderColor = `rgba(${hexToRgb(colorCode)}, 0.3)`;
        element.style.backgroundColor = `rgba(0,0,0,0.4)`;
        dot.style.backgroundColor = colorCode;
        dot.style.color = colorCode;
        span.style.color = `rgba(${hexToRgb(colorCode)}, 0.8)`;
    }
}

// Helper to convert hex to rgb for rgba manipulation
function hexToRgb(hex) {
    const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? `${parseInt(result[1], 16)}, ${parseInt(result[2], 16)}, ${parseInt(result[3], 16)}` : '255,255,255';
}

// ==================== DATA PROCESSING & UI UPDATE ====================

function styleValueBadge(element, condition, textGood, textBad) {
    if (condition) {
        element.textContent = textGood;
        element.className = "inline-flex px-2 py-0.5 rounded text-[10px] font-bold bg-[#00FF66]/10 text-[#00FF66] border border-[#00FF66]/30 uppercase tracking-widest";
    } else {
        element.textContent = textBad;
        element.className = "inline-flex px-2 py-0.5 rounded text-[10px] font-bold bg-[#FF2A54]/10 text-[#FF2A54] border border-[#FF2A54]/30 uppercase tracking-widest animate-pulse";
    }
}

function processIncomingData(data) {
    // 1. Update Grid Values
    if (data.ph !== undefined) {
        DOM.phValue.textContent = data.ph.toFixed(2);
        styleValueBadge(DOM.phBadge, data.ph >= 6.5 && data.ph <= 8.5, "OPTIMAL", "WARNING");
    }
    if (data.tds !== undefined) {
        DOM.tdsValue.textContent = data.tds.toFixed(0);
        styleValueBadge(DOM.tdsBadge, data.tds < 50, "PURE", "HIGH TDS");
    }
    if (data.turbidity !== undefined) {
        DOM.turbidityValue.textContent = data.turbidity.toFixed(1);
        styleValueBadge(DOM.turbBadge, data.turbidity < 5, "CLEAR", "CLOUDY");
    }
    if (data.temperature !== undefined) {
        DOM.tempValue.textContent = data.temperature.toFixed(1);
        styleValueBadge(DOM.tempBadge, data.temperature > 15 && data.temperature < 35, "NOMINAL", "ALERT");
    }

    // 2. Main Water Status Hero Card
    if (data.status) {
        if (data.status.toUpperCase() === 'LAYAK' || data.status.toUpperCase() === 'SAFE') {
            DOM.waterStatusText.textContent = "SAFE TO DRINK";
            DOM.waterStatusText.className = "text-3xl font-extrabold tracking-tight z-10 text-ocean-success mb-1 text-shadow-sm";
            DOM.statusIconWrapper.innerHTML = `<svg class="w-16 h-16 text-ocean-success" fill="none" stroke="currentColor"
