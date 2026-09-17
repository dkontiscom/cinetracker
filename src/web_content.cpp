#include "web_content.h"

const char *web_content_index_html() {
  return R"CINETRACKER_HTML(<!DOCTYPE html>
<html lang='en'>
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no'>
    <meta name='apple-mobile-web-app-capable' content='yes'>
    <meta name='apple-mobile-web-app-status-bar-style' content='black-translucent'>
    <title>CineTracker</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            -webkit-tap-highlight-color: transparent;
        }

        :root {
            --primary: #00ff88;
            --danger: #ff4444;
            --warning: #ffaa00;
            --bg-dark: #0a0a0a;
            --bg-card: #1a1a1a;
            --bg-input: #2a2a2a;
            --text: #ffffff;
            --text-dim: #888;
            --border: #333;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: var(--bg-dark);
            color: var(--text);
            overflow-x: hidden;
            -webkit-font-smoothing: antialiased;
            touch-action: pan-y;
        }

        /* Header */
        .header {
            background: linear-gradient(135deg, #1a1a1a 0%, #2a2a2a 100%);
            padding: 1rem;
            box-shadow: 0 2px 10px rgba(0,0,0,0.5);
            position: sticky;
            top: 0;
            z-index: 100;
        }

        .header h1 {
            font-size: 1.2rem;
            font-weight: 600;
            display: flex;
            align-items: center;
            justify-content: space-between;
        }

        .status-indicator {
            width: 12px;
            height: 12px;
            border-radius: 50%;
            background: var(--danger);
            animation: pulse 2s infinite;
        }

        .status-indicator.connected {
            background: var(--primary);
        }

        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }

        /* Tab Navigation */
        .tab-nav {
            display: flex;
            background: var(--bg-card);
            border-bottom: 1px solid var(--border);
            position: sticky;
            top: 60px;
            z-index: 90;
        }

        .tab-btn {
            flex: 1;
            padding: 1rem;
            background: none;
            border: none;
            color: var(--text-dim);
            font-size: 0.9rem;
            font-weight: 500;
            position: relative;
            transition: color 0.3s;
        }

        .tab-btn.active {
            color: var(--primary);
        }

        .tab-btn.active::after {
            content: '';
            position: absolute;
            bottom: 0;
            left: 0;
            right: 0;
            height: 3px;
            background: var(--primary);
        }

        /* Tab Content */
        .tab-content {
            display: none;
            padding: 1rem;
            animation: fadeIn 0.3s;
        }

        .tab-content.active {
            display: block;
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(10px); }
            to { opacity: 1; transform: translateY(0); }
        }

        /* Cards */
        .card {
            background: var(--bg-card);
            border-radius: 12px;
            padding: 1.25rem;
            margin-bottom: 1rem;
            box-shadow: 0 2px 8px rgba(0,0,0,0.3);
        }

        .card h2 {
            font-size: 0.875rem;
            text-transform: uppercase;
            letter-spacing: 0.05em;
            color: var(--text-dim);
            margin-bottom: 1rem;
        }

        /* Encoder Display */
        .encoder-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 0.75rem;
            margin: 1.5rem 0;
        }

        .encoder-item {
            background: var(--bg-input);
            border-radius: 8px;
            padding: 1rem;
            text-align: center;
            border: 1px solid var(--border);
        }

        .encoder-label {
            font-size: 0.75rem;
            color: var(--text-dim);
            margin-bottom: 0.5rem;
            text-transform: uppercase;
        }

        .encoder-value {
            font-size: 1.75rem;
            font-weight: 700;
            color: var(--primary);
            font-variant-numeric: tabular-nums;
        }

        .encoder-unit {
            font-size: 0.75rem;
            color: var(--text-dim);
            margin-top: 0.25rem;
        }

        /* Buttons */
        .btn {
            display: block;
            width: 100%;
            padding: 1rem;
            border: none;
            border-radius: 8px;
            font-size: 1rem;
            font-weight: 600;
            text-align: center;
            cursor: pointer;
            transition: all 0.2s;
            user-select: none;
            -webkit-user-select: none;
        }

        .btn:active {
            transform: scale(0.98);
        }

        .btn-primary {
            background: var(--primary);
            color: var(--bg-dark);
        }

        .btn-primary:active {
            background: #00dd77;
        }

        .btn-secondary {
            background: var(--bg-input);
            color: var(--text);
            border: 1px solid var(--border);
        }

        .btn-danger {
            background: var(--danger);
            color: white;
        }

        .btn-group {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 0.75rem;
            margin: 1rem 0;
        }

        /* Calibration Steps */
        .calibration-step {
            background: var(--bg-input);
            border-radius: 8px;
            padding: 1rem;
            margin: 1rem 0;
            border-left: 4px solid var(--primary);
        }

        .calibration-step.inactive {
            opacity: 0.5;
            border-left-color: var(--border);
        }

        .step-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 0.5rem;
        }

        .step-title {
            font-weight: 600;
        }

        .step-status {
            font-size: 0.75rem;
            color: var(--primary);
        }

        /* Forms */
        .form-group {
            margin-bottom: 1.25rem;
        }

        label {
            display: block;
            font-size: 0.875rem;
            color: var(--text-dim);
            margin-bottom: 0.5rem;
            text-transform: uppercase;
            letter-spacing: 0.025em;
        }

        input[type='text'],
        input[type='number'],
        select {
            width: 100%;
            padding: 0.875rem;
            background: var(--bg-input);
            border: 1px solid var(--border);
            border-radius: 8px;
            color: var(--text);
            font-size: 1rem;
            -webkit-appearance: none;
        }

        input:focus,
        select:focus {
            outline: none;
            border-color: var(--primary);
            box-shadow: 0 0 0 3px rgba(0, 255, 136, 0.1);
        }

        /* Toggle Switch */
        .toggle-group {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 0.75rem 0;
        }

        .toggle {
            position: relative;
            width: 52px;
            height: 28px;
        }

        .toggle input {
            opacity: 0;
            width: 0;
            height: 0;
        }

        .toggle-slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: var(--bg-input);
            border: 1px solid var(--border);
            transition: 0.3s;
            border-radius: 28px;
        }

        .toggle-slider:before {
            position: absolute;
            content: '';
            height: 20px;
            width: 20px;
            left: 4px;
            bottom: 3px;
            background: white;
            transition: 0.3s;
            border-radius: 50%;
        }

        input:checked + .toggle-slider {
            background: var(--primary);
            border-color: var(--primary);
        }

        input:checked + .toggle-slider:before {
            transform: translateX(24px);
        }

        /* Status Messages */
        .alert {
            padding: 1rem;
            border-radius: 8px;
            margin: 1rem 0;
            font-size: 0.875rem;
            display: flex;
            align-items: center;
            gap: 0.75rem;
        }

        .alert-success {
            background: rgba(0, 255, 136, 0.15);
            border: 1px solid rgba(0, 255, 136, 0.3);
            color: var(--primary);
        }

        .alert-error {
            background: rgba(255, 68, 68, 0.15);
            border: 1px solid rgba(255, 68, 68, 0.3);
            color: var(--danger);
        }

        .alert-warning {
            background: rgba(255, 170, 0, 0.15);
            border: 1px solid rgba(255, 170, 0, 0.3);
            color: var(--warning);
        }

        /* Loading Spinner */
        .spinner {
            width: 40px;
            height: 40px;
            margin: 2rem auto;
            border: 3px solid var(--border);
            border-top-color: var(--primary);
            border-radius: 50%;
            animation: spin 1s linear infinite;
        }

        @keyframes spin {
            to { transform: rotate(360deg); }
        }

        /* Modal */
        .modal {
            display: none;
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(0,0,0,0.8);
            z-index: 1000;
            padding: 1rem;
            overflow-y: auto;
        }

        .modal.active {
            display: flex;
            align-items: center;
            justify-content: center;
        }

        .modal-content {
            background: var(--bg-card);
            border-radius: 12px;
            width: 100%;
            max-width: 400px;
            padding: 1.5rem;
            animation: slideUp 0.3s;
        }

        @keyframes slideUp {
            from { transform: translateY(50px); opacity: 0; }
            to { transform: translateY(0); opacity: 1; }
        }

        /* Lens Profile List */
        .profile-list {
            max-height: 300px;
            overflow-y: auto;
        }

        .profile-item {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 1rem;
            background: var(--bg-input);
            border-radius: 8px;
            margin-bottom: 0.5rem;
            cursor: pointer;
            transition: all 0.2s;
        }

        .profile-item:active {
            background: var(--border);
        }

        .profile-item.selected {
            border: 2px solid var(--primary);
        }

        /* Responsive adjustments for very small screens */
        @media (max-width: 360px) {
            .encoder-grid {
                grid-template-columns: 1fr;
            }

            .btn-group {
                grid-template-columns: 1fr;
            }
        }

        /* Safe area for iPhone notch */
        @supports (padding-top: env(safe-area-inset-top)) {
            .header {
                padding-top: calc(1rem + env(safe-area-inset-top));
            }
        }

        /* iOS input zoom fix */
        @supports (-webkit-touch-callout: none) {
            input, textarea, select { font-size: 16px !important; }
        }
    </style>
