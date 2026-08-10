AURA.pages = AURA.pages || {};

AURA.pages.dashboard = function() {
  var st = AURA.state.status;
  var wi = AURA.state.wifi;
  var se = AURA.state.settings;

  var upt = st ? AURA.fmtUptime(st.uptime) : '--';
  var heap = st ? AURA.fmtBytes(st.heap_free) : '--';
  var req = st ? st.requests : '--';
  var run = st && st.running ? 'Running' : 'Stopped';
  var wc = wi && wi.connected ? 'Connected' : 'Disconnected';
  var ss = wi ? wi.ssid : '--';
  var ip = wi ? wi.ip : '--';
  var sig = wi ? wi.signal : 0;
  var sl = wi ? AURA.fmtSignal(sig) : '--';
  var ver = se ? se.version : '--';
  var bd = se ? se.build_date : '--';

  AURA.components.render(
    '<div class="page active" id="page-dashboard">' +
    '<div class="page-h1">Dashboard</div>' +
    '<div class="page-sub">System overview and quick actions</div>' +
    '<div class="grid grid-4 sect">' +
    '<div class="crd acc-cyan"><div class="crd-h"><div class="crd-lbl">Uptime</div><div class="crd-icon"><span class="material-symbols-outlined">schedule</span></div></div><div class="crd-val" id="db-uptime">' + upt + '</div></div>' +
    '<div class="crd acc-green"><div class="crd-h"><div class="crd-lbl">Free Heap</div><div class="crd-icon"><span class="material-symbols-outlined">memory</span></div></div><div class="crd-val" id="db-heap">' + heap + '</div></div>' +
    '<div class="crd acc-blue"><div class="crd-h"><div class="crd-lbl">Status</div><div class="crd-icon"><span class="material-symbols-outlined">check_circle</span></div></div><div class="crd-val" style="font-size:20px" id="db-status">' + run + '</div></div>' +
    '<div class="crd acc-purple"><div class="crd-h"><div class="crd-lbl">Requests</div><div class="crd-icon"><span class="material-symbols-outlined">http</span></div></div><div class="crd-val" id="db-req">' + req + '</div></div>' +
    '</div>' +
    '<div class="grid grid-3 sect">' +
    '<div class="crd acc-blue"><div class="crd-h"><div class="crd-lbl">Wi-Fi</div><div class="crd-icon"><span class="material-symbols-outlined">wifi</span></div></div><div class="crd-val" style="font-size:18px" id="db-wifi">' + wc + '</div><div class="crd-sub" id="db-ssid">' + ss + '</div></div>' +
    '<div class="crd acc-cyan"><div class="crd-h"><div class="crd-lbl">IP Address</div><div class="crd-icon"><span class="material-symbols-outlined">lan</span></div></div><div class="crd-val" style="font-size:18px" id="db-ip">' + (ip || '--') + '</div></div>' +
    '<div class="crd acc-yellow"><div class="crd-h"><div class="crd-lbl">Signal</div><div class="crd-icon"><span class="material-symbols-outlined">signal_cellular_alt</span></div></div><div class="crd-val" style="font-size:18px" id="db-signal">' + sig + ' dBm</div><div class="crd-sub" id="db-signal-lbl">' + sl + '</div></div>' +
    '</div>' +
    '<div class="sect"><div class="sect-h">Quick Actions</div>' +
    '<div class="actions">' +
    '<a href="#chat" class="act"><span class="material-symbols-outlined">chat</span>Chat</a>' +
    '<a href="#memory" class="act"><span class="material-symbols-outlined">memory</span>Memory</a>' +
    '<a href="#reminders" class="act"><span class="material-symbols-outlined">notifications</span>Reminders</a>' +
    '<a href="#ota" class="act"><span class="material-symbols-outlined">system_update</span>OTA</a>' +
    '<a href="#settings" class="act"><span class="material-symbols-outlined">settings</span>Settings</a>' +
    '<a href="#logs" class="act"><span class="material-symbols-outlined">article</span>Logs</a>' +
    '<a href="#errors" class="act"><span class="material-symbols-outlined">report</span>Errors</a>' +
    '<button class="act" onclick="AURA.restartDevice()"><span class="material-symbols-outlined">restart_alt</span>Restart</button>' +
    '<button class="act danger" onclick="AURA.factoryResetDevice()"><span class="material-symbols-outlined">dangerous</span>Factory Reset</button>' +
    '</div></div>' +
    '<div class="sect"><div class="sect-h">Firmware</div>' +
    '<div class="crd"><div class="crd-h"><div class="crd-lbl">Version</div><span style="font-family:var(--font-mono);font-size:14px;color:var(--text-secondary)" id="db-version">v' + ver + '</span></div><div style="font-size:12px;color:var(--text-muted)" id="db-build">Built: ' + (bd || '--') + '</div></div>' +
    '</div></div>'
  );
  AURA.components.showPage('page-dashboard');
};

AURA.upDB = function() {
  var e = AURA.$('db-uptime');
  if (!e) return;
  var st = AURA.state.status;
  var wi = AURA.state.wifi;
  var se = AURA.state.settings;

  if (st) {
    AURA.$('db-uptime').textContent = AURA.fmtUptime(st.uptime);
    AURA.$('db-heap').textContent = AURA.fmtBytes(st.heap_free);
    AURA.$('db-status').textContent = st.running ? 'Running' : 'Stopped';
    AURA.$('db-req').textContent = st.requests;
  }
  if (wi) {
    AURA.$('db-wifi').textContent = wi.connected ? 'Connected' : 'Disconnected';
    AURA.$('db-ssid').textContent = wi.ssid || '--';
    AURA.$('db-ip').textContent = wi.ip || '--';
    AURA.$('db-signal').textContent = wi.signal + ' dBm';
    if (AURA.$('db-signal-lbl')) AURA.$('db-signal-lbl').textContent = AURA.fmtSignal(wi.signal);
  }
  if (se) {
    if (AURA.$('db-version')) AURA.$('db-version').textContent = 'v' + se.version;
    if (AURA.$('db-build')) AURA.$('db-build').textContent = 'Built: ' + (se.build_date || '--');
  }
};
