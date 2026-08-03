AURA.api = AURA.api || {};

// Session state. The token lives in memory and (per tab) in sessionStorage so
// a page refresh keeps the session, but nothing is written to localStorage.
AURA.session = AURA.session || {
  token: null,
  authenticated: false,
  mustChange: false,
  expiresIn: 0
};

AURA.session.save = function() {
  try {
    if (AURA.session.token) {
      sessionStorage.setItem('aura_token', AURA.session.token);
    } else {
      sessionStorage.removeItem('aura_token');
    }
  } catch (e) {}
};

AURA.session.restore = function() {
  try {
    var t = sessionStorage.getItem('aura_token');
    if (t) AURA.session.token = t;
  } catch (e) {}
};

AURA.session.clear = function() {
  AURA.session.token = null;
  AURA.session.authenticated = false;
  AURA.session.mustChange = false;
  AURA.session.expiresIn = 0;
  AURA.session.save();
};

AURA.session.handleExpired = function() {
  var wasAuth = AURA.session.authenticated;
  AURA.session.clear();
  if (wasAuth && AURA.ws && AURA.ws.close) AURA.ws.close();
  if (AURA.auth && AURA.auth.showLogin) {
    AURA.auth.showLogin(wasAuth ? 'Session expired. Please sign in again.' : 'Please sign in.');
  }
};

// Attach the X-Auth-Token header to every request. The firmware rejects API
// calls that do not carry a valid session token.
AURA.api.headers = function(extra) {
  var h = extra || {};
  if (AURA.session.token) h['X-Auth-Token'] = AURA.session.token;
  return h;
};

AURA.api.request = async function(url, opts) {
  opts = opts || {};
  opts.headers = AURA.api.headers(opts.headers || {});
  var r;
  try {
    r = await fetch(url, opts);
  } catch (e) {
    return { ok: false, status: 0, data: null };
  }

  // A 401 from a non-login endpoint means the session is gone (expired,
  // invalidated by logout elsewhere, or rebooted). Do not treat a failed
  // login attempt (also 401) as an expired session.
  if (r.status === 401 && url.indexOf('/api/auth/login') === -1 && url.indexOf('/api/auth/change-password') === -1) {
    AURA.session.handleExpired();
  }

  var data = null;
  try { data = await r.json(); } catch (e) { data = null; }
  return { ok: r.ok, status: r.status, data: data };
};

AURA.api.postJSON = function(url, body) {
  return AURA.api.request(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  });
};

AURA.api.fetchJSON = function(url) {
  return AURA.api.request(url).then(function(res) { return res.data; });
};

// Auth
AURA.api.login = function(username, password) {
  return AURA.api.postJSON('/api/auth/login', { username: username, password: password });
};
AURA.api.logout = function() {
  return AURA.api.request('/api/auth/logout', { method: 'POST' });
};
AURA.api.changePassword = function(current, next) {
  return AURA.api.postJSON('/api/auth/change-password', { current_password: current, new_password: next });
};
AURA.api.authStatus = function() {
  return AURA.api.fetchJSON('/api/auth/status');
};

// Device data
AURA.api.getStatus = function() {
  return AURA.api.fetchJSON('/api/status');
};
AURA.api.getWifi = function() {
  return AURA.api.fetchJSON('/api/wifi');
};
AURA.api.getSettings = function() {
  return AURA.api.fetchJSON('/api/settings');
};

// Actions
AURA.api.restart = function() {
  return AURA.api.request('/restart', { method: 'POST' });
};
AURA.api.factoryReset = function() {
  return AURA.api.request('/factory-reset', { method: 'POST' });
};
AURA.api.saveWifiForm = function(data) {
  return AURA.api.request('/wifi', { method: 'POST', body: new URLSearchParams(data) });
};
AURA.api.saveSettingsForm = function(data) {
  return AURA.api.request('/settings', { method: 'POST', body: new URLSearchParams(data) });
};