</head>
<body>
    <!-- Header -->
    <div class='header'>
        <h1>
            <span>CineTracker</span>
            <span class='status-indicator' id='status-indicator'></span>
        </h1>
    </div>

    <!-- Tab Navigation -->
    <div class='tab-nav'>
        <button class='tab-btn active' onclick='switchTab("monitor")'>Monitor</button>
        <button class='tab-btn' onclick='switchTab("calibrate")'>Calibrate</button>
        <button class='tab-btn' onclick='switchTab("settings")'>Settings</button>
        <button class='tab-btn' onclick='switchTab("profiles")'>Profiles</button>
    </div>

    <!-- Monitor Tab -->
    <div id='tab-monitor' class='tab-content active'>
        <div class='card'>
            <h2>Live Values</h2>
            <div class='encoder-grid'>
                <div class='encoder-item'>
                    <div class='encoder-label'>Focus</div>
                    <div class='encoder-value' id='focus-value'>--</div>
                    <div class='encoder-unit'>mm</div>
                </div>
                <div class='encoder-item'>
                    <div class='encoder-label'>Iris</div>
                    <div class='encoder-value' id='iris-value'>--</div>
                    <div class='encoder-unit'>T-stop</div>
                </div>
                <div class='encoder-item'>
                    <div class='encoder-label'>Zoom</div>
                    <div class='encoder-value' id='zoom-value'>--</div>
                    <div class='encoder-unit'>mm</div>
                </div>
            </div>
        </div>

        <div class='card'>
            <h2>Stream Control</h2>
            <button id='stream-btn' class='btn btn-primary' onclick='toggleStream()'>Start Streaming</button>
            <div style='margin-top: 1rem; display: flex; justify-content: space-between;'>
                <small style='color: var(--text-dim)'>FreeD Protocol</small>
                <small style='color: var(--text-dim)' id='frame-rate'>0 fps</small>
            </div>
        </div>

        <div class='card'>
            <h2>Quick Actions</h2>
            <div class='btn-group'>
                <button class='btn btn-secondary' onclick='resetOvershoot()'>Reset Overshoot</button>
                <button class='btn btn-secondary' onclick='zeroEncoders()'>Zero All</button>
            </div>
        </div>
    </div>

    <!-- Calibrate Tab -->
    <div id='tab-calibrate' class='tab-content'>
        <div class='card'>
            <h2>Calibration Wizard</h2>
            <div id='calibration-status' class='alert alert-warning'>Ready to calibrate</div>

            <div class='calibration-step' id='step-1'>
                <div class='step-header'>
                    <span class='step-title'>1. Focus Limits</span>
                    <span class='step-status' id='focus-status'>Not Set</span>
                </div>
                <div class='btn-group'>
                    <button class='btn btn-secondary' onclick='setLimit("focus", "min")'>Set Near</button>
                    <button class='btn btn-secondary' onclick='setLimit("focus", "mid")'>Set Mid</button>
                    <button class='btn btn-secondary' onclick='setLimit("focus", "max")'>Set Far</button>
                </div>
            </div>

            <div class='calibration-step' id='step-2'>
                <div class='step-header'>
                    <span class='step-title'>2. Iris Limits</span>
                    <span class='step-status' id='iris-status'>Not Set</span>
                </div>
                <div class='btn-group'>
                    <button class='btn btn-secondary' onclick='setLimit("iris", "min")'>Set Open</button>
                    <button class='btn btn-secondary' onclick='setLimit("iris", "mid")'>Set Mid</button>
                    <button class='btn btn-secondary' onclick='setLimit("iris", "max")'>Set Closed</button>
                </div>
            </div>

            <div class='calibration-step' id='step-3'>
                <div class='step-header'>
                    <span class='step-title'>3. Zoom Limits</span>
                    <span class='step-status' id='zoom-status'>Not Set</span>
                </div>
                <div class='btn-group'>
                    <button class='btn btn-secondary' onclick='setLimit("zoom", "min")'>Set Wide</button>
                    <button class='btn btn-secondary' onclick='setLimit("zoom", "mid")'>Set Mid</button>
                    <button class='btn btn-secondary' onclick='setLimit("zoom", "max")'>Set Tele</button>
                </div>
            </div>

            <button class='btn btn-primary' onclick='saveCalibration()' style='margin-top: 1.5rem;'>Save Calibration</button>
        </div>
    </div>

    <!-- Settings Tab -->
    <div id='tab-settings' class='tab-content'>
        <div class='card'>
            <h2>Network Settings</h2>
            <div class='form-group'>
                <label>IP Address</label>
                <input type='text' id='ip-address' value='192.168.1.100' inputmode='decimal'>
            </div>
            <div class='form-group'>
                <label>Destination IP</label>
                <input type='text' id='multicast-ip' value='236.10.10.10' inputmode='decimal'>
            </div>
            <div class='form-group'>
                <label>Port</label>
                <input type='number' id='port' value='40000' inputmode='numeric'>
            </div>
            <button class='btn btn-primary' onclick='saveNetworkSettings()'>Save Network Settings</button>
        </div>

        <div class='card'>
            <h2>FreeD Settings</h2>
            <div class='form-group'>
                <label>Camera ID</label>
                <input type='number' id='camera-id' value='1' min='1' max='255' inputmode='numeric'>
            </div>
            <div class='form-group'>
                <label>Frame Rate</label>
                <select id='frame-rate-select'>
                    <option value='24'>24 fps</option>
                    <option value='25'>25 fps</option>
                    <option value='30'>30 fps</option>
                    <option value='50'>50 fps</option>
                    <option value='60'>60 fps</option>
                </select>
            </div>
            <button class='btn btn-primary' onclick='saveFreeDSettings()'>Save FreeD Settings</button>
        </div>

        <div class='card'>
            <h2>System</h2>
            <div class='toggle-group'>
                <span>Filter Enabled</span>
                <label class='toggle'>
                    <input type='checkbox' id='filter-enabled' checked>
                    <span class='toggle-slider'></span>
                </label>
            </div>
            <div class='toggle-group'>
                <span>LED Indicator</span>
                <label class='toggle'>
                    <input type='checkbox' id='led-enabled' checked>
                    <span class='toggle-slider'></span>
                </label>
            </div>
            <button class='btn btn-danger' onclick='confirmReboot()' style='margin-top: 1rem;'>Reboot System</button>
        </div>
    </div>

    <!-- Profiles Tab -->
    <div id='tab-profiles' class='tab-content'>
        <div class='card'>
            <h2>Lens Profiles</h2>
            <div class='profile-list' id='profile-list'>
                <!-- Profiles loaded dynamically -->
            </div>
            <div class='btn-group' style='margin-top: 1rem;'>
                <button class='btn btn-primary' onclick='loadSelectedProfile()'>Load Profile</button>
                <button class='btn btn-secondary' onclick='showNewProfileDialog()'>New Profile</button>
            </div>
        </div>
    </div>

    <!-- Modal for dialogs -->
    <div class='modal' id='modal'>
        <div class='modal-content' id='modal-content'>
            <!-- Dynamic content -->
        </div>
    </div>

    <script>
        console.log('CineTracker v1.0');

        // Global state
        let currentTab = 'monitor';
        let isStreaming = false;
        let selectedProfile = null;
        let calibrationData = {};

        // Tab switching
        function switchTab(tab) {
            document.querySelectorAll('.tab-btn').forEach(btn => {
                btn.classList.remove('active');
            });
            document.querySelectorAll('.tab-content').forEach(content => {
                content.classList.remove('active');
            });

            event.target.classList.add('active');
            document.getElementById('tab-' + tab).classList.add('active');
            currentTab = tab;

            // Load tab-specific data
            if (tab === 'profiles') {
                loadProfiles();
            }
        }

        // Real-time updates
        function updateStatus() {
            if (currentTab !== 'monitor') return;

            fetch('/api/status')
                .then(r => r.json())
                .then(data => {
                    // Update connection indicator
                    document.getElementById('status-indicator').className =
                        'status-indicator ' + (data.connected ? 'connected' : '');

                    // Update encoder values
                    document.getElementById('focus-value').textContent =
                        data.focus_mm ? data.focus_mm.toFixed(0) : '--';
                    document.getElementById('iris-value').textContent =
                        data.iris_fstop ? 'T' + data.iris_fstop.toFixed(1) : '--';
                    document.getElementById('zoom-value').textContent =
                        data.zoom_mm ? data.zoom_mm.toFixed(0) : '--';

                    // Update frame rate
                    document.getElementById('frame-rate').textContent =
                        data.fps ? data.fps + ' fps' : '0 fps';

                    // Update streaming button
                    isStreaming = data.streaming;
                    const btn = document.getElementById('stream-btn');
                    btn.textContent = isStreaming ? 'Stop Streaming' : 'Start Streaming';
                    btn.className = isStreaming ? 'btn btn-danger' : 'btn btn-primary';
                })
                .catch(err => {
                    document.getElementById('status-indicator').className = 'status-indicator';
                });
        }

        // Stream control
        function toggleStream() {
            const endpoint = isStreaming ? '/api/stream/stop' : '/api/stream/start';
            fetch(endpoint, { method: 'POST' })
                .then(r => r.json())
                .then(data => {
                    showToast(data.message, data.success ? 'success' : 'error');
                    updateStatus();
                });
        }

        // Calibration functions
        function setLimit(encoder, limit) {
            fetch('/api/calibration/set', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ encoder, limit })
            })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    calibrationData[encoder + '_' + limit] = data.value;
                    document.getElementById(encoder + '-status').textContent = 'Set';
                    document.getElementById(encoder + '-status').style.color = 'var(--primary)';
                    showToast(`${encoder} ${limit} set to ${data.value}`, 'success');
                } else {
                    showToast(data.message, 'error');
                }
            });
        }

        function saveCalibration() {
            fetch('/api/calibration/save', { method: 'POST' })
                .then(r => r.json())
                .then(data => {
                    showToast(data.message, data.success ? 'success' : 'error');
                    if (data.success) {
                        // Reset calibration UI
                        document.querySelectorAll('.step-status').forEach(el => {
                            el.textContent = 'Saved';
                        });
                    }
                });
        }

        // Network settings
        function saveNetworkSettings() {
            const settings = {
                ip: document.getElementById('ip-address').value,
                multicast: document.getElementById('multicast-ip').value,
                port: parseInt(document.getElementById('port').value)
            };

            fetch('/api/network', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(settings)
            })
            .then(r => r.json())
            .then(data => {
                showToast(data.message, data.success ? 'success' : 'error');
            });
        }

        // FreeD settings
        function saveFreeDSettings() {
            const settings = {
                camera_id: parseInt(document.getElementById('camera-id').value),
                frame_rate: parseInt(document.getElementById('frame-rate-select').value)
            };

            fetch('/api/freed', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(settings)
            })
            .then(r => r.json())
            .then(data => {
                showToast(data.message, data.success ? 'success' : 'error');
            });
        }

        // Profiles
        function loadProfiles() {
            fetch('/api/profiles')
                .then(r => r.json())
                .then(data => {
                    const list = document.getElementById('profile-list');
                    list.innerHTML = '';

                    if (data.profiles && data.profiles.length > 0) {
                        data.profiles.forEach((profile, i) => {
                            const item = document.createElement('div');
                            item.className = 'profile-item';
                            item.innerHTML = `
                                <div>
                                    <div style='font-weight: 600'>${profile.name}</div>
                                    <div style='font-size: 0.75rem; color: var(--text-dim)'>
                                        ${profile.focus_min}-${profile.focus_max}mm
                                    </div>
                                </div>
                                <button class='btn btn-secondary' style='width: auto; padding: 0.5rem 1rem'
                                        onclick='deleteProfile(${profile.id})'>Delete</button>
                            `;
                            item.onclick = () => selectProfile(profile.id);
                            list.appendChild(item);
                        });
                    } else {
                        list.innerHTML = '<div style=\"text-align: center; color: var(--text-dim); padding: 2rem;\">No profiles saved</div>';
                    }
                });
        }

        function selectProfile(id) {
            selectedProfile = id;
            document.querySelectorAll('.profile-item').forEach(item => {
                item.classList.remove('selected');
            });
            event.currentTarget.classList.add('selected');
        }

        function loadSelectedProfile() {
            if (!selectedProfile) {
                showToast('Please select a profile first', 'warning');
                return;
            }

            fetch('/api/profiles/load', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ id: selectedProfile })
            })
            .then(r => r.json())
            .then(data => {
                showToast(data.message, data.success ? 'success' : 'error');
            });
        }

        // Quick actions
        function resetOvershoot() {
            fetch('/api/encoders/reset', { method: 'POST' })
                .then(r => r.json())
                .then(data => {
                    showToast('Overshoot reset', 'success');
                });
        }

        function zeroEncoders() {
            fetch('/api/encoders/zero', { method: 'POST' })
                .then(r => r.json())
                .then(data => {
                    showToast('Encoders zeroed', 'success');
                });
        }

        // System functions
        function confirmReboot() {
            if (confirm('Are you sure you want to reboot the system?')) {
                fetch('/api/system/reboot', { method: 'POST' })
                    .then(() => {
                        showToast('System rebooting...', 'warning');
                        setTimeout(() => {
                            location.reload();
                        }, 5000);
                    });
            }
        }

        // Toast notifications
        function showToast(message, type = 'info') {
            const toast = document.createElement('div');
            toast.className = 'alert alert-' + type;
            toast.textContent = message;
            toast.style.position = 'fixed';
            toast.style.bottom = '20px';
            toast.style.left = '20px';
            toast.style.right = '20px';
            toast.style.zIndex = '999';
            document.body.appendChild(toast);

            setTimeout(() => {
                toast.style.opacity = '0';
                setTimeout(() => toast.remove(), 300);
            }, 3000);
        }

        // Modal functions
        function showModal(content) {
            document.getElementById('modal-content').innerHTML = content;
            document.getElementById('modal').classList.add('active');
        }

        function closeModal() {
            document.getElementById('modal').classList.remove('active');
        }

        // Initialize
        document.addEventListener('DOMContentLoaded', () => {
            // Load initial settings
            fetch('/api/config')
                .then(r => r.json())
                .then(config => {
                    document.getElementById('ip-address').value = config.ip || '192.168.1.100';
                    document.getElementById('multicast-ip').value = config.multicast || '236.10.10.10';
                    document.getElementById('port').value = config.port || 40000;
                    document.getElementById('camera-id').value = config.camera_id || 1;
                    document.getElementById('frame-rate-select').value = config.frame_rate || 24;
                    document.getElementById('filter-enabled').checked = config.filter_enabled !== false;
                    document.getElementById('led-enabled').checked = config.led_enabled !== false;
                });

            // Start status updates
            setInterval(updateStatus, 100);
            updateStatus();

            // Handle modal close on background click
            document.getElementById('modal').onclick = function(e) {
                if (e.target === this) closeModal();
            };
        });

        // Prevent pull-to-refresh on mobile
        document.addEventListener('touchmove', function(e) {
            if (e.touches.length > 1) e.preventDefault();
        }, { passive: false });
    </script>
</body>
</html>)CINETRACKER_HTML";
}
