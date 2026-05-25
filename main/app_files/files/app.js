(() => {
    'use strict';

    const STATUS_API      = '/app/api/status';
    const CALIBRATE_API   = '/app/api/calibrate';
    const VENUS_CONFIG_API = '/app/api/venus/config';
    const REFRESH_MS      = 5_000;

    let refreshTimer = null;

    // ── Tab switching ─────────────────────────────────────────────────────────

    const tabs   = document.querySelectorAll('.app-tab');
    const panels = document.querySelectorAll('.app-tab-panel');

    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            tabs.forEach(t => t.classList.remove('active'));
            panels.forEach(p => p.classList.add('hidden'));
            tab.classList.add('active');
            document.getElementById('tab-' + tab.dataset.tab).classList.remove('hidden');
            if (tab.dataset.tab === 'venus') loadVenusConfig();
        });
    });

    // ── Status polling ────────────────────────────────────────────────────────

    async function fetchStatus() {
        clearTimeout(refreshTimer);
        try {
            const res = await fetch(STATUS_API);
            if (!res.ok) throw new Error(`HTTP ${res.status}`);
            const data = await res.json();
            renderDashboard(data);
            renderCalibrate(data);
            renderSystem(data);
            const ts = 'Updated ' + new Date().toLocaleTimeString();
            setStatusLine('dash-status', ts);
            setStatusLine('sys-status',  ts);
        } catch (e) {
            setStatusLine('dash-status', 'Could not reach device');
            setStatusLine('sys-status',  'Could not reach device');
        } finally {
            refreshTimer = setTimeout(fetchStatus, REFRESH_MS);
        }
    }

    // ── Dashboard rendering ───────────────────────────────────────────────────

    function renderDashboard(d) {
        const w = d.water   || {};
        const b = d.battery || {};

        const pct = typeof w.pct === 'number' ? w.pct : null;
        setText('water-pct',    pct !== null ? pct.toFixed(0) : '—');
        setText('water-litres', typeof w.litres === 'number' ? w.litres.toFixed(0) + ' L' : '— L');
        document.getElementById('water-gauge').style.width =
            pct !== null ? Math.max(0, Math.min(100, pct)) + '%' : '0%';

        setText('bat-soc',     fmt(b.soc,     1, ' %'));
        setText('bat-voltage', fmt(b.voltage,  1, ' V'));
        setText('bat-current', fmt(b.current,  1, ' A'));
        setText('bat-solar',   fmt(b.solarW,   0, ' W'));
        setText('bat-load',    fmt(b.loadW,    0, ' W'));
    }

    // ── Calibrate rendering ───────────────────────────────────────────────────

    function renderCalibrate(d) {
        const w = d.water || {};
        setText('cal-raw-volts', typeof w.rawVolts  === 'number' ? w.rawVolts.toFixed(3)  : '—');
        setText('cal-v-empty',   typeof w.calVEmpty  === 'number' ? w.calVEmpty.toFixed(3) : '—');
        setText('cal-v-full',    typeof w.calVFull   === 'number' ? w.calVFull.toFixed(3)  : '—');

        // Pre-fill capacity input only when it has no user-entered value
        const capInput = document.getElementById('cal-capacity-input');
        if (!capInput.value && typeof w.tankLitres === 'number' && w.tankLitres > 0) {
            capInput.value = w.tankLitres;
        }
    }

    // ── System rendering ──────────────────────────────────────────────────────

    function renderSystem(d) {
        const s = d.system || {};
        setText('sys-uptime',   typeof s.uptimeS === 'number' ? formatUptime(s.uptimeS) : '—');
        setText('sys-firmware', s.firmwareVersion || '—');
    }

    function formatUptime(s) {
        const d = Math.floor(s / 86400);
        const h = Math.floor((s % 86400) / 3600);
        const m = Math.floor((s % 3600) / 60);
        if (d > 0) return `${d}d ${h}h ${m}m`;
        if (h > 0) return `${h}h ${m}m`;
        return `${m}m`;
    }

    // ── Calibrate actions ─────────────────────────────────────────────────────

    async function postCalibrate(body) {
        setStatusLine('cal-status', 'Saving…');
        setCalButtonsDisabled(true);
        try {
            const res = await fetch(CALIBRATE_API, {
                method:  'POST',
                headers: { 'Content-Type': 'application/json' },
                body:    JSON.stringify(body),
            });
            const data = await res.json();
            if (!res.ok || data.error) {
                setStatusLine('cal-status', 'Error: ' + (data.error || res.status));
            } else {
                setStatusLine('cal-status', 'Saved ✓');
                // Refresh immediately so cal points update
                clearTimeout(refreshTimer);
                fetchStatus();
            }
        } catch (e) {
            setStatusLine('cal-status', 'Could not reach device');
        } finally {
            setCalButtonsDisabled(false);
        }
    }

    function setCalButtonsDisabled(disabled) {
        document.getElementById('btn-mark-empty').disabled = disabled;
        document.getElementById('btn-mark-full').disabled  = disabled;
    }

    document.getElementById('venus-save-btn').addEventListener('click', saveVenusConfig);

    // ── Venus OS config load / save ───────────────────────────────────────────

    async function loadVenusConfig() {
        setStatusLine('venus-status', '');
        try {
            const res  = await fetch(VENUS_CONFIG_API);
            if (!res.ok) throw new Error(`HTTP ${res.status}`);
            const data = await res.json();
            document.getElementById('venus-broker-ip').value = data.broker_ip || 'venus.local';
            document.getElementById('venus-portal-id').value = data.portal_id || '';
        } catch (e) {
            setStatusLine('venus-status', 'Could not load config');
        }
    }

    async function saveVenusConfig() {
        const brokerIp = document.getElementById('venus-broker-ip').value.trim();
        const portalId = document.getElementById('venus-portal-id').value.trim();

        if (!brokerIp) {
            setStatusLine('venus-status', 'Broker IP / hostname is required');
            return;
        }

        const btn = document.getElementById('venus-save-btn');
        btn.disabled = true;
        setStatusLine('venus-status', 'Saving…');
        try {
            const res  = await fetch(VENUS_CONFIG_API, {
                method:  'POST',
                headers: { 'Content-Type': 'application/json' },
                body:    JSON.stringify({ broker_ip: brokerIp, portal_id: portalId }),
            });
            const data = await res.json();
            if (!res.ok || data.error) {
                setStatusLine('venus-status', 'Error: ' + (data.error || res.status));
            } else {
                setStatusLine('venus-status', 'Saved ✓');
            }
        } catch (e) {
            setStatusLine('venus-status', 'Could not reach device');
        } finally {
            btn.disabled = false;
        }
    }

    // ── Settings dropdown ─────────────────────────────────────────────────────

    const navBtn      = document.getElementById('nav-settings-btn');
    const navDropdown = document.getElementById('nav-settings-dropdown');
    if (navBtn && navDropdown) {
        navBtn.addEventListener('click', e => {
            e.stopPropagation();
            const open = !navDropdown.classList.contains('hidden');
            navDropdown.classList.toggle('hidden', open);
            navBtn.setAttribute('aria-expanded', String(!open));
        });
        document.addEventListener('click', () => {
            navDropdown.classList.add('hidden');
            navBtn.setAttribute('aria-expanded', 'false');
        });
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    function setText(id, text) {
        const el = document.getElementById(id);
        if (el) el.textContent = text;
    }

    function setStatusLine(id, text) {
        const el = document.getElementById(id);
        if (el) el.textContent = text;
    }

    /** Format a numeric value with fixed decimals and a unit suffix, or '—'. */
    function fmt(val, decimals, unit) {
        return typeof val === 'number' ? val.toFixed(decimals) + unit : '—';
    }

    // ── Public API (used by inline onclick handlers) ───────────────────────────

    window.app = {
        markEmpty: () => postCalibrate({ action: 'setEmpty' }),
        markFull:  () => postCalibrate({ action: 'setFull' }),
        saveCapacity: () => {
            const v = parseInt(document.getElementById('cal-capacity-input').value, 10);
            if (!v || v < 10 || v > 10000) {
                setStatusLine('cal-status', 'Enter a capacity between 10 and 10 000 L');
                return;
            }
            postCalibrate({ action: 'setCapacity', litres: v });
        },
    };

    // ── Boot ──────────────────────────────────────────────────────────────────

    fetchStatus();
})();
