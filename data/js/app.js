AURA.state = {
  status: null,
  wifi: null,
  settings: null
};

AURA.pages = AURA.pages || {};
AURA.refreshTimer = null;

AURA.upNav = function() {
  var st = AURA.state.status;
  var wi = AURA.state.wifi;
  var se = AURA.state.settings;

  if (!st && !wi) return;

  var dot = AURA.$('top-dot');
  var conn = AURA.$('top-conn');

  if (wi && wi.connected === true) {
    if (dot) dot.className = 'conn-dot on';
    if (conn) conn.textContent = wi.ssid || 'Connected';
  } else {
    if (dot) dot.className = 'conn-dot off';
    if (conn) conn.textContent = 'Disconnected';
  }

  if (se && se.version && AURA.$('top-ver')) {
    AURA.$('top-ver').textContent = 'v' + se.version;
  }
};

AURA.refresh = function() {
  if (!AURA.session || !AURA.session.authenticated) return;
  Promise.all([
    AURA.api.getStatus(),
    AURA.api.getWifi(),
    AURA.api.getSettings()
  ]).then(function(results) {
    if (results[0]) AURA.state.status = results[0];
    if (results[1]) AURA.state.wifi = results[1];
    if (results[2]) AURA.state.settings = results[2];
    AURA.upNav();
    AURA.upDB();
  });
};

AURA.router = function() {
  var hash = location.hash.slice(1) || 'dashboard';
  var fn = AURA.pages[hash];

  if (!fn) {
    AURA.components.render(
      '<div class="page active" id="page-404">' +
      '<div class="page-h1">404</div>' +
      '<div class="page-sub">Page not found</div>' +
      '<div class="cta-placeholder"><span class="material-symbols-outlined">search_off</span><h3>Page Not Found</h3>' +
      '<p>The page you are looking for does not exist. <a href="#dashboard" style="color:var(--accent);text-decoration:none">Return to Dashboard</a></p></div></div>'
    );
    AURA.components.showPage('page-404');
    return;
  }

  AURA.$$('.sidebar a').forEach(function(a) {
    a.classList.toggle('active', a.dataset.page === hash);
  });

  fn();
};

AURA.initClock = function() {
  function update() {
    var el = AURA.$('top-tm');
    if (el) {
      el.textContent = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    }
  }
  update();
  setInterval(update, 10000);
};

// Called after a valid session is established (either fresh login or a token
// restored from sessionStorage and confirmed by /api/auth/status).
AURA.startApp = function(mustChange) {
  AURA.router();
  AURA.anim.init();
  AURA.ws.connect();
  AURA.refresh();
  AURA.refreshTimer = setInterval(AURA.refresh, 5000);
  if (mustChange) AURA.auth.openChangePassword(true);
};

AURA.init = function() {
  if (!location.hash) {
    var m = {
      '/': 'dashboard',
      '/status': 'dashboard',
      '/wifi': 'settings',
      '/settings': 'settings',
      '/ota': 'ota',
      '/chat': 'chat',
      '/memory': 'memory',
      '/reminders': 'reminders',
      '/logs': 'logs'
    };
    location.hash = m[location.pathname] || 'dashboard';
  }

  AURA.initClock();
  AURA.session.restore();

  // Verify the (possibly restored) session token against the device.
  AURA.api.authStatus().then(function(st) {
    if (st && st.authenticated) {
      AURA.session.authenticated = true;
      AURA.session.mustChange = !!st.must_change;
      AURA.session.expiresIn = st.expiresIn || 0;
      AURA.startApp(AURA.session.mustChange);
    } else {
      AURA.session.authenticated = false;
      AURA.auth.showLogin();
    }
  });
};

document.addEventListener('DOMContentLoaded', AURA.init);
window.addEventListener('hashchange', AURA.router);
