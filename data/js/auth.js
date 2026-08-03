AURA.auth = AURA.auth || {};

AURA.auth.renderLogin = function(message) {
  var m = message ? '<div class="auth-msg">' + message + '</div>' : '';
  AURA.$('auth-root').innerHTML =
    '<div class="auth-card">' +
    '<div class="auth-logo"><span class="logo-dot"></span>AURA</div>' +
    '<div class="auth-title">Sign in to AURA</div>' +
    '<div class="auth-sub">Enter the admin credentials shown on the device Serial monitor on first boot.</div>' +
    m +
    '<form id="login-form" onsubmit="AURA.auth.submitLogin(event)">' +
    '<div class="frm-grp"><label for="auth-user">Username</label><input type="text" id="auth-user" name="username" autocomplete="username" required></div>' +
    '<div class="frm-grp"><label for="auth-pass">Password</label><input type="password" id="auth-pass" name="password" autocomplete="current-password" required></div>' +
    '<div class="auth-err" id="auth-err"></div>' +
    '<button type="submit" class="btn btn-primary auth-btn" id="auth-btn">Sign In</button>' +
    '</form></div>';
  AURA.$('auth-root').classList.add('show');
};

AURA.auth.showLogin = function(message) {
  if (!AURA.$('auth-root')) return;
  AURA.auth.renderLogin(message);
};

AURA.auth.hideLogin = function() {
  var el = AURA.$('auth-root');
  if (el) el.classList.remove('show');
};

AURA.auth.submitLogin = function(e) {
  e.preventDefault();
  var err = AURA.$('auth-err');
  if (err) err.textContent = '';
  var btn = AURA.$('auth-btn');
  if (btn) btn.disabled = true;

  AURA.api.login(AURA.$('auth-user').value, AURA.$('auth-pass').value).then(function(res) {
    if (btn) btn.disabled = false;
    if (res.ok && res.data && res.data.token) {
      AURA.session.token = res.data.token;
      AURA.session.authenticated = true;
      AURA.session.mustChange = !!res.data.must_change;
      AURA.session.expiresIn = res.data.expiresIn || 0;
      AURA.session.save();
      AURA.auth.hideLogin();
      AURA.startApp(AURA.session.mustChange);
    } else {
      if (err) err.textContent = (res.data && res.data.error) ? res.data.error : 'Invalid username or password';
      var pass = AURA.$('auth-pass');
      if (pass) pass.value = '';
      if (pass) pass.focus();
    }
  });
};

AURA.auth.openChangePassword = function(required) {
  var modal = AURA.$('pwd-modal');
  if (!modal) return;
  var msg = AURA.$('pwd-msg');
  if (msg) {
    msg.textContent = required
      ? 'Your admin password is temporary. Please set a new one to continue.'
      : 'Update your admin password.';
  }
  modal.classList.add('show');
};

AURA.auth.closeChangePassword = function() {
  var modal = AURA.$('pwd-modal');
  if (modal) modal.classList.remove('show');
};

AURA.auth.submitChangePassword = function(e) {
  e.preventDefault();
  var err = AURA.$('pwd-err');
  if (err) err.textContent = '';

  var cur = AURA.$('pwd-current').value;
  var n1 = AURA.$('pwd-new').value;
  var n2 = AURA.$('pwd-confirm').value;

  if (n1 !== n2) {
    if (err) err.textContent = 'New passwords do not match.';
    return;
  }
  if (n1.length < 8) {
    if (err) err.textContent = 'New password must be at least 8 characters.';
    return;
  }

  AURA.api.changePassword(cur, n1).then(function(res) {
    if (res.ok) {
      AURA.session.mustChange = false;
      AURA.auth.closeChangePassword();
      AURA.components.toast('Password updated', 'success');
    } else {
      if (err) err.textContent = (res.data && res.data.error) ? res.data.error : 'Failed to update password';
    }
  });
};

AURA.auth.logout = function() {
  AURA.api.logout().then(function() {
    AURA.session.clear();
    if (AURA.ws && AURA.ws.close) AURA.ws.close();
    AURA.auth.showLogin();
  });
};
