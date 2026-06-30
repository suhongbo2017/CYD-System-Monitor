const maxDataPoints = 30
const cpuData = Array(maxDataPoints).fill(0)
const memoryData = Array(maxDataPoints).fill(0)
const labels = Array(maxDataPoints).fill('')

// --- Charts created lazily once Chart.js loads ---
let cpuChart = null
let memoryChart = null
let tempGauge = null
let gaugesReady = false
let chartsReady = false
let pendingData = null
let isEditing = false

function initCharts() {
  if (chartsReady || typeof Chart === 'undefined') return
  chartsReady = true
  const primaryColor = getComputedStyle(document.documentElement).getPropertyValue('--primary-color')
  const textColor = getComputedStyle(document.documentElement).getPropertyValue('--text-color')
  const borderColor = getComputedStyle(document.documentElement).getPropertyValue('--border-color')

  try {
    cpuChart = new Chart(document.getElementById('cpuChart').getContext('2d'), {
      type: 'line',
      data: { labels, datasets: [{ label: 'CPU Usage %', data: cpuData, borderColor: primaryColor, tension: 0.4, fill: false }] },
      options: {
        responsive: true, maintainAspectRatio: false,
        scales: { y: { beginAtZero: true, max: 100, ticks: { color: textColor }, grid: { color: borderColor } }, x: { display: false } },
        plugins: { legend: { display: false }, tooltip: { enabled: false } }
      }
    })
  } catch(e) { console.warn('CPU chart init failed:', e) }

  try {
    memoryChart = new Chart(document.getElementById('memoryChart').getContext('2d'), {
      type: 'line',
      data: { labels, datasets: [{ label: 'Memory %', data: memoryData, borderColor: '#10b981', tension: 0.4, fill: false }] },
      options: {
        responsive: true, maintainAspectRatio: false,
        scales: { y: { beginAtZero: true, max: 100, ticks: { color: textColor }, grid: { color: borderColor } }, x: { display: false } },
        plugins: { legend: { display: false }, tooltip: { enabled: false } }
      }
    })
  } catch(e) { console.warn('Memory chart init failed:', e) }
}

function initGauge() {
  if (gaugesReady || typeof Gauge === 'undefined') return
  gaugesReady = true
  try {
    const target = document.getElementById('tempGauge')
    if (!target) return
    const ctx = target.getContext('2d')
    tempGauge = new Gauge(ctx).setOptions({
      angle: 0.15, lineWidth: 0.44, radiusScale: 1,
      pointer: { length: 0.6, strokeWidth: 0.035, color: '#000000' },
      limitMax: false, limitMin: false,
      colorStart: '#6FADCF', colorStop: '#8FC0DA',
      strokeColor: '#E0E0E0', generateGradient: true,
      highDpiSupport: true,
      staticLabels: { font: '10px sans-serif', labels: [0, 25, 50, 75, 100], color: '#000000', fractionDigits: 0 },
      staticZones: [
        { strokeStyle: '#F03E3E', min: 80, max: 100 },
        { strokeStyle: '#FFDD00', min: 60, max: 80 },
        { strokeStyle: '#30B32D', min: 0, max: 60 }
      ]
    })
    tempGauge.maxValue = 100
    tempGauge.setMinValue(0)
    tempGauge.animationSpeed = 32
    tempGauge.set(0)
  } catch(e) { console.warn('Gauge init failed:', e) }
}

function setupCharts() {
  initCharts()
  initGauge()
}

// Call setup as soon as possible, retry if CDN scripts haven't loaded yet
setupCharts()
if (!chartsReady || !gaugesReady) {
  // Retry after CDN scripts finish loading
  document.addEventListener('DOMContentLoaded', () => setTimeout(setupCharts, 100))
  window.addEventListener('load', () => setTimeout(setupCharts, 50))
}

function updateSystemInfo(data) {
  const fields = [
    'chipModel','chipRevision','sdkVersion','cpuFreqMHz','cycleCount','efuseMac',
    'temperature','hallSensor','uptime','lastResetReason',
    'totalHeap','freeHeap','minFreeHeap','maxAllocHeap','heapFragmentation',
    'psramSize','freePsram','minFreePsram','maxAllocPsram',
    'flashChipSize','flashChipSpeed','flashChipMode','sketchSize','sketchMD5','freeSketchSpace',
    'wifiSSID','wifiBSSID','isHidden','autoReconnect','hostname','wifiMode',
    'ipAddress','macAddress','dnsIP','gatewayIP','subnetMask'
  ]
  for (const key of fields) {
    const el = document.getElementById(key)
    if (el && data[key] !== undefined) {
      el.textContent = (typeof data[key] === 'string' || typeof data[key] === 'number') ? data[key] : JSON.stringify(data[key])
    }
  }
}

