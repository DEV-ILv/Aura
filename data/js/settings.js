AURA.pages = AURA.pages || {};

AURA.pages.settings = function() {
  var wi = AURA.state.wifi;
  var se = AURA.state.settings;

  var pwdBtn = AURA.session && AURA.session.mustChange
    ? '<button class="btn btn-primary btn-sm" onclick="AURA.auth.openChangePassword(true)">Set New Password</button>'
    : '<button class="btn btn-primary btn-sm" onclick="AURA.auth.openChangePassword(false)">Change Password</button>';

  AURA.components.render(
    '<div class="page" id="page-settings">' +
    '<div class="page-h1">Settings</div>' +
    '<div class="page-sub">Device configuration</div>' +
    '<form id="wifi-form" onsubmit="AURA.saveWifi(event)">' +
    '<div class="sect"><div class="sect-h">Wi-Fi Configuration</div>' +
    '<div class="crd">' +
    '<div class="frm-grp"><label for="s-ssid">SSID</label><input type="text" id="s-ssid" name="ssid" placeholder="Enter Wi-Fi SSID"' + (wi ? ' value="' + wi.ssid + '"' : '') + '></div>' +
    '<div class="frm-grp"><label for="s-pass">Password</label><input type="password" id="s-pass" name="password" placeholder="Enter Wi-Fi password"></div>' +
    '<div class="btn-group"><button type="submit" class="btn btn-primary btn-sm">Save Wi-Fi</button></div>' +
    '</div></div></form>' +
    '<form id="set-form" onsubmit="AURA.saveSettings(event)">' +
    '<div class="sect"><div class="sect-h">Device</div>' +
    '<div class="crd">' +
    '<div class="frm-grp"><label for="s-name">Device Name</label><input type="text" id="s-name" name="device_name" placeholder="AURA" value="' + (se ? se.device_name : 'AURA') + '"></div>' +
    '<div class="btn-group"><button type="submit" class="btn btn-primary btn-sm">Save Settings</button></div>' +
    '</div></div></form>' +
    '<form id="output-form" onsubmit="AURA.saveOutputSettings(event)">' +
    '<div class="sect"><div class="sect-h">Output</div>' +
    '<div class="crd">' +
    '<label style="display:flex;align-items:center;gap:10px;margin:10px 0;font-size:14px;cursor:pointer"><input type="checkbox" id="s-oled" name="output_oled" ' + (se && se.output_oled ? 'checked' : '') + '><span>OLED Display</span></label>' +
    '<label style="display:flex;align-items:center;gap:10px;margin:10px 0;font-size:14px;cursor:pointer"><input type="checkbox" id="s-speaker" name="output_speaker" ' + (se && se.output_speaker ? 'checked' : '') + '><span>Speaker</span></label>' +
    '<label style="display:flex;align-items:center;gap:10px;margin:10px 0;font-size:14px;cursor:pointer"><input type="checkbox" id="s-companion" name="output_companion" ' + (se && se.output_companion ? 'checked' : '') + '><span>Companion App</span></label>' +
    '<label style="display:flex;align-items:center;gap:10px;margin:10px 0;font-size:14px;cursor:not-allowed;opacity:0.5"><input type="checkbox" disabled><span>Push Notifications (future)</span></label>' +
    '<div class="btn-group"><button type="submit" class="btn btn-primary btn-sm">Save Output Settings</button></div>' +
    '</div></div></form>' +
    '<div class="sect"><div class="sect-h">Security</div>' +
    '<div class="crd">' +
    '<p style="font-size:13px;color:var(--text-secondary);margin-bottom:14px">Manage the admin password used to sign in to this web portal.</p>' +
    '<div class="btn-group">' + pwdBtn +
    '<button class="btn btn-ghost btn-sm" onclick="AURA.auth.logout()">Sign Out</button>' +
    '</div></div></div>' +
    '<div class="sect"><div class="sect-h">About</div>' +
    '<div class="crd" style="display:flex;flex-direction:column;gap:8px">' +
    '<div style="display:flex;justify-content:space-between;font-size:13px"><span style="color:var(--text-secondary)">Version</span><span style="font-family:var(--font-mono)">v' + (se ? se.version : '--') + '</span></div>' +
    '<div style="display:flex;justify-content:space-between;font-size:13px"><span style="color:var(--text-secondary)">Build Date</span><span>' + (se ? se.build_date + ' ' + se.build_time : '--') + '</span></div>' +
    '<div style="display:flex;justify-content:space-between;font-size:13px"><span style="color:var(--text-secondary)">Platform</span><span>ESP32-WROOM-32</span></div>' +
    '<div style="display:flex;justify-content:space-between;font-size:13px"><span style="color:var(--text-secondary)">Hostname</span><span style="font-family:var(--font-mono)">' + (wi ? wi.ip : '--') + '</span></div>' +
    '</div></div>' +
    '<div class="sect"><div class="sect-h" style="color:var(--danger)">Danger Zone</div>' +
    '<div class="crd" style="border-color:rgba(255,82,82,0.15)">' +
    '<p style="font-size:13px;color:var(--text-secondary);margin-bottom:14px">Irreversible actions that will affect device operation.</p>' +
    '<div class="btn-group">' +
    '<button class="btn btn-danger btn-sm" onclick="AURA.restartDevice()"><span class="material-symbols-outlined" style="font-size:16px">restart_alt</span>Restart Device</button>' +
    '<button class="btn btn-danger btn-sm" onclick="AURA.factoryResetDevice()"><span class="material-symbols-outlined" style="font-size:16px">delete_forever</span>Factory Reset</button>' +
    '</div></div></div></div>'
  );
  AURA.components.showPage('page-settings');
};

AURA.saveWifi = function(e) {
  e.preventDefault();
  var fd = new FormData(e.target);
  AURA.api.saveWifiForm(fd).then(function(res) {
    AURA.components.toast(
      (res.data && res.data.message) || (res.data && res.data.error) || 'Wi-Fi saved',
      res.ok ? 'success' : 'error'
    );
  }).catch(function() {
    AURA.components.toast('Failed to save Wi-Fi', 'error');
  });
};

AURA.saveSettings = function(e) {
  e.preventDefault();
  var fd = new FormData(e.target);
  AURA.api.saveSettingsForm(fd).then(function(res) {
    AURA.components.toast(
      (res.data && res.data.message) || (res.data && res.data.error) || 'Settings saved',
      res.ok ? 'success' : 'error'
    );
  }).catch(function() {
    AURA.components.toast('Failed to save settings', 'error');
  });
};

AURA.saveOutputSettings = function(e) {
  e.preventDefault();
  AURA.api.postJSON('/api/settings', {
    output_oled: !!document.getElementById('s-oled').checked,
    output_speaker: !!document.getElementById('s-speaker').checked,
    output_companion: !!document.getElementById('s-companion').checked
  }).then(function(res) {
    AURA.components.toast(
      (res.data && res.data.message) || (res.data && res.data.error) || 'Output settings saved',
      res.ok ? 'success' : 'error'
    );
  }).catch(function() {
    AURA.components.toast('Failed to save output settings', 'error');
  });
};

AURA.restartDevice = function() {
  AURA.components.toast('Restarting...', 'info');
  AURA.api.restart().then(function(res) {
    AURA.components.toast((res.data && res.data.message) || 'Restarting...', 'info');
  });
};

AURA.factoryResetDevice = function() {
  if (!confirm('This will erase ALL settings and reset the device. Continue?')) return;
  AURA.api.factoryReset().then(function(res) {
    AURA.components.toast((res.data && res.data.message) || 'Factory reset...', 'error');
  });
};
