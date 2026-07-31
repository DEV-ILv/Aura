AURA.api = AURA.api || {};

AURA.api.fetchJSON = async function(url) {
  try {
    const r = await fetch(url);
    return await r.json();
  } catch (e) {
    return null;
  }
};

AURA.api.getStatus = function() {
  return AURA.api.fetchJSON('/api/status');
};

AURA.api.getWifi = function() {
  return AURA.api.fetchJSON('/api/wifi');
};

AURA.api.getSettings = function() {
  return AURA.api.fetchJSON('/api/settings');
};

AURA.api.restart = function() {
  return fetch('/restart', { method: 'POST' }).then(function(r) {
    return r.json();
  });
};

AURA.api.factoryReset = function() {
  return fetch('/factory-reset', { method: 'POST' }).then(function(r) {
    return r.json();
  });
};

AURA.api.saveWifiForm = function(data) {
  return fetch('/wifi', {
    method: 'POST',
    body: new URLSearchParams(data)
  }).then(function(r) {
    return r.json();
  });
};

AURA.api.saveSettingsForm = function(data) {
  return fetch('/settings', {
    method: 'POST',
    body: new URLSearchParams(data)
  }).then(function(r) {
    return r.json();
  });
};