function updateVisualizations(data) {
  // CPU chart
  if (cpuChart && data.cpuUsage !== undefined) {
    cpuData.push(data.cpuUsage)
    cpuData.shift()
    cpuChart.data.datasets[0].data = cpuData
    cpuChart.update('none')
  }

  // Memory chart (approximate from system data: 100 - freeHeap/totalHeap*100)
  if (memoryChart && data.totalHeap && data.freeHeap !== undefined) {
    const memPct = Math.round((1 - data.freeHeap / data.totalHeap) * 100)
    memoryData.push(memPct)
    memoryData.shift()
    memoryChart.data.datasets[0].data = memoryData
    memoryChart.update('none')
  }

  // Temperature gauge (approximate from internal temp)
  if (tempGauge && data.temperature !== undefined) {
    const tempVal = parseFloat(data.temperature)
    if (!isNaN(tempVal)) {
      tempGauge.set(Math.min(tempVal, 100))
      document.getElementById('tempValue').textContent = tempVal.toFixed(1) + '°C'
    }
  }

  // WiFi signal bars
  if (data.wifiStrength !== undefined) {
    const rssi = Math.abs(data.wifiStrength)
    const bars = document.querySelectorAll('.signal-bar')
    let level = 0
    if (rssi < 50) level = 4; else if (rssi < 60) level = 3; else if (rssi < 70) level = 2; else level = 1
    bars.forEach((bar, i) => {
      bar.classList.remove('active', 'excellent', 'good', 'fair', 'poor')
      if (i < level) {
        bar.classList.add('active')
        if (level >= 4) bar.classList.add('excellent')
        else if (level === 3) bar.classList.add('good')
        else if (level === 2) bar.classList.add('fair')
        else bar.classList.add('poor')
      }
    })
    document.getElementById('signalValue').textContent = data.wifiStrength + ' dBm'
  }
}

function updateColorPickers() {
  const colorMap = { bgColor: 'bgColor', textColor: 'textColor', cpuColor: 'cpuColor', ramColor: 'ramColor', borderColor: 'borderColor', cardBgColor: 'cardBgColor' }
  for (const [id, key] of Object.entries(colorMap)) {
    const el = document.getElementById(id)
    if (el && el.dataset.lastValue !== document.documentElement.style.getPropertyValue('--' + id.replace(/([A-Z])/g,'-$1').toLowerCase())) {
      // Will be updated via API response
    }
  }
}

function updateServerDisplay() {
  const host = document.getElementById('glancesHost').value
  const port = document.getElementById('glancesPort').value
  const displayEl = document.getElementById('serverDisplay')
  if (host && port) { displayEl.textContent = host + ':' + port }
  else { displayEl.textContent = 'Not configured' }
}

function toggleServerEdit() {
  isEditing = !isEditing
  document.querySelector('.server-edit').classList.toggle('active')
  document.querySelector('.server-display').style.display = isEditing ? 'none' : ''
}

// --- Main data polling loop ---
// Runs every 3 seconds. Updates page content as data arrives.
// No overlay loader — page renders immediately with "---" placeholders.
setInterval(() => {
  fetch('/settings')
    .then(r => r.json())
    .then(data => {
      pendingData = data
      updateSystemInfo(data)
      updateVisualizations(data)
      updateColorPickers()

      if (!isEditing) {
        document.getElementById('glancesHost').value = data.glances_host || ''
        document.getElementById('glancesPort').value = data.glances_port || ''
        updateServerDisplay()
      }

      // Update theme/dark mode
      const darkMode = document.getElementById('darkMode')
      if (data.darkMode !== undefined) {
        darkMode.checked = data.darkMode
        document.getElementById('themeState').textContent = data.darkMode ? 'DARK' : 'LIGHT'
        document.documentElement.setAttribute('data-theme', data.darkMode ? 'dark' : '')
      }
      const colorKeys = { bgColor: 'bg_color', textColor: 'text_color', cpuColor: 'cpu_color', ramColor: 'ram_color', borderColor: 'border_color', cardBgColor: 'card_bg_color' }
      for (const [jsKey, cssVar] of Object.entries(colorKeys)) {
        if (data[jsKey]) {
          const el = document.getElementById(jsKey)
          if (el) { el.value = data[jsKey]; el.dataset.lastValue = data[jsKey] }
        }
      }

      // Retry chart init if not yet ready (CDN may have arrived)
      setupCharts()
    })
    .catch(err => {
      console.warn('ESP32 data fetch failed, retrying...', err)
    })
}, 3000)

// --- Action functions ---
function updateTheme() {
  const dark = document.getElementById('darkMode').checked
  fetch('/settings', {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ darkMode: dark })
  }).catch(e => console.error(e))
}

function updateThemeColor(key, value) {
  fetch('/settings', {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ [key]: parseInt(value.replace('#',''), 16) })
  }).catch(e => console.error(e))
}

function resetTheme() {
  fetch('/resetTheme', { method: 'POST' })
    .then(() => { window.location.reload() })
    .catch(e => console.error(e))
}

function restartESP() {
  fetch('/restart', { method: 'POST' }).then(() => {
    setTimeout(() => { window.location.reload() }, 5000)
  }).catch(e => console.error(e))
}

function saveGlancesSettings() {
  const host = document.getElementById('glancesHost').value
  const port = parseInt(document.getElementById('glancesPort').value)
  if (!host || !port) { alert('Please enter both host and port'); return }
  fetch('/settings', {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ glances_host: host, glances_port: port })
  })
    .then(r => r.json())
    .then(data => {
      if (data.status === 'success') {
        updateServerDisplay(); toggleServerEdit()
      }
    })
    .catch(e => { console.error(e); alert('Failed to update') })
}

let displayOn = true
function toggleDisplay() {
  const screenToggle = document.getElementById('screenToggle')
  displayOn = !screenToggle.checked
  document.getElementById('displayState').textContent = displayOn ? 'ON' : 'OFF'
  fetch('/displaySleep', {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ sleep: !displayOn })
  }).catch(e => console.error(e))
}